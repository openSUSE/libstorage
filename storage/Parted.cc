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


#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/hdreg.h>
#include <fstream>

#include "storage/AppUtil.h"
#include "storage/SystemCmd.h"
#include "storage/StorageDefines.h"
#include "storage/Parted.h"
#include "storage/Enum.h"
#include "storage/Partition.h"


namespace storage
{
    using namespace std;


    Geometry::Geometry()
	: cylinders(0), heads(0), sectors(0), logical_sector_size(512)
    {
    }


    std::ostream& operator<<(std::ostream& s, const Geometry& geo)
    {
	return s << "cylinders:" << geo.cylinders << " heads:" << geo.heads << " sectors:"
		 << geo.sectors << " logical_sector_size:" << geo.logical_sector_size;
    }


    bool
    detectGeometry(const string& device, Geometry& geo)
    {
	y2mil("device:" << device);

	bool ret = false;

	int fd = open(device.c_str(), O_RDONLY);
	if (fd >= 0)
	{
	    int rcode = ioctl(fd, BLKSSZGET, &geo.logical_sector_size);
	    y2mil("BLKSSZGET rcode:" << rcode << " logical_sector_size:" << geo.logical_sector_size);

	    geo.cylinders = 16;
	    geo.heads = 255;
	    geo.sectors = 63;

	    struct hd_geometry hd_geo;
	    rcode = ioctl(fd, HDIO_GETGEO, &hd_geo);
	    y2mil("HDIO_GETGEO rcode:" << rcode << " cylinders:" << (unsigned) hd_geo.cylinders <<
		  " heads:" << (unsigned) hd_geo.heads << " sectors:" << (unsigned) hd_geo.sectors);
	    if (rcode == 0)
	    {
		geo.cylinders = hd_geo.cylinders > 0 ? hd_geo.cylinders : geo.cylinders;
		geo.heads = hd_geo.heads > 0 ? hd_geo.heads : geo.heads;
		geo.sectors = hd_geo.sectors > 0 ? hd_geo.sectors : geo.sectors;
	    }

	    uint64_t bytes = 0;
	    rcode = ioctl(fd, BLKGETSIZE64, &bytes);
	    y2mil("BLKGETSIZE64 rcode:" << rcode << " bytes:" << bytes);
	    if (rcode == 0 && bytes != 0)
	    {
		geo.cylinders = bytes / (uint64_t)(geo.cylinderSize());
		y2mil("calc cylinders:" << geo.cylinders);
		ret = true;
	    }
	    else
	    {
		unsigned long blocks;
		rcode = ioctl(fd, BLKGETSIZE, &blocks);
		y2mil("BLKGETSIZE rcode:" << rcode << " blocks:" << blocks);
		if (rcode == 0 && blocks != 0)
		{
		    geo.cylinders = (uint64_t)(blocks) * 512 / (uint64_t)(geo.cylinderSize());
		    y2mil("calc cylinders:" << geo.cylinders);
		    ret = true;
		}
	    }

	    close(fd);
	}

	y2mil("device:" << device << " ret:" << ret << " " << geo);
	return ret;
    }


