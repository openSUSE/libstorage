/*
 * Copyright (c) [2004-2009] Novell, Inc.
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


#include "storage/AppUtil.h"
#include "storage/StorageTmpl.h"
#include "storage/AsciiFile.h"
#include "storage/ProcParts.h"


namespace storage
{
    using namespace std;


    ProcParts::ProcParts()
    {
	reload();
    }


    void
    ProcParts::reload()
    {
	data.clear();

	AsciiFile parts("/proc/partitions");
	parts.remove(0, 2);

	for (vector<string>::const_iterator it = parts.lines().begin(); it != parts.lines().end(); ++it)
	{
	    string device = "/dev/" + extractNthWord(3, *it);
	    unsigned long long sizeK;
	    extractNthWord(2, *it) >> sizeK;
	    data[device] = sizeK;
	}

	for (const_iterator it = data.begin(); it != data.end(); ++it)
	    y2mil("data[" << it->first << "] -> " << it->second);
    }


    bool
    ProcParts::findDevice(const string& device) const
    {
    return( findEntry(device)!=data.end() );
    }


    bool
    ProcParts::getSize(const string& device, unsigned long long& sizeK) const
    {
	const_iterator i = findEntry(device);
	if (i == data.end())
	{
	    y2err("dev:" << device << " not found");
	    return false;
	}

	sizeK = i->second;
	y2mil("dev:" << device << " sizeK:" << sizeK);
	return true;
    }


    list<string>
    ProcParts::getEntries() const
    {
	list<string> ret;
	for (const_iterator i = data.begin(); i != data.end(); ++i)
	    ret.push_back(i->first);
	return ret;
    }


    ProcParts::const_iterator
    ProcParts::findEntry(const string& device) const
    {
	return( data.find(normalizeDevice(device)) );
    }

}
