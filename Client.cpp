/* 
 * Copyright (C) 2001-2015 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"
#include "Client.h"

#include "AirUtil.h"
#include "BufferedSocket.h"
#include "ClientManager.h"
#include "ConnectivityManager.h"
#include "DebugManager.h"
#include "FavoriteManager.h"
#include "LogManager.h"
#include "MessageManager.h"
#include "ResourceManager.h"
#include "ThrottleManager.h"
#include "TimerManager.h"

namespace dcpp {

atomic<long> Client::counts[COUNT_UNCOUNTED];
ClientToken idCounter = 0;

Client::Client(const string& hubURL, char separator_, const ClientPtr& aOldClient) :
	myIdentity(ClientManager::getInstance()->getMe(), 0), clientId(aOldClient ? aOldClient->getClientId() : ++idCounter),
	reconnDelay(120), lastActivity(GET_TICK()), registered(false), autoReconnect(false),
	state(STATE_DISCONNECTED), sock(0),
	separator(separator_),
	countType(COUNT_UNCOUNTED), availableBytes(0), favToken(0), iskeypError(false), cache(aOldClient ? aOldClient->getCache() : SettingsManager::HUB_MESSAGE_CACHE)
{
	setHubUrl(hubURL);
	TimerManager::getInstance()->addListener(this);
}

void Client::setHubUrl(const string& aUrl) {
	hubUrl = aUrl;
	secure = Util::strnicmp("adcs://", aUrl.c_str(), 7) == 0 || Util::strnicmp("nmdcs://", aUrl.c_str(), 8) == 0;

	string file, proto, query, fragment;
	Util::decodeUrl(hubUrl, proto, address, port, file, query, fragment);
	keyprint = Util::decodeQuery(query)["kp"];
}

Client::~Client() {
	dcdebug("Client %s was deleted\n", hubUrl.c_str());
}

void Client::reconnect() {
	disconnect(true);
	setAutoReconnect(true);
	setReconnDelay(0);
}

void Client::setActive() {
	fire(ClientListener::SetActive(), this);
}

void Client::shutdown(ClientPtr& aClient, bool aRedirect) {
	FavoriteManager::getInstance()->removeUserCommand(getHubUrl());
	TimerManager::getInstance()->removeListener(this);

	if (!aRedirect) {
		fire(ClientListener::Disconnecting(), this);
	}

	if(sock) {
		BufferedSocket::putSocket(sock, [=] { // Ensure that the pointer won't be deleted too early
			state = STATE_DISCONNECTED;
			if (!aRedirect) {
				cache.clear();
			}

			aClient->clearUsers();
			updateCounts(true);
		});
	}
}

string Client::getDescription() const {
	string ret = get(HubSettings::Description);

	int upLimit = ThrottleManager::getInstance()->getUpLimit();
	if(upLimit > 0)
		ret = "[L:" + Util::toString(upLimit) + "KB] " + ret;
	return ret;
}

void Client::reloadSettings(bool updateNick) {
	/// @todo update the nick in ADC hubs?
	string prevNick;
	if(!updateNick)
		prevNick = get(Nick);

	auto fav = FavoriteManager::getInstance()->getFavoriteHubEntry(getHubUrl());

	*static_cast<HubSettings*>(this) = SettingsManager::getInstance()->getHubSettings();

	bool isAdcHub = AirUtil::isAdcHub(hubUrl);

	if(fav) {
		FavoriteManager::getInstance()->mergeHubSettings(fav, *this);
		if(!fav->getPassword().empty())
			setPassword(fav->getPassword());

		setStealth(!isAdcHub ? fav->getStealth() : false);
		setFavNoPM(fav->getFavNoPM());

		favToken = fav->getToken();
	} else {
		setStealth(false);
		setFavNoPM(false);
		setPassword(Util::emptyString);
	}

	searchQueue.minInterval = get(HubSettings::SearchInterval) * 1000; //convert from seconds
	if (updateNick)
		checkNick(get(Nick));
	else
		get(Nick) = prevNick;
}

bool Client::changeBoolHubSetting(HubSettings::HubBoolSetting aSetting) {
	auto newValue = !get(aSetting);
	get(aSetting) = newValue;

	//save for a favorite hub if needed
	if (favToken > 0) {
		FavoriteManager::getInstance()->setHubSetting(hubUrl, aSetting, newValue);
	}
	return newValue;
}

void Client::updated(const OnlineUserPtr& aUser) {
	fire(ClientListener::UserUpdated(), this, aUser);
}

void Client::updated(OnlineUserList& users) {
	//std::for_each(users.begin(), users.end(), [](OnlineUser* user) { UserMatchManager::getInstance()->match(*user); });

	fire(ClientListener::UsersUpdated(), this, users);
}

const string& Client::getUserIp4() const {
	if (!get(UserIp).empty()) {
		return get(UserIp);
	}
	return CONNSETTING(EXTERNAL_IP);
}

const string& Client::getUserIp6() const {
	if (!get(UserIp6).empty()) {
		return get(UserIp6);
	}
	return CONNSETTING(EXTERNAL_IP6);
}

bool Client::isActive() const {
	return isActiveV4() || isActiveV6();
}

bool Client::isActiveV4() const {
	return get(HubSettings::Connection) != SettingsManager::INCOMING_PASSIVE && get(HubSettings::Connection) != SettingsManager::INCOMING_DISABLED;
}

bool Client::isActiveV6() const {
	return !v4only() && get(HubSettings::Connection6) != SettingsManager::INCOMING_PASSIVE && get(HubSettings::Connection6) != SettingsManager::INCOMING_DISABLED;
}

void Client::connect() {
	if (sock) {
		BufferedSocket::putSocket(sock);
		sock = 0;
	}

	redirectUrl = Util::emptyString;
	setAutoReconnect(true);
	setReconnDelay(120 + Util::rand(0, 60));
	reloadSettings(true);
	setRegistered(false);
	setMyIdentity(Identity(ClientManager::getInstance()->getMe(), 0));
	setHubIdentity(Identity());

	setConnectState(STATE_CONNECTING);

	try {
		sock = BufferedSocket::getSocket(separator, v4only());
		sock->addListener(this);
		sock->connect(Socket::AddressInfo(address, Socket::AddressInfo::TYPE_URL), port, secure, SETTING(ALLOW_UNTRUSTED_HUBS), true, keyprint /**/);
	} catch (const Exception& e) {
		setConnectState(STATE_DISCONNECTED);
		fire(ClientListener::Failed(), hubUrl, e.getError());
	}
	updateActivity();
}

