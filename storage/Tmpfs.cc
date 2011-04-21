/*
 * Copyright (c) [2004-2011] Novell, Inc.
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

#include "storage/Tmpfs.h"
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

Tmpfs::Tmpfs(const TmpfsCo& d, const string& m, bool mounted ) : Volume(d,"none","none")
    {
    y2mil("constructed tmpfs vol mount:" << m << " mounted:" << mounted );
    if( mounted )
	setMount(m);
    else
	mp = m;
    setFs(TMPFS);
    is_mounted = mounted;
    }

Tmpfs::Tmpfs(const TmpfsCo& d, const xmlNode* node ) : Volume(d, node)
    {
    }


Tmpfs::Tmpfs(const TmpfsCo& c, const Tmpfs& v) : Volume(c, v)
    {
    y2deb("copy-constructed Tmpfs from " << v.mp);
    }

Tmpfs::~Tmpfs()
    {
    y2deb("destructed Tmpfs for mount:" << mp);
    }

Text Tmpfs::removeText(bool doing) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by path name e.g /var/run 
	txt = sformat( _("Removing Tmpfs volume from mount point %1$s"), orig_mp.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by path name e.g /var/run
	txt = sformat( _("Remove Tmpfs volume from mount point %1$s"), orig_mp.c_str() );
	}
    return( txt );
    }

Text Tmpfs::mountText( bool doing ) const
    {
    Text txt;
    if( doing )
        {
	if( !mp.empty() )
	    {
	    // displayed text during action, %1$s is replaced by mount point e.g. /home
	    txt = sformat(_("Mounting tmpfs to %1$s"), mp.c_str());
	    }
	else
	    {
	    // displayed text during action
	    txt = _("Unmounting tmpfs");
	    }
        }
    else
        {
	if( !orig_mp.empty() && !mp.empty() && 
	    (!getStorage()->instsys()||mp!=orig_mp||mp!="swap") )
	    {
	    // displayed text before action, %1$s is replaced by mount point e.g. /home
	    txt = sformat(_("Change mount point of tmpfs to %1$s"), mp.c_str());
	    }
	else if( !mp.empty() )
	    {
	    // displayed text before action, %1$s is replaced by mount point e.g. /home
	    txt = sformat(_("Set mount point of tmpfs to %1$s"), mp.c_str());
	    }
	else if( !orig_mp.empty() )
	    {
	    string fn = "/etc/fstab";
	    if( inCrypttab() )
		fn = "/etc/crypttab";
	    if( inCryptotab() )
		fn = "/etc/cryptotab";
	    // displayed text before action, %1$s is replaced by pathname e.g. /etc/fstab
	    txt = sformat(_("Remove tmpfs from %1$s"), fn.c_str());
	    }
        }
    return( txt );
    }

void Tmpfs::getInfo( TmpfsInfo& tinfo ) const
    {
    Volume::getInfo(info.v);
    tinfo = info;
    }

std::ostream& operator<< (std::ostream& s, const Tmpfs& v )
    {
    s << "Tmpfs " << dynamic_cast<const Volume&>(v);
    return( s );
    }


bool Tmpfs::equalContent( const Tmpfs& rhs ) const
    {
    return( Volume::equalContent(rhs) );
    }


void Tmpfs::logDifference(std::ostream& log, const Tmpfs& rhs) const
    {
    Volume::logDifference(log, rhs);
    }

void Tmpfs::saveData(xmlNode* node) const
    {
    Volume::saveData(node);
    }

}
