/*
* Copyright (C) 2011-2015 AirDC++ Project
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

#include "PrivateChat.h"

#include "Message.h"
#include "ConnectionManager.h"
#include "LogManager.h"

namespace dcpp 
{

PrivateChat::PrivateChat(const HintedUser& aUser, UserConnection* aUc) :
	uc(aUc), replyTo(aUser), ccpmAttempts(0), allowAutoCCPM(true), lastCCPMAttempt(0), ccpmState(DISCONNECTED),
	online(aUser.user->isOnline()), hubName(ClientManager::getInstance()->getHubName(aUser.hint)), cache(SettingsManager::PM_MESSAGE_CACHE) {
		
	string _err = Util::emptyString;
	supportsCCPM = ClientManager::getInstance()->getSupportsCCPM(aUser.user, _err);
	lastCCPMError = _err;

	if (aUc) {
		ccpmState = CONNECTED;
		aUc->addListener(this);
	} else {
		delayEvents.addEvent(CCPM_AUTO, [this] { checkAlwaysCCPM(); }, 1000);
	}

	ClientManager::getInstance()->addListener(this);
}

PrivateChat::~PrivateChat() {
	ClientManager::getInstance()->removeListener(this);
	if (uc)
		uc->removeListener(this);
}

const string& PrivateChat::ccpmStateToString(uint8_t aState) noexcept {
	switch (aState) {
	case CONNECTING: return STRING(CONNECTING);
	case CONNECTED: return STRING(CONNECTED);
	case DISCONNECTED: return STRING(DISCONNECTED);
	}

	return Util::emptyString;
}


void PrivateChat::CCPMConnected(UserConnection* aUc) {
	ccpmState = CONNECTED;
	setUc(aUc);
	aUc->addListener(this);
	statusMessage(STRING(CCPM_ESTABLISHED), LogMessage::SEV_INFO);
	fire(PrivateChatListener::CCPMStatusUpdated(), this);
}

void PrivateChat::CCPMDisconnected() {
	if (ccReady()) {
		ccpmState = DISCONNECTED;
		uc->removeListener(this);
		setUc(nullptr);
		statusMessage(STRING(CCPM_DISCONNECTED), LogMessage::SEV_INFO);
		fire(PrivateChatListener::CCPMStatusUpdated(), this);
		delayEvents.addEvent(CCPM_AUTO, [this] { checkAlwaysCCPM(); }, 1000);
	}
}

bool PrivateChat::sendMessage(const string& msg, string& error_, bool thirdPerson) {
	if (ccReady()) {
		uc->pm(msg, thirdPerson);
		return true;
	}

	return ClientManager::getInstance()->privateMessage(replyTo, msg, error_, thirdPerson);
}

void PrivateChat::closeCC(bool now, bool noAutoConnect) {
	if (ccReady()) {
		if (noAutoConnect) {
			sendPMInfo(NO_AUTOCONNECT);
			allowAutoCCPM = false;
		}
		//Don't disconnect graceless so the last command can be transferred successfully.
		uc->disconnect(now && !noAutoConnect);
		if (now) {
			ccpmState = DISCONNECTED;
			uc->removeListener(this);
			setUc(nullptr);
		}
	}
}

void PrivateChat::handleMessage(const ChatMessagePtr& aMessage) {
	if (aMessage->getReplyTo()->getHubUrl() != replyTo.hint) {
		if (!ccReady()) {
			statusMessage(STRING_F(MESSAGES_SENT_THROUGH_REMOTE,
				ClientManager::getInstance()->getHubName(aMessage->getReplyTo()->getHubUrl())), LogMessage::SEV_INFO);
		}

		setHubUrl(aMessage->getReplyTo()->getHubUrl());
		fire(PrivateChatListener::UserUpdated(), this);
	}

	cache.addMessage(aMessage);
	fire(PrivateChatListener::PrivateMessage(), this, aMessage);
}

void PrivateChat::setRead() noexcept {
	auto updated = cache.setRead();
	if (updated > 0) {
		fire(PrivateChatListener::MessagesRead(), this);
	}
}

int PrivateChat::clearCache() noexcept {
	auto ret = cache.clear();
	if (ret > 0) {
		fire(PrivateChatListener::MessagesCleared(), this);
	}

	return ret;
}

void PrivateChat::statusMessage(const string& aMessage, LogMessage::Severity aSeverity) noexcept {
	auto message = make_shared<LogMessage>(aMessage, aSeverity);

	fire(PrivateChatListener::StatusMessage(), this, message);
	cache.addMessage(message);
}

void PrivateChat::close() {
	fire(PrivateChatListener::Close(), this);

	//PM window closed, signal it if the user supports CPMI
	if (ccReady() && uc) {
		if (uc->isSet(UserConnection::FLAG_CPMI))
			sendPMInfo(QUIT);
		else
			closeCC(true, false);
	}

	LogManager::getInstance()->removePmCache(getUser());
}

void PrivateChat::startCC() {
	bool protocolError;
	if (!replyTo.user->isOnline() || ccpmState < DISCONNECTED) {
		return;
	}

	ccpmState = CONNECTING;
	lastCCPMError = Util::emptyString;

	auto token = ConnectionManager::getInstance()->tokens.getToken(CONNECTION_TYPE_PM);
	bool connecting = ClientManager::getInstance()->connect(replyTo.user, token, true, lastCCPMError, replyTo.hint, protocolError, CONNECTION_TYPE_PM);
	allowAutoCCPM = !protocolError;

	if (!connecting) {
		ccpmState = DISCONNECTED;
		if (!lastCCPMError.empty()) {
			statusMessage(lastCCPMError, LogMessage::SEV_ERROR);
		}
	} else {
		statusMessage(STRING(CCPM_ESTABLISHING), LogMessage::SEV_INFO);
		fire(PrivateChatListener::CCPMStatusUpdated(), this);
		delayEvents.addEvent(CCPM_TIMEOUT, [this] { checkCCPMTimeout(); }, 30000); // 30 seconds, completely arbitrary amount of time.
	}
	
}

void PrivateChat::checkAlwaysCCPM() {
	if (!replyTo.user->isOnline() || !SETTING(ALWAYS_CCPM) || !getSupportsCCPM() || replyTo.user->isNMDC() || replyTo.user->isSet(User::BOT))
		return;

	if (allowAutoCCPM && ccpmState == DISCONNECTED) {
		startCC();
		allowAutoCCPM = allowAutoCCPM && ccpmAttempts++ < 3;
	}else if (ccReady()){
		allowAutoCCPM = true;
	}
}

void PrivateChat::checkCCPMTimeout() {
	if (ccpmState == CONNECTING) {
		statusMessage(STRING(CCPM_TIMEOUT), LogMessage::SEV_INFO);
		fire(PrivateChatListener::CCPMStatusUpdated(), this);
		ccpmState = DISCONNECTED;
	} 
}

void PrivateChat::on(ClientManagerListener::UserDisconnected, const UserPtr& aUser, bool wentOffline) noexcept{
	if (aUser != replyTo.user)
		return;

	setSupportsCCPM(ClientManager::getInstance()->getSupportsCCPM(replyTo, lastCCPMError));
	if (wentOffline) {
		delayEvents.removeEvent(USER_UPDATE);
		if (ccpmState == CONNECTING) {
			delayEvents.removeEvent(CCPM_TIMEOUT);
			ccpmState = DISCONNECTED;
		}

		closeCC(true, false);
		allowAutoCCPM = true;
		online = false;
		fire(PrivateChatListener::UserUpdated(), this);
		statusMessage(STRING(USER_WENT_OFFLINE), LogMessage::SEV_INFO);
	} else {
		delayEvents.addEvent(USER_UPDATE, [this] {
			checkUserHub(true);
			fire(PrivateChatListener::UserUpdated(), this);
		}, 1000);
	}
}

/*
The hub window was closed, we might be using the client of it for messages(CCPM and status messages) and its soon to be deleted..
This listener comes from the main thread so we should be able to pass the next speaker message before any other messages.
*/
void PrivateChat::on(ClientManagerListener::ClientDisconnected, const string& aHubUrl) noexcept {
	if (aHubUrl == getHubUrl()) {
		checkUserHub(true);
		fire(PrivateChatListener::UserUpdated(), this);
	}
}


