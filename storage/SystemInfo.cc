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


#include "storage/StorageDefines.h"

#include "storage/SystemCmd.h"

#include "storage/SystemInfo.h"


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

CmdBtrfsShow::CmdBtrfsShow()
    {
    SystemCmd c(BTRFSBIN " filesystem show");
    if (c.retcode() != 0 || c.numLines() == 0)
	return;

    const vector<string>& lines = c.stdout();
    vector<string>::const_iterator it = lines.begin();

    while( it!=lines.end() )
	{
	while( it != lines.end() && !boost::contains( *it, " uuid: " ))
	    ++it;
	if( it!=lines.end() )
	    {
	    y2mil( "uuid line:" << *it );
	    string uuid = extractNthWord( 3, *it );
	    y2mil( "uuid:" << uuid );
	    Entry e;
	    ++it;
	    while( it!=lines.end() && !boost::contains( *it, " uuid: " ) && 
		   !boost::contains( *it, "devid " ) )
		++it;
	    while( it!=lines.end() && boost::contains( *it, "devid " ) )
		{
		y2mil( "devs line:" << *it );
		e.devices.push_back( extractNthWord( 7, *it ));
		++it;
		}
	    y2mil( "devs:" << e.devices );
	    fs[uuid] = e;
	    }
	}
    }

bool
CmdBtrfsShow::getEntry( const string& uuid, Entry& entry) const
    {
    const_iterator it = fs.find(uuid);
    if( it!=fs.end() )
	entry = it->second;
    return( it!=fs.end() );
    }

list<string> 
CmdBtrfsShow::getUuids() const
    {
    list<string> ret;

    const_iterator it = fs.begin();
    while( it != fs.end() )
	{
	ret.push_back( it->first );
	++it;
	}
    y2mil( "ret:" << ret );
    return( ret );
    }

}
