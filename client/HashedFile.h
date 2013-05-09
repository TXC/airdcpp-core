/*
 * Copyright (C) 2011-2013 AirDC++ Project
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

#ifndef DCPLUSPLUS_DCPP_HASHEDFILEINFO_H
#define DCPLUSPLUS_DCPP_HASHEDFILEINFO_H

#include "typedefs.h"
#include "GetSet.h"
#include "MerkleTree.h"
#include "Pointer.h"

namespace dcpp {

class HashedFile {
public:
	HashedFile() : size(-1), timeStamp(0) { }
	HashedFile(const TTHValue& aRoot, uint64_t aTimeStamp, int64_t aSize) :
		root(aRoot), timeStamp(aTimeStamp), size(aSize) { }

	~HashedFile() { }

	//GETSET(string, fileName, FileName);
	GETSET(TTHValue, root, Root);
	GETSET(uint64_t, timeStamp, TimeStamp);
	GETSET(int64_t, size, Size);

	/*struct FileLess {
		bool operator()(const HashedFilePtr& a, const HashedFilePtr& b) const { return (a->getFileName().compare(b->getFileName()) < 0); }
	};

	struct Name {
		const string& operator()(const HashedFilePtr& a) const { return a->getFileName(); }
	};*/
};

}

#endif // !defined(DCPLUSPLUS_DCPP_HASHEDFILEINFO_H)