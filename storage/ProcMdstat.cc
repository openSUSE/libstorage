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


#include <locale>
#include <boost/algorithm/string.hpp>

#include "storage/AppUtil.h"
#include "storage/AsciiFile.h"
#include "storage/StorageInterface.h"
#include "storage/ProcMdstat.h"
#include "storage/Enum.h"
#include "storage/StorageDefines.h"
#include "storage/SystemCmd.h"
#include "storage/StorageTmpl.h"


namespace storage
{
    using namespace std;


    ProcMdstat::ProcMdstat()
    {
	AsciiFile mdstat("/proc/mdstat");
	mdstat.logContent();

	for (vector<string>::const_iterator it1 = mdstat.lines().begin(); it1 != mdstat.lines().end(); ++it1)
	{
	    if (extractNthWord(1, *it1) == ":")
	    {
		string name = extractNthWord(0, *it1);
		if (boost::starts_with(name, "md"))
		    data[name] = parse(*it1, *(it1 + 1));
	    }
	}

	for (const_iterator it = data.begin(); it != data.end(); ++it)
	    y2mil("data[" << it->first << "] -> " << it->second);
    }


    ProcMdstat::Entry
    ProcMdstat::parse(const string& line1, const string& line2)
    {
	ProcMdstat::Entry entry;

	string::size_type pos;
	string tmp;

	string line = line1;
	if( (pos=line.find( ':' ))!=string::npos )
	    line.erase( 0, pos+1 );
	boost::trim_left(line, locale::classic());
	if( (pos=line.find_first_of( app_ws ))!=string::npos )
	{
	    if (line.substr(0, pos) == "active")
		line.erase(0, pos);
	}
	boost::trim_left(line, locale::classic());
	if( (pos=line.find_first_of( app_ws ))!=string::npos )
	{
	    tmp = line.substr( 0, pos );
	    if( tmp=="(read-only)" || tmp=="(auto-read-only)" || tmp=="inactive" )
	    {
		entry.readonly = true;
		line.erase( 0, pos );
		boost::trim_left(line, locale::classic());
	    }
	}
	boost::trim_left(line, locale::classic());
	if( (pos=line.find_first_of( app_ws ))!=string::npos )
	{
	    if( line.substr( 0, pos ).find( "active" )!=string::npos )
		line.erase( 0, pos );
	}
	boost::trim_left(line, locale::classic());

	tmp = extractNthWord( 0, line );
	if (boost::starts_with(tmp, "raid"))
	{
	    entry.md_type = toValueWithFallback(tmp, RAID_UNK);
	    if (entry.md_type == RAID_UNK)
		y2war("unknown raid type " << tmp);

	    if( (pos=line.find_first_of( app_ws ))!=string::npos )
		line.erase( 0, pos );
	    if( (pos=line.find_first_not_of( app_ws ))!=string::npos && pos!=0 )
		line.erase( 0, pos );
	}
	else
	{
	    entry.is_container = true;
	}

	while( (pos=line.find_first_not_of( app_ws ))==0 )
	{
	    tmp = extractNthWord( 0, line );

	    string d;
	    string::size_type bracket = tmp.find( '[' );
	    if( bracket!=string::npos )
		d = normalizeDevice(tmp.substr(0, bracket));
	    else
		d = normalizeDevice(tmp);

	    bool is_spare = boost::ends_with(tmp, "(S)");
	    if (!is_spare)
		entry.devices.push_back(d);
	    else
		entry.spares.push_back(d);

	    line.erase( 0, tmp.length() );
	    if( (pos=line.find_first_not_of( app_ws ))!=string::npos && pos!=0 )
		line.erase( 0, pos );
	}

	extractNthWord(0, line2) >> entry.size_k;

	pos = line2.find( "chunk" );
	if( pos != string::npos )
	{
	    pos = line2.find_last_not_of( app_ws, pos-1 );
	    pos = line2.find_last_of( app_ws, pos );
	    line2.substr( pos+1 ) >> entry.chunk_k;
	}

	pos = line2.find("super");
	if (pos != string::npos)
	{
	    string::size_type pos1 = line2.find_first_of(app_ws, pos);
	    pos1 = line2.find_first_not_of(app_ws, pos1);
	    string::size_type pos2 = line2.find_first_of(app_ws, pos1);
	    entry.super = string(line2, pos1, pos2 - pos1);

	    if (!entry.is_container && boost::starts_with(entry.super, "external:"))
	    {
		string::size_type pos1 = entry.super.find_first_of("/");
		string::size_type pos2 = entry.super.find_last_of("/");

		if (pos1 != string::npos && pos2 != string::npos && pos1 != pos2)
		{
		    entry.has_container = true;
		    entry.container_name = string(entry.super, pos1 + 1, pos2 - pos1 - 1);
		    entry.container_member = string(entry.super, pos2 + 1);
		}
	    }
	}

	entry.md_parity = PAR_DEFAULT;
	pos = line2.find( "algorithm" );
	if( pos != string::npos )
	{
	    unsigned alg = 999;
	    pos = line2.find_first_of( app_ws, pos );
	    pos = line2.find_first_not_of( app_ws, pos );
	    line2.substr( pos ) >> alg;
	    switch( alg )
	    {
		case 0:
		    entry.md_parity = LEFT_ASYMMETRIC;
		    break;
		case 1:
		    entry.md_parity = RIGHT_ASYMMETRIC;
		    break;
		case 2:
		    entry.md_parity = LEFT_SYMMETRIC;
		    break;
		case 3:
		    entry.md_parity = RIGHT_SYMMETRIC;
		    break;
		case 4:
		    entry.md_parity = PAR_FIRST;
		    break;
		case 5:
		    entry.md_parity = PAR_LAST;
		    break;
		case 16:
		    entry.md_parity = LEFT_ASYMMETRIC_6;
		    break;
		case 17:
		    entry.md_parity = RIGHT_ASYMMETRIC_6;
		    break;
		case 18:
		    entry.md_parity = LEFT_SYMMETRIC_6;
		    break;
		case 19:
		    entry.md_parity = RIGHT_SYMMETRIC_6;
		    break;
		case 20:
		    entry.md_parity = PAR_FIRST_6;
		    break;
		default:
		    y2war("unknown parity " << line2.substr(pos));
		    break;
	    }
	}
	pos = line2.find( "-copies" );
	if( pos != string::npos )
	{
	    unsigned num = 0;
	    string where;
	    pos = line2.find_last_of( app_ws, pos );
	    line2.substr( pos ) >> where;
	    pos = line2.find_last_not_of( app_ws, pos );
	    pos = line2.find_last_of( app_ws, pos );
	    line2.substr( pos ) >> num;
	    y2mil( "where:" << where << " num:" << num );
	    if( where=="near-copies" )
		entry.md_parity = (num==3)?PAR_NEAR_3:PAR_NEAR_2;
	    else if( where=="far-copies" )
		entry.md_parity = (num==3)?PAR_FAR_3:PAR_FAR_2;
	    else if( where=="offset-copies" )
		entry.md_parity = (num==3)?PAR_OFFSET_3:PAR_OFFSET_2;
	}

	return entry;
    }


