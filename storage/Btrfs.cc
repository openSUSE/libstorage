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


#include <sys/stat.h>
#include <sstream>

#include "storage/Btrfs.h"
#include "storage/StorageTypes.h"
#include "storage/Container.h"
#include "storage/AppUtil.h"
#include "storage/SystemInfo.h"
#include "storage/Storage.h"
#include "storage/SystemCmd.h"
#include "storage/StorageDefines.h"


namespace storage
{
using namespace std;

Btrfs::Btrfs(const BtrfsCo& d, const Volume& v, unsigned long long sz,
             const list<string>& devs ) : Volume(d, v), devices(devs)
    {
    y2mil("constructed btrfs vol size:" << sz << " devs:" << devs );
    y2mil("constructed btrfs vol from:" << v );
    setSize( sz );
    }

Btrfs::Btrfs(const BtrfsCo& d, const xmlNode* node ) : Volume(d, node)
    {
    list<const xmlNode*> l = getChildNodes(node, "devices");
    for( list<const xmlNode*>::const_iterator it=l.begin(); it!=l.end(); ++it )
	{
	string s;
	getChildValue(*it, "devices", s);
	devices.push_back(s);
	}
    l = getChildNodes(node, "subvolumes");
    for (list<const xmlNode*>::const_iterator it=l.begin(); it!=l.end(); ++it )
	{
	subvol.push_back(Subvolume(*it));
	}
    }


Btrfs::Btrfs(const BtrfsCo& c, const Btrfs& v) : Volume(c, v), 
    devices(v.devices), subvol(v.subvol)
    {
    y2deb("copy-constructed Btrfs from " << v.dev);
    }


Btrfs::~Btrfs()
    {
    y2deb("destructed Btrfs " << dev);
    }

void Btrfs::addSubvol( const string& path )
    {
    y2mil( "path:\"" << path << "\"" );
    Subvolume v( path );
    if( !contains( subvol, v ))
	subvol.push_back( v );
    else
	y2war( "subvolume " << v << " already extsist!" );
    }

void Btrfs::getInfo( BtrfsInfo& tinfo ) const
    {
    Volume::getInfo(info.v);
    info.devices = devices;
    info.subvol = list<SubvolInfo>(subvol.begin(), subvol.end());
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Btrfs& v )
    {
    s << "Btrfs " << dynamic_cast<const Volume&>(v);
    s << " devices:" << v.devices;
    if( !v.subvol.empty() )
	s << " subvol:" << v.subvol;
    return( s );
    }


bool Btrfs::equalContent( const Btrfs& rhs ) const
    {
    return( Volume::equalContent(rhs) && devices==rhs.devices &&
            subvol==rhs.subvol );
    }


void Btrfs::logDifference(std::ostream& log, const Btrfs& rhs) const
    {
    Volume::logDifference(log, rhs);
    }

void Btrfs::saveData(xmlNode* node) const
    {
    Volume::saveData(node);
    setChildValue(node, "devices", devices);
    if (!subvol.empty())
	setChildValue(node, "subvolume", subvol);
    }

}