void PrivateChat::checkUserHub(bool wentOffline) {
	auto hubs = ClientManager::getInstance()->getHubs(replyTo.user->getCID());
	if (hubs.empty())
		return;

	if (find_if(hubs.begin(), hubs.end(), CompareFirst<string, string>(replyTo.hint)) == hubs.end()) {
		if (!ccReady()) {
			auto statusText = wentOffline ? STRING_F(USER_OFFLINE_PM_CHANGE, hubName % hubs[0].second) :
				STRING_F(MESSAGES_SENT_THROUGH, hubs[0].second);

			statusMessage(statusText, LogMessage::SEV_INFO);
		}

		setHubUrl(hubs[0].first);
		hubName = hubs[0].second;
	}
}

void PrivateChat::setHubUrl(const string& hint) { 
	replyTo.hint = hint;
	hubName = ClientManager::getInstance()->getHubName(replyTo.hint);
}

void PrivateChat::sendPMInfo(uint8_t aType) {
	if (ccReady() && uc && uc->isSet(UserConnection::FLAG_CPMI)) {
		AdcCommand c(AdcCommand::CMD_PMI);
		switch (aType) {
		case MSG_SEEN:
			c.addParam("SN", "1");
			break;
		case TYPING_ON:
			c.addParam("TP", "1");
			break;
		case TYPING_OFF:
			c.addParam("TP", "0");
			break;
		case NO_AUTOCONNECT:
			c.addParam("AC", "0");
			break;
		case QUIT:
			c.addParam("QU", "1");
			break;
		default:
			c.addParam("\n");
		}

		uc->send(c);
	}
}