    Parted::Parted(const string& device)
	: gpt_enlarge(false)
    {
	SystemCmd cmd(PARTEDCMD + quote(device) + " unit cyl print unit s print");

	if (cmd.select("Partition Table:") > 0)
	    label = extractNthWord(2, cmd.getLine(0, true));
	else
	    y2war("could not find partition table");

	// only present for unrecognised disk label due to patch in parted
	if (cmd.select("BIOS cylinder") > 0)
	    scanGeometryLine(cmd.getLine(0, true));
	else
	    y2err("could not find geometry");

	// not present for unrecognised disk label
	if (cmd.select("Sector size") > 0)
	    scanSectorSizeLine(cmd.getLine(0, true));
	else
	    y2war("could not find sector size");

	gpt_enlarge = cmd.select("fix the GPT to use all") > 0;

	if (label != "loop")
	{
	    int els = 0;

	    const vector<string>& lines = cmd.stdout();
	    for (vector<string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
	    {
		if (it->empty())
		    els++;

		string tmp = extractNthWord(0, *it);
		if (!tmp.empty() && isdigit(tmp[0]))
		{
		    if (els <= 2)
			scanCylEntryLine(*it);
		    else
			scanSecEntryLine(*it);
		}
	    }
	}

	y2mil("device:" << device << " label:" << label);
	y2mil(geometry);
	if (gpt_enlarge)
	    y2mil("gpt_enlarge");

	for (const_iterator it = entries.begin(); it != entries.end(); ++it)
	    y2mil(*it);
    }


    bool
    Parted::getEntry(unsigned num, Entry& entry) const
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


    std::ostream& operator<<(std::ostream& s, const Parted::Entry& e)
    {
	s << "num:" << e.num << " cylRegion:" << e.cylRegion << " secRegion:" << e.secRegion
	  << " type:" << toString(e.type) << " id:" << e.id;

	if (e.boot)
	    s << " boot";

	return s;
    }


    void
    Parted::scanGeometryLine(const string& line)
    {
	string tmp(line);
	tmp.erase(0, tmp.find(':') + 1);
	tmp = extractNthWord(0, tmp);

	list<string> l = splitString(extractNthWord(0, tmp), ",");
	assert(l.size() == 3);
	if (l.size() == 3)
	{
	    list<string>::const_iterator i = l.begin();
	    *i++ >> geometry.cylinders;
	    *i++ >> geometry.heads;
	    *i++ >> geometry.sectors;
	}
	else
	{
	    y2err("could not find geometry");
	}
    }


    void
    Parted::scanSectorSizeLine(const string& line)
    {
	string tmp(line);
	tmp.erase(0, tmp.find(':') + 1);
	tmp = extractNthWord(0, tmp);

	list<string> l = splitString(extractNthWord(0, tmp), "/");
	assert(l.size() == 2);
	if (l.size() == 2)
	{
	    list<string>::const_iterator i = l.begin();
	    *i >> geometry.logical_sector_size;
	}
	else
	{
	    y2war("could not find sector size");
	}
    }


    void
    Parted::scanCylEntryLine(const string& line)
    {
	Entry entry;

	std::istringstream Data(line);
	classic(Data);

	unsigned long StartM = 0;
	unsigned long EndM = 0;
	unsigned long SizeM = 0;
	string PartitionTypeStr;
	string skip;

	if( label == "msdos" )
	{
	    Data >> entry.num >> StartM >> skip >> EndM >> skip >> SizeM >> skip >> PartitionTypeStr;
	}
	else
	{
	    Data >> entry.num >> StartM >> skip >> EndM >> skip >> SizeM >> skip;
	}

	assert(entry.num != 0);

	if (Data.fail() || entry.num == 0)
	{
	    y2err("invalid line:" << line);
	    return;
	}

	char c;
	string TInfo;
	Data.unsetf(ifstream::skipws);
	Data >> c;
	char last_char = ',';
	while( Data.good() && !Data.eof() )
	{
	    if( !isspace(c) )
	    {
		TInfo += c;
		last_char = c;
	    }
	    else
	    {
		if( last_char != ',' )
		{
		    TInfo += ",";
		    last_char = ',';
		}
	    }
	    Data >> c;
	}

	y2mil("num:" << entry.num << " Start:" << StartM << " End:" << EndM << " Size:" << SizeM);

	unsigned long start = StartM;
	unsigned long csize = EndM-StartM+1;
	if( start + csize > geometry.cylinders )
	{
	    csize = geometry.cylinders - start;
	    y2mil("new csize:" << csize);
	}
	entry.cylRegion = Region(start, csize);

	boost::to_lower(TInfo, locale::classic());
	list<string> flags = splitString(TInfo, ",");
	y2mil("TInfo:" << TInfo << " flags:" << flags);

	entry.boot = contains(flags, "boot");

	entry.id = Partition::ID_LINUX;

	if( label == "msdos" )
	{
	    if( PartitionTypeStr == "extended" )
	    {
		entry.type = EXTENDED;
		entry.id = Partition::ID_EXTENDED;
	    }
	    else if( entry.num >= 5 )
	    {
		entry.type = LOGICAL;
	    }
	}
	else if (contains_if(flags, string_starts_with("fat")))
	{
	    entry.id = Partition::ID_DOS32;
	}
	else if (contains(flags, "ntfs"))
	{
	    entry.id = Partition::ID_NTFS;
	}
	else if (contains_if(flags, string_contains("swap")))
	{
	    entry.id = Partition::ID_SWAP;
	}
	else if (contains(flags, "raid"))
	{
	    entry.id = Partition::ID_RAID;
	}
	else if (contains(flags, "lvm"))
	{
	    entry.id = Partition::ID_LVM;
	}

	list<string>::const_iterator it1 = find_if(flags.begin(), flags.end(),
						   string_starts_with("type="));
	if (it1 != flags.end())
	{
	    string val = string(*it1, 5);

	    if( label != "mac" )
	    {
		int tmp_id = 0;
		std::istringstream Data2(val);
		classic(Data2);
		Data2 >> std::hex >> tmp_id;
		if( tmp_id>0 )
		{
		    entry.id = tmp_id;
		}
	    }
	    else
	    {
		if( entry.id == Partition::ID_LINUX )
		{
		    if( val.find( "apple_hfs" ) != string::npos ||
			val.find( "apple_bootstrap" ) != string::npos )
		    {
			entry.id = Partition::ID_APPLE_HFS;
		    }
		    else if( val.find( "apple_partition" ) != string::npos ||
			     val.find( "apple_driver" ) != string::npos ||
			     val.find( "apple_loader" ) != string::npos ||
			     val.find( "apple_boot" ) != string::npos ||
			     val.find( "apple_prodos" ) != string::npos ||
			     val.find( "apple_fwdriver" ) != string::npos ||
			     val.find( "apple_patches" ) != string::npos )
		    {
			entry.id = Partition::ID_APPLE_OTHER;
		    }
		    else if( val.find( "apple_ufs" ) != string::npos )
		    {
			entry.id = Partition::ID_APPLE_UFS;
		    }
		}
	    }
	}

	if( label == "gpt" )
	{
	    if (contains(flags, "boot") && contains_if(flags, string_starts_with("fat")))
	    {
		entry.id = Partition::ID_GPT_BOOT;
	    }
	    if (contains(flags, "hp-service"))
	    {
		entry.id = Partition::ID_GPT_SERVICE;
	    }
	    if (contains(flags, "msftres"))
	    {
		entry.id = Partition::ID_GPT_MSFTRES;
	    }
	    if (contains(flags, "hfs+") || contains(flags, "hfs"))
	    {
		entry.id = Partition::ID_APPLE_HFS;
	    }
	}
	y2mil("num:" << entry.num << " id:" << entry.id << " type:" << toString(entry.type));

	entries.push_back(entry);
    }


    void
    Parted::scanSecEntryLine(const string& line)
    {
	std::istringstream Data(line);
	classic(Data);

	unsigned num;
	unsigned long long startSec = 0;
	unsigned long long endSec = 0;
	unsigned long long sizeSec = 0;
	string skip;

	Data >> num >> startSec >> skip >> endSec >> skip >> sizeSec >> skip;

	assert(num != 0);

	if (Data.fail() || num == 0)
	    return;

	for (iterator it = entries.begin(); it != entries.end(); ++it)
	{
	    if (it->num == num)
	    {
		it->secRegion = Region(startSec, sizeSec);
		return;
	    }
	}
    }

}
