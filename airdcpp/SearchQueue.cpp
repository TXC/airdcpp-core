/* 
 * Copyright (C) 2003-2016 RevConnect, http://www.revconnect.com
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
#include "SearchQueue.h"

#include "TimerManager.h"
#include "QueueManager.h"
#include "SearchManager.h"

namespace dcpp {

using boost::range::for_each;
	
SearchQueue::SearchQueue() : nextInterval(10 * 1000) {

}

SearchQueue::~SearchQueue() { }

int SearchQueue::getInterval(Priority aPriority) const noexcept {
	int ret = 0;
	switch(aPriority) {
		case Priority::HIGHEST:
		case Priority::HIGH: ret = 5000; break;
		case Priority::NORMAL: ret = 10000; break;
		case Priority::LOW: ret = 15000; break;
		default: ret = 20000; break;
	}
	return max(ret, minInterval);
}

void SearchQueue::clear() noexcept {
	Lock l(cs);
	searchQueue.clear();
}

uint64_t SearchQueue::getNextSearchTick() const noexcept {
	return lastSearchTime + nextInterval; 
}

uint64_t SearchQueue::add(const SearchPtr& s) noexcept {
	dcassert(s->owners.size() == 1);
	uint32_t x = 0;
	bool add = true;

	Lock l(cs);

	auto i = searchQueue.begin();
	for (;;) {
		if (i == searchQueue.end())
			break;
		if(s->priority < (*i)->priority) {
			//we found our place :}
			if((*i) == s) {
				//replace the lower prio item with this one, move the owners from the old search
				//boost::for_each(i->owners, [&s](Search& tmp) { s.owners.insert(tmp); });
				searchQueue.erase(i);
				prev(i);
			}
			break;
		} else if(s == *i) {
			//don't queue the same item twice
			void* aOwner = *(s->owners.begin());
			(*i)->owners.insert(aOwner);
			add = false;
			break;
		}

		x += getInterval((*i)->priority);
		i++;
	}

	if (add)
		searchQueue.insert(i, move(s));

	auto now = GET_TICK();
	if (x > 0) {
		dcassert(nextInterval > 0);
		//LogManager::getInstance()->message("Time remaining in this queue: " + Util::toString(x - (getNextSearchTick() - now)) + " (next search " + Util::toString(getNextSearchTick())
		//	+ "ms, now " + Util::toString(now) + "ms, queueTime: " + Util::toString(x) + "ms)");

		if (getNextSearchTick() <= now) {
			//we have queue but the a search can be performed
			return x;
		} else {
			//we have queue and even waiting time for the next search
			return x + (getNextSearchTick() - now);
		}
	} else {
		//we have the first item, recount the tick allowed for the search
		nextInterval = getInterval(searchQueue.front()->priority);
		if (getNextSearchTick() <= now) {
			return 0;
		}

		//we still need to wait after the previous search, subract the waiting time from the interval of this item
		return getNextSearchTick() - now;
	}
}

SearchPtr SearchQueue::pop() noexcept {
	uint64_t now = GET_TICK();
	if(now <= lastSearchTime + nextInterval) 
		return nullptr;
	
	{
		Lock l(cs);
		if(!searchQueue.empty()){
			auto s = move(searchQueue.front());
			searchQueue.pop_front();
			lastSearchTime = GET_TICK();
			nextInterval = !searchQueue.empty() ? getInterval(searchQueue.front()->priority) : minInterval;
			return move(s);
		} else {
			nextInterval = -1;
		}
	}

	return nullptr;
}

bool SearchQueue::hasWaitingTime(uint64_t aTick) const noexcept {
	return nextInterval < 0 || lastSearchTime + nextInterval > aTick;
}

bool SearchQueue::cancelSearch(void* aOwner) noexcept {
	dcassert(aOwner);

	Lock l(cs);
	for(auto i = searchQueue.begin(); i != searchQueue.end(); i++){
		if((*i)->owners.count(aOwner)){
			(*i)->owners.erase(aOwner);
			if((*i)->owners.empty())
				searchQueue.erase(i);
			return true;
		}
	}
	return false;
}

}