void Client::info() {
	callAsync([this] { infoImpl(); });
}

void Client::send(const char* aMessage, size_t aLen) {
	if (!isConnected() || !sock) {
		dcassert(0);
		return;
	}
	updateActivity();
	sock->write(aMessage, aLen);
	COMMAND_DEBUG(aMessage, DebugManager::TYPE_HUB, DebugManager::OUTGOING, getIpPort());
}

void Client::on(BufferedSocketListener::Connected) noexcept {
	statusMessage(STRING(CONNECTED), LogMessage::SEV_INFO);

	updateActivity();
	ip = sock->getIp();
	localIp = sock->getLocalIp();
	
	fire(ClientListener::Connected(), this);
	setConnectState(STATE_PROTOCOL);
}

void Client::setConnectState(State aState) noexcept {
	if (state == aState) {
		return;
	}

	state = aState;
	fire(ClientListener::ConnectStateChanged(), this, aState);
}

void Client::statusMessage(const string& aMessage, LogMessage::Severity aSeverity, int aFlag) noexcept {
	auto message = make_shared<LogMessage>(aMessage, aSeverity);

	if (aFlag != ClientListener::FLAG_IS_SPAM) {
		cache.addMessage(message);

		if (SETTING(LOG_STATUS_MESSAGES)) {
			ParamMap params;
			getHubIdentity().getParams(params, "hub", false);
			params["hubURL"] = getHubUrl();
			getMyIdentity().getParams(params, "my", true);
			params["message"] = aMessage;
			LOG(LogManager::STATUS, params);
		}
	}

	fire(ClientListener::StatusMessage(), this, message, aFlag);
}

void Client::setRead() noexcept {
	auto updated = cache.setRead();
	if (updated > 0) {
		fire(ClientListener::MessagesRead(), this);
	}
}

int Client::clearCache() noexcept {
	auto ret = cache.clear();
	if (ret > 0) {
		fire(ClientListener::MessagesCleared(), this);
	}

	return ret;
}

void Client::onPassword() {
	setConnectState(STATE_VERIFY);
	if (!defpassword.empty()) {
		password(defpassword);
		statusMessage(STRING(STORED_PASSWORD_SENT), LogMessage::SEV_INFO);
	} else {
		fire(ClientListener::GetPassword(), this);
	}
}

void Client::onRedirect(const string& aRedirectUrl) noexcept {
	if (ClientManager::getInstance()->hasClient(aRedirectUrl)) {
		statusMessage(STRING(REDIRECT_ALREADY_CONNECTED), LogMessage::SEV_INFO);
		return;
	}

	redirectUrl = aRedirectUrl;

	if (SETTING(AUTO_FOLLOW)) {
		doRedirect();
	} else {
		fire(ClientListener::Redirect(), this, redirectUrl);
	}
}

ProfileToken Client::getShareProfile() const noexcept {
	if (favToken > 0) {
		return get(HubSettings::ShareProfile);
	}

	return customShareProfile;
}

void Client::allowUntrustedConnect() noexcept {
	if (isConnected() || !iskeypError)
		return;
	keyprint = Util::emptyString;
	connect();
}

