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

#ifndef DCPLUSPLUS_DCPP_MESSAGE_H
#define DCPLUSPLUS_DCPP_MESSAGE_H

#include "forward.h"
#include "GetSet.h"

namespace dcpp {

class ChatMessage {
public:
	ChatMessage(const string& aText, const OnlineUserPtr& aFrom, const OnlineUserPtr& aTo = nullptr, const OnlineUserPtr& aReplyTo = nullptr) noexcept;

	GETSET(OnlineUserPtr, from, From);
	GETSET(OnlineUserPtr, to, To);
	GETSET(OnlineUserPtr, replyTo, ReplyTo);

	GETSET(time_t, time, Time);
	IGETSET(bool, thirdPerson, ThirdPerson, false);

	GETSET(bool, read, Read);
	GETSET(uint64_t, id, Id);

	string format() const;

	const string& getText() const noexcept {
		return text;
	}
private:
	string text;
};

class LogMessage {
public:
	enum Severity : uint8_t { SEV_INFO, SEV_WARNING, SEV_ERROR };

	LogMessage(const string& aMessage, Severity sev) noexcept;

	uint64_t getId() const noexcept {
		return id;
	}

	const string& getText() const noexcept {
		return text;
	}

	Severity getSeverity() const noexcept {
		return severity;
	}

	time_t getTime() const noexcept {
		return time;
	}

	IGETSET(bool, read, Read, false);
private:
	uint64_t id;
	string text;
	time_t time;
	Severity severity;
};

struct Message {
	Message(const ChatMessagePtr& aMessage) noexcept : type(TYPE_CHAT), chatMessage(aMessage) {}
	Message(const LogMessagePtr& aMessage) noexcept : type(TYPE_LOG), logMessage(aMessage) {}

	enum Type {
		TYPE_CHAT,
		TYPE_LOG
	};

	const ChatMessagePtr chatMessage = nullptr;
	const LogMessagePtr logMessage = nullptr;

	const Type type;
};

} // namespace dcpp

#endif // !defined(DCPLUSPLUS_DCPP_CHAT_MESSAGE_H)
