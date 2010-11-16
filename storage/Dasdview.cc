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


#include "storage/Dasdview.h"
#include "storage/StorageDefines.h"
#include "storage/SystemCmd.h"


namespace storage
{

    Dasdview::Dasdview(const string& device)
	: dasd_format(Dasd::DASDF_NONE)
    {
	SystemCmd cmd(DASDVIEWBIN " --extended " + quote(device));

	if (cmd.retcode() == 0)
	{
	    if (cmd.select("^format") > 0)
	    {
		string tmp = cmd.getLine(0, true);
		y2mil("Format line:" << tmp);
		tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
		tmp = boost::to_lower_copy(extractNthWord(4, tmp), locale::classic());
		if( tmp == "cdl" )
		    dasd_format = Dasd::DASDF_CDL;
		else if( tmp == "ldl" )
		    dasd_format = Dasd::DASDF_LDL;
	    }

	    scanGeometry(cmd);
	}
	else
	{
	    y2err("dasdview failed");

	    geometry.heads = 15;
	    geometry.sectors = 12;
	    geometry.sector_size = 4096;
	}

	y2mil("device: " << device << " geometry:" << geometry << " dasd_format:" <<
	      toString(dasd_format));
    }


    void
    Dasdview::scanGeometry(SystemCmd& cmd)
    {
	if (cmd.select("number of cylinders") > 0)
	{
	    string tmp = cmd.getLine(0, true);
	    y2mil("Cylinder line:" << tmp);
	    tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	    tmp = extractNthWord( 3, tmp );
	    tmp >> geometry.cylinders;
	}

	if (cmd.select("tracks per cylinder") > 0)
	{
	    string tmp = cmd.getLine(0, true);
	    y2mil("Tracks line:" << tmp);
	    tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	    tmp = extractNthWord( 3, tmp );
	    tmp >> geometry.heads;
	}

	if (cmd.select("blocks per track") > 0)
	{
	    string tmp = cmd.getLine(0, true);
	    y2mil("Blocks line:" << tmp);
	    tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	    tmp = extractNthWord( 3, tmp );
	    tmp >> geometry.sectors;
	}

	if (cmd.select("blocksize") > 0)
	{
	    string tmp = cmd.getLine(0, true);
	    y2mil("Bytes line:" << tmp);
	    tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	    tmp = extractNthWord( 3, tmp );
	    tmp >> geometry.sector_size;
	}
    }


    Fdasd::Fdasd(const string& device)
    {
	SystemCmd cmd(FDASDBIN " --table " + quote(device));

	if (cmd.retcode() == 0)
	{
	    scanGeometry(cmd);

	    Regex part("^" + device + "[0123456789]+$");
	    cmd.select( device );
	    int cnt = cmd.numLines();
	    for (int i = 0; i < cnt; ++i)
	    {
		string line = cmd.getLine(i);
		string tmp = extractNthWord( 0, line );
		if( part.match(tmp) )
		{
		    scanEntryLine(line);
		}
	    }
	}
	else
	{
	    y2err("fdasd failed");
	}

	y2mil("device:" << device << " geometry:" << geometry);

	for (const_iterator it = entries.begin(); it != entries.end(); ++it)
	    y2mil(*it);
    }


    bool
    Fdasd::getEntry(unsigned num, Entry& entry) const
    {
	for (const_iterator it = entries.begin(); it != entries.end(); ++it)
	{
	    if (it->num == num)
	    {
		entry = *it;
		return true;
	    }
	}

	return false;
    }


    std::ostream& operator<<(std::ostream& s, const Fdasd::Entry& e)
    {
	return s << "num:" << e.num << " cylRegion:" << e.cylRegion << " headRegion:"
		 << e.headRegion;
    }


    void
    Fdasd::scanGeometry(SystemCmd& cmd)
    {
	if (cmd.select("cylinders") > 0)
	{
	    string tmp = cmd.getLine(0, true);
	    y2mil("Cylinder line:" << tmp);
	    tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	    tmp >> geometry.cylinders;
	}

	if (cmd.select("tracks per cylinder") > 0)
	{
	    string tmp = cmd.getLine(0, true);
	    y2mil("Tracks line:" << tmp);
	    tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	    tmp >> geometry.heads;
	}

	if (cmd.select("blocks per track") > 0)
	{
	    string tmp = cmd.getLine(0, true);
	    y2mil("Blocks line:" << tmp);
	    tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	    tmp >> geometry.sectors;
	}

	if (cmd.select("bytes per block") > 0)
	{
	    string tmp = cmd.getLine(0, true);
	    y2mil("Bytes line:" << tmp);
	    tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	    tmp >> geometry.sector_size;
	}
    }


    void
    Fdasd::scanEntryLine(const string& line)
    {
	std::istringstream Data(line);
	classic(Data);

	Entry entry;

	unsigned long StartM = 0;
	unsigned long EndM = 0;
	unsigned long SizeM = 0;

	string devname;
	Data >> devname >> StartM >> EndM >> SizeM;

	string::size_type pos = devname.find_last_not_of("0123456789");
	string(devname, pos + 1) >> entry.num;

	y2mil("Fields Num:" << entry.num << " Start:" << StartM << " End:" << EndM <<
	      " Size:" << SizeM);

	assert(!Data.fail());
	assert(entry.num != 0);

	if (Data.fail() || entry.num == 0)
	{
	    y2err("invalid line:" << line);
	    return;
	}

	unsigned long start = StartM / geometry.heads;
	unsigned long csize = EndM / geometry.heads - start + 1;
	if( start+csize > geometry.cylinders )
	{
	    csize = geometry.cylinders - start;
	    y2mil("new csize:" << csize);
	}
	entry.cylRegion = Region(start, csize);

	entry.headRegion = Region(StartM, SizeM);

	y2mil("Fields num:" << entry.num << " cylRegion:" << entry.cylRegion << " headRegion:" <<
	      entry.headRegion);

	entries.push_back(entry);
    }

}
