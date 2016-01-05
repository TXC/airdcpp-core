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

#ifndef DCPLUSPLUS_DCPP_VIEWFILE_H
#define DCPLUSPLUS_DCPP_VIEWFILE_H

#include "forward.h"

#include "MerkleTree.h"
#include "TrackableDownloadItem.h"

namespace dcpp {
	class ViewFile : public TrackableDownloadItem {

	typedef std::function<void(const TTHValue&)> UpdateF;
	public:
		ViewFile(const string& aTarget, const TTHValue& aTTH, bool aIsText, UpdateF&& aUpdateFunction) noexcept;

		const string& getPath() const noexcept {
			return path;
		}

		bool isText() const noexcept {
			return text;
		}

		const TTHValue& getTTH() const noexcept {
			return tth;
		}
	protected:
		void onStateChanged() noexcept;
	private:
		const string path;
		const UpdateF updateFunction;
		const TTHValue tth;
		const bool text;
	};

} // namespace dcpp

#endif // !defined(DCPLUSPLUS_DCPP_VIEWFILE_H)