void Client::onChatMessage(const ChatMessagePtr& aMessage) noexcept {
	if (MessageManager::getInstance()->isIgnoredOrFiltered(aMessage, this, false))
		return;

	if (get(HubSettings::LogMainChat)) {
		ParamMap params;
		params["message"] = aMessage->format();
		getHubIdentity().getParams(params, "hub", false);
		params["hubURL"] = getHubUrl();
		getMyIdentity().getParams(params, "my", true);
		LOG(LogManager::CHAT, params);
	}

	cache.addMessage(aMessage);

	fire(ClientListener::ChatMessage(), this, aMessage);
}

void Client::on(BufferedSocketListener::Connecting) noexcept {
	statusMessage(STRING(CONNECTING_TO) + " " + getHubUrl() + " ...", LogMessage::SEV_INFO);
	fire(ClientListener::Connecting(), this);
}

bool Client::saveFavorite() {
	FavoriteHubEntryPtr e = new FavoriteHubEntry();
	e->setServer(getHubUrl());
	e->setName(getHubName());
	e->setDescription(getHubDescription());
	e->setAutoConnect(true);
	if (!defpassword.empty()) {
		e->setPassword(defpassword);
	}

	return FavoriteManager::getInstance()->addFavoriteHub(e);
}

void Client::doRedirect() noexcept {
	if (redirectUrl.empty()) {
		return;
	}

	if (ClientManager::getInstance()->hasClient(redirectUrl)) {
		statusMessage(STRING(REDIRECT_ALREADY_CONNECTED), LogMessage::SEV_INFO);
		return;
	}

	auto newClient = ClientManager::getInstance()->redirect(getHubUrl(), redirectUrl);
	fire(ClientListener::Redirected(), getHubUrl(), newClient);
}

void Client::on(Failed, const string& aLine) noexcept {
	clearUsers();
	
	if(stateNormal())
		FavoriteManager::getInstance()->removeUserCommand(hubUrl);

	//Better ways to transfer the text in here?...
	string aError = aLine;
	if (secure && SETTING(ALLOW_UNTRUSTED_HUBS) && sock && !sock->isKeyprintMatch()) {
		aError += ", type /allow to proceed with untrusted connection";
		iskeypError = true;
	}

	setConnectState(STATE_DISCONNECTED);
	statusMessage(aError, LogMessage::SEV_WARNING); //Error?

	sock->removeListener(this);
	fire(ClientListener::Failed(), getHubUrl(), aError);
}

void Client::disconnect(bool graceLess) {
	if(sock) 
		sock->disconnect(graceLess);
}

bool Client::isConnected() const {
	State s = state;
	return s != STATE_CONNECTING && s != STATE_DISCONNECTED; 
}

bool Client::isSecure() const {
	return isConnected() && sock->isSecure();
}

bool Client::isTrusted() const {
	return isConnected() && sock->isTrusted();
}

std::string Client::getEncryptionInfo() const {
	return isConnected() ? sock->getEncryptionInfo() : Util::emptyString;
}

vector<uint8_t> Client::getKeyprint() const {
	return isConnected() ? sock->getKeyprint() : vector<uint8_t>();
}

bool Client::updateCounts(bool aRemove) {
	// We always remove the count and then add the correct one if requested...
	if(countType != COUNT_UNCOUNTED) {
		counts[countType]--;
		countType = COUNT_UNCOUNTED;
	}

	if(!aRemove) {
		if(getMyIdentity().isOp()) {
			countType = COUNT_OP;
		} else if(getMyIdentity().isRegistered()) {
			countType = COUNT_REGISTERED;
		} else {
				//disconnect before the hubcount is updated.
			if(SETTING(DISALLOW_CONNECTION_TO_PASSED_HUBS)) {
				fire(ClientListener::AddLine(), this, STRING(HUB_NOT_PROTECTED));
				disconnect(true);
				setAutoReconnect(false);
				return false;
			}

			countType = COUNT_NORMAL;
		}

		counts[countType]++;
	}
	return true;
}

uint64_t Client::queueSearch(SearchPtr aSearch){
	dcdebug("Queue search %s\n", aSearch->query.c_str());
	return searchQueue.add(move(aSearch));
}

string Client::getCounts() {
	char buf[128];
	return string(buf, snprintf(buf, sizeof(buf), "%ld/%ld/%ld",
		counts[COUNT_NORMAL].load(), counts[COUNT_REGISTERED].load(), counts[COUNT_OP].load()));
}
 
void Client::on(Line, const string& aLine) noexcept {
	updateActivity();
	COMMAND_DEBUG(aLine, DebugManager::TYPE_HUB, DebugManager::INCOMING, getIpPort());
}

void Client::on(Second, uint64_t aTick) noexcept{
	if (state == STATE_DISCONNECTED && getAutoReconnect() && (aTick > (getLastActivity() + getReconnDelay() * 1000))) {
		// Try to reconnect...
		connect();
	}

	if (searchQueue.hasWaitingTime(aTick)) return;

	if (isConnected()){
		auto s = move(searchQueue.pop());
		if (s){
			search(move(s));
		}
	}
}

}