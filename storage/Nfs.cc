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


Nfs::Nfs( const NfsCo& d, const string& NfsDev ) :
    Volume( d, 0, 0 )
    {
    y2deb("constructed nfs dev:" << NfsDev);
    if( d.type() != NFSC )
	y2err("constructed nfs with wrong container");
    dev = canonicalName(NfsDev);
    if( dev != NfsDev )
	alt_names.push_back( NfsDev );
    init();
    }

Nfs::~Nfs()
    {
    y2deb("destructed nfs " << dev);
    }

string Nfs::removeText( bool doing ) const
    {
    string txt;
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
Nfs::init()
    {
    numeric = false;
    nm = dev;
    setFs(NFS);
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

Nfs& Nfs::operator= ( const Nfs& rhs )
    {
    y2deb("operator= from " << rhs.nm);
    *((Volume*)this) = rhs;
    return( *this );
    }

Nfs::Nfs( const NfsCo& d, const Nfs& rhs ) : Volume(d)
    {
    y2deb("constructed nfs by copy constructor from " << rhs.nm);
    *this = rhs;
    }

}
