/*
 * Copyright (C) 2011-2016 AirDC++ Project
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

#ifndef DCPLUSPLUS_DCPP_TARGET_UTIL_H
#define DCPLUSPLUS_DCPP_TARGET_UTIL_H

#include "typedefs.h"

#include "GetSet.h"
#include "Util.h"

#include <string>

namespace dcpp {
using std::string;

class TargetUtil {

public:
	class TargetInfo {
	public:
		explicit TargetInfo() { }
		explicit TargetInfo(const string& aPath, int64_t aFreeSpace = 0) : target(aPath), freeDiskSpace(aFreeSpace) { }

		int64_t getRealFreeSpace() const { return freeDiskSpace - queued; }
		bool isInitialized() { return freeDiskSpace != 0 || queued != 0 || !target.empty(); }

		bool operator<(const TargetInfo& ti) const {
			return (freeDiskSpace - queued) < (ti.freeDiskSpace - ti.queued);
		}

		int64_t getQueued() const noexcept {
			return queued;
		}

		bool hasTarget() const noexcept {
			return !target.empty();
		}

		bool hasFreeSpace(int64_t aRequiredBytes) const noexcept {
			return getRealFreeSpace() >= aRequiredBytes;
		}

		GETSET(string, target, Target);
		IGETSET(int64_t, freeDiskSpace, FreeDiskSpace, 0);

		void addQueued(int64_t aBytes) noexcept {
			queued += aBytes;
		}
	private:
		int64_t queued = 0;
	};

	typedef unordered_map<string, TargetInfo, noCaseStringHash, noCaseStringEq> TargetInfoMap;
	typedef unordered_set<string, noCaseStringHash, noCaseStringEq> VolumeSet;

	// Get a set of all mount points
	static void getVolumes(VolumeSet& volumes);

	static string getMountPath(const string& aPath, const VolumeSet& aVolumes);

	static string formatSizeNotification(const TargetInfo& ti, int64_t aSize);
	static string formatSizeConfirmation(const TargetInfo& ti, int64_t aSize);


	// LEGACY
	enum TargetType {
		TARGET_PATH,
		TARGET_FAVORITE,
		TARGET_SHARE,
		TARGET_LAST
	};

	// LEGACY
	static bool getVirtualTarget(const string& aTarget, TargetType targetType, TargetInfo& ti_, const int64_t& size);

	static bool getDiskInfo(TargetInfo& ti_);
private:
	static bool getTarget(const OrderedStringSet& aTargets, TargetInfo& ti_, const int64_t& size);
	static void compareMap(const TargetInfoMap& targets, TargetInfo& retTi_);
};

}

#endif