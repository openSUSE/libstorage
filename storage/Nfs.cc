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


#include <sstream>

#include "storage/Nfs.h"
#include "storage/StorageTypes.h"
#include "storage/Container.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"


namespace storage
{
    using namespace std;


    Nfs::Nfs(const NfsCo& c, const string& NfsDev, bool nfs4)
	: Volume(c, 0, 0)
    {
	y2mil("constructed Nfs dev:" << NfsDev << " nfs4:" << nfs4);
    if( c.type() != NFSC )
	y2err("constructed nfs with wrong container");
    dev = canonicalName(NfsDev);
    if( dev != NfsDev )
	alt_names.push_back( NfsDev );
    init(nfs4);
    }


    Nfs::Nfs(const NfsCo& c, const Nfs& v)
	: Volume(c, v)
    {
	y2deb("copy-constructed Nfs from " << v.dev);
    }


    Nfs::~Nfs()
    {
	y2deb("destructed Nfs " << dev);
    }


Text Nfs::removeText( bool doing ) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by volume name e.g. hilbert:/work
	txt = sformat( _("Removing nfs volume %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by volume name e.g. hilbert:/work
	txt = sformat( _("Remove nfs volume %1$s"), dev.c_str() );
	}
    return( txt );
    }

void
Nfs::init(bool nfs4)
    {
    numeric = false;
    nm = dev;
    setFs(nfs4 ? NFS4 : NFS);
    }


    string
    Nfs::canonicalName(const string& d)
    {
	string dev = boost::replace_all_copy(d, "//", "/");
	if (!dev.empty() && *dev.rbegin() == '/')
	    dev.erase(dev.size() - 1);
	if (dev != d)
	    y2mil("dev:" << dev << " d:" << d);
	return dev;
    }


void Nfs::getInfo( NfsInfo& tinfo ) const
    {
    ((Volume*)this)->getInfo( info.v );
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Nfs& l )
    {
    s << "Nfs " << *(Volume*)&l;
    return( s );
    }


bool Nfs::equalContent( const Nfs& rhs ) const
    {
    return( Volume::equalContent(rhs) );
    }

void Nfs::logDifference( const Nfs& rhs ) const
{
    string log = Volume::logDifference(rhs);
    y2mil(log);
}

}
