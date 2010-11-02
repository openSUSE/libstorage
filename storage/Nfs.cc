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
	: Volume(c, canonicalName(NfsDev), canonicalName(NfsDev))
    {
	assert(c.type() == NFSC);

	if (dev != NfsDev)
	    alt_names.push_back(NfsDev);

	setFs(nfs4 ? NFS4 : NFS);

	y2deb("constructed Nfs " << dev << " nfs4:" << nfs4);
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


    string
    Nfs::canonicalName(const string& d)
    {
	string dev = boost::replace_all_copy(d, "//", "/");
	if (dev.size() > 2 && dev[dev.size() - 2] != ':' && dev[dev.size() - 1] == '/')
	    dev.erase(dev.size() - 1);
	if (dev != d)
	    y2mil("old:" << d << " new:" << dev);
	return dev;
    }


void Nfs::getInfo( NfsInfo& tinfo ) const
    {
    Volume::getInfo(info.v);
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Nfs& l )
    {
    s << "Nfs " << dynamic_cast<const Volume&>(l);
    return( s );
    }


bool Nfs::equalContent( const Nfs& rhs ) const
    {
    return( Volume::equalContent(rhs) );
    }


    void
    Nfs::logDifference(std::ostream& log, const Nfs& rhs) const
    {
	Volume::logDifference(log, rhs);
    }

}
