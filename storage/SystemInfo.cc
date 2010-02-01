/*
 * Copyright (c) [2004-2010] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#include "storage/SystemInfo.h"
#include "storage/ProcParts.h"
#include "storage/ProcMounts.h"
#include "storage/Blkid.h"
#include "storage/DmCo.h"
#include "storage/DmraidCo.h"
#include "storage/DmmultipathCo.h"


namespace storage
{

    SystemInfo::SystemInfo()
    {
	y2deb("constructed SystemInfo");
    }


    SystemInfo::~SystemInfo()
    {
	y2deb("destructed SystemInfo");
    }


    const UdevMap&
    SystemInfo::getUdevMap(const string& path)
    {
	map<string, UdevMap>::iterator pos = udevmaps.lower_bound(path);
	if (pos == udevmaps.end() || map<string, UdevMap>::key_compare()(path, pos->first))
	{
	    UdevMap udevmap = storage::getUdevMap(path.c_str());
	    pos = udevmaps.insert(pos, map<string, UdevMap>::value_type(path, udevmap));
	}

	return pos->second;
    }

}
