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


#include "storage/AppUtil.h"
#include "storage/SystemCmd.h"
#include "storage/AsciiFile.h"
#include "storage/ProcMounts.h"
#include "storage/StorageTmpl.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    ProcMounts::ProcMounts()
    {
	reload();
    }


    void
    ProcMounts::reload()
    {
	data.clear();

    SystemCmd mt(MOUNTBIN);

    AsciiFile mounts("/proc/mounts");

    for (vector<string>::const_iterator it = mounts.lines().begin(); it != mounts.lines().end(); ++it)
    {
	string dev = boost::replace_all_copy(extractNthWord(0, *it), "\\040", " ");
	string dir = boost::replace_all_copy(extractNthWord(1, *it), "\\040", " ");

	if (dev == "rootfs" || dev == "/dev/root" || isBind(mt, dir))
	{
	    y2mil("skipping line:" << *it);
	}
	else
	{
	    FstabEntry entry;
	    entry.device = dev;
	    entry.mount = dir;
	    entry.fs = extractNthWord(2, *it);
	    entry.opts = splitString(extractNthWord(3, *it), ",");
	    data.insert(make_pair(dev, entry));
	}
    }

    AsciiFile swaps("/proc/swaps");
    swaps.remove(0, 1);

    for (vector<string>::const_iterator it = swaps.lines().begin(); it != swaps.lines().end(); ++it)
    {
	string dev = boost::replace_all_copy(extractNthWord(0, *it), "\\040", " ");
	string::size_type pos = dev.find(" (deleted)");
	if (pos != string::npos)
	    dev.erase(pos);

	FstabEntry entry;
	entry.device = dev;
	entry.mount = "swap";
	entry.fs = "swap";
	data.insert(make_pair(dev, entry));
    }

    for (const_iterator it = data.begin(); it != data.end(); ++it)
	y2mil("data[" << it->first << "] -> " << it->second);
    }


    bool
    ProcMounts::isBind(SystemCmd& mt, const string& dir) const
    {
	bool ret = false;

	mt.select( (string)" on "+dir+' ' );
	if( mt.numLines(true)>0 )
	{
	    list<string> sl = splitString( mt.getLine(0,true) );
	    y2mil( "sl:" << sl );
	    if( sl.size()>=6 )
	    {
		list<string>::const_iterator i=sl.begin();
		++i;
		++i;
		++i;
		++i;
		if( *i == "none" )
		{
		    ++i;
		    string opt = *i;
		    if( !opt.empty() && opt[0]=='(' )
			opt.erase( 0, 1 );
		    if( !opt.empty() && opt[opt.size()-1]==')' )
			opt.erase( opt.size()-1, 1 );
		    sl = splitString( opt, "," );
		    y2mil( "sl:" << sl );
		    ret = find(sl.begin(), sl.end(), "bind") != sl.end();
		}
	    }
	}

	return ret;
    }


string
ProcMounts::getMount(const string& device) const
    {
    string ret;
    const_iterator i = data.find(device);
    if (i != data.end())
	ret = i->second.mount;
    return ret;
    }

string
ProcMounts::getMount(const list<string>& devices) const
    {
    string ret;
    list<string>::const_iterator i = devices.begin();
    while (ret.empty() && i != devices.end())
	{
	ret = getMount( *i );
	++i;
	}
    return ret;
    }


    list<string>
    ProcMounts::getAllMounts(const string& device) const
    {
	list<string> ret;

	pair<const_iterator, const_iterator> range = data.equal_range(device);
	for (const_iterator i = range.first; i != range.second; ++i)
	    ret.push_back(i->second.mount);

	return ret;
    }


    list<string>
    ProcMounts::getAllMounts(const list<string>& devices) const
    {
	list<string> ret;

	for (list<string>::const_iterator i = devices.begin(); i != devices.end(); ++i)
	    ret.splice(ret.end(), getAllMounts(*i));

	return ret;
    }


map<string, string>
ProcMounts::allMounts() const
    {
    map<string, string> ret;
    for (const_iterator i = data.begin(); i != data.end(); ++i)
	{
	ret[i->second.mount] = i->first;
	}
    return ret;
    }


    list<FstabEntry>
    ProcMounts::getEntries() const
    {
	list<FstabEntry> ret;
	for (const_iterator i = data.begin(); i != data.end(); ++i)
	    ret.push_back(i->second);
	return ret;
    }

}