    list<string>
    ProcMdstat::getEntries() const
    {
	list<string> ret;
	for (const_iterator i = data.begin(); i != data.end(); ++i)
	    ret.push_back(i->first);
	return ret;
    }


    bool
    ProcMdstat::getEntry(const string& name, Entry& entry) const
    {
	const_iterator i = data.find(name);
	if (i == data.end())
	    return false;

	entry = i->second;
	return true;
    }


    std::ostream& operator<<(std::ostream& s, const ProcMdstat::Entry& entry)
    {
	s << "md_type:" << toString(entry.md_type);

	if (entry.md_parity != PAR_DEFAULT)
	    s << " md_parity:" + toString(entry.md_parity);

	if (!entry.super.empty())
	    s << " super:" + entry.super;

	if (entry.chunk_k != 0)
	    s << " chunk_k:" << entry.chunk_k;

	s << " size_k:" << entry.size_k;

	if (entry.readonly)
	    s << " readonly";

	s << " devices:" << entry.devices;
	if (!entry.spares.empty())
	    s << " spares:" << entry.spares;

	if (entry.is_container)
	    s << " is_container";

	if (entry.has_container)
	    s << " has_container container_name:" << entry.container_name << " container_member:"
	      << entry.container_member;

	return s;
    }


    bool
    getMdadmDetails(const string& dev, MdadmDetails& detail)
    {
	SystemCmd cmd(MDADMBIN " --detail " + quote(dev) + " --export");
	if (cmd.retcode() != 0)
	{
	    y2err("running mdadm failed");
	    return false;
	}

	detail.uuid = detail.devname = detail.metadata = "";

	const vector<string>& lines = cmd.stdout();
	for (vector<string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
	{
	    if (boost::starts_with(*it, "MD_UUID="))
		detail.uuid = string(*it, 8);
	    else if (boost::starts_with(*it, "MD_DEVNAME="))
		detail.devname = string(*it, 11);
	    else if (boost::starts_with(*it, "MD_METADATA="))
		detail.metadata = string(*it, 12);
	}

	y2mil("dev:" << dev << " uuid:" << detail.uuid << " devname:" << detail.devname <<
	      " metadata:" << detail.metadata);
	return true;
    }


    bool
    isImsmPlatform()
    {
	bool ret = false;

	SystemCmd cmd(MDADMBIN " --detail-platform");

	cmd.select("Platform : ");
	if (cmd.numLines(true) > 0)
	{
	    string line = cmd.getLine(0, true);
	    if (boost::contains(line, "Intel(R) Matrix Storage Manager"))
	    {
		ret = true;
	    }
	}

	y2mil("ret:" << ret);
	return ret;
    }

}