void PrivateChat::on(AdcCommand::PMI, UserConnection*, const AdcCommand& cmd) noexcept{

	auto type = PMINFO_LAST;
	string tmp;

	//We only send one flag at a time so we can do it like this.
	if (cmd.hasFlag("SN", 0)) {
		type = MSG_SEEN;
	}
	else if (cmd.getParam("TP", 0, tmp)) {
		type = (tmp == "1") ? TYPING_ON : TYPING_OFF;
	}
	else if (cmd.getParam("AC", 0, tmp)) {
		allowAutoCCPM = tmp == "1" ? true : false;
		type = NO_AUTOCONNECT;
	}
	else if (cmd.hasFlag("QU", 0)) {
		type = QUIT;
	}

	if (type != PMINFO_LAST)
		fire(PrivateChatListener::PMStatus(), this, type);
}

void PrivateChat::on(ClientManagerListener::UserUpdated, const OnlineUser& aUser) noexcept{
	if (aUser.getUser() != replyTo.user)
		return;

	setSupportsCCPM(ClientManager::getInstance()->getSupportsCCPM(replyTo, lastCCPMError));
	delayEvents.addEvent(USER_UPDATE, [this] {
		if (!online) {
			auto hubNames = ClientManager::getInstance()->getFormatedHubNames(replyTo);
			auto nicks = ClientManager::getInstance()->getFormatedNicks(replyTo);
			statusMessage(STRING(USER_WENT_ONLINE) + " [" + nicks + " - " + hubNames + "]",
				LogMessage::SEV_INFO);

			// online from a different hub?
			checkUserHub(false);
			online = true;
		}

		fire(PrivateChatListener::UserUpdated(), this);
	}, 1000);

	delayEvents.addEvent(CCPM_AUTO, [this] { checkAlwaysCCPM(); }, 3000);
}

void PrivateChat::logMessage(const string& aMessage) {
	if (SETTING(LOG_PRIVATE_CHAT)) {
		ParamMap params;
		params["message"] = aMessage;
		fillLogParams(params);
		LogManager::getInstance()->log(getUser(), params);
	}
}

void PrivateChat::fillLogParams(ParamMap& params) const {
	const CID& cid = getUser()->getCID();;
	params["hubNI"] = [&] { return Util::listToString(ClientManager::getInstance()->getHubNames(cid)); };
	params["hubURL"] = [&] { return getHubUrl(); };
	params["userCID"] = [&cid] { return cid.toBase32(); };
	params["userNI"] = [&] { return ClientManager::getInstance()->getNick(getUser(), getHubUrl()); };
	params["myCID"] = [] { return ClientManager::getInstance()->getMe()->getCID().toBase32(); };
}

string PrivateChat::getLogPath() const {
	ParamMap params;
	fillLogParams(params);
	return LogManager::getInstance()->getPath(getUser(), params);
}

}