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
#include <boost/algorithm/string.hpp>

#include "storage/LvmLv.h"
#include "storage/LvmVg.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    LvmLv::LvmLv(const LvmVg& c, const string& name, const string& device, const string& origi,
		 unsigned long le, const string& uuid, const string& stat, const string& alloc)
	: Dm(c, name, device, makeDmTableName(c.name(), name)), origin(origi)
{
	Dm::init();
    setUuid( uuid );
    setStatus( stat );
    setAlloc( alloc );
    setLe( le );
    calcSize();
    getTableInfo();

    y2deb("constructed lvm lv dev:" << dev << " vg:" << cont->name() << " origin:" << origin);
}


    LvmLv::LvmLv(const LvmVg& c, const string& name, const string& device, const string& origi,
		 unsigned long le, unsigned str)
	: Dm(c, name, device, makeDmTableName(c.name(), name)), origin(origi)
{
	Dm::init();
    setLe( le );
    calcSize();
    stripe = str;
    fs = detected_fs = FSNONE;
    alt_names.push_back("/dev/mapper/" + getTableName());

    y2deb("constructed lvm lv dev:" << dev << " vg:" << cont->name() << " origin:" << origin);
}


    LvmLv::LvmLv(const LvmVg& c, const xmlNode* node)
	: Dm(c, node)
    {
	assert(!numeric);
	assert(num == 0);

	y2deb("constructed LvmLv " << dev);
    }


    LvmLv::LvmLv(const LvmVg& c, const LvmLv& v)
	: Dm(c, v), origin(v.origin), vol_uuid(v.vol_uuid), status(v.status),
	  allocation(v.allocation)
    {
	y2deb("copy-constructed LvmLv " << dev);
    }


    LvmLv::~LvmLv()
    {
	y2deb("destructed LvmLv " << dev);
    }


    void
    LvmLv::saveData(xmlNode* node) const
    {
	Dm::saveData(node);
    }


const LvmVg* LvmLv::vg() const
{
    return(dynamic_cast<const LvmVg* const>(cont));
}


    string
    LvmLv::makeDmTableName(const string& vg_name, const string& lv_name)
    {
	return boost::replace_all_copy(vg_name, "-", "--") + "-" +
	    boost::replace_all_copy(lv_name, "-", "--");
    }


void LvmLv::calcSize()
{
    if (!isSnapshot())
    {
	Dm::calcSize();
    }
    else
    {
	LvmVg::ConstLvmLvPair p = vg()->LvmVg::lvmLvPair(LvmLv::notDeleted);
	LvmVg::ConstLvmLvIter i = p.begin();
	while( i!=p.end() && i->name()!=origin )
	    ++i;
	if (i != p.end())
	{
	    setSize(i->sizeK());
	}
	else
	{
	    setSize(0);
	    y2err("not found " << origin);
	}
    }
}


bool
LvmLv::hasSnapshots() const
{
    LvmVg::ConstLvmLvPair p = vg()->LvmVg::lvmLvPair(LvmLv::notDeleted);
    LvmVg::ConstLvmLvIter i = p.begin();
    while( i!=p.end() && i->getOrigin()!=name() )
	++i;
    return i != p.end();
}


void
LvmLv::getState(LvmLvSnapshotStateInfo& info)
{
    SystemCmd cmd(LVSBIN " --options lv_name,lv_attr,snap_percent " + quote(cont->name()));

    if (cmd.retcode() == 0 && cmd.numLines() > 0)
    {
	for (unsigned int l = 1; l < cmd.numLines(); l++)
	{
	    string line = cmd.getLine(l);
	    
	    if (extractNthWord(0, line) == name())
	    {
		string attr = extractNthWord(1, line);
		info.active = attr.size() >= 6 && attr[4] == 'a';
		
		string percent = extractNthWord(2, line);
		percent >> info.allocated;
	    }
	}
    }
}


Text LvmLv::removeText( bool doing ) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	txt = sformat( _("Deleting logical volume %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete logical volume %1$s (%2$s)"), dev.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

Text LvmLv::createText( bool doing ) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	txt = sformat( _("Creating logical volume %1$s"), dev.c_str() );
	}
    else
	{
	if( mp=="swap" )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create swap logical volume %1$s (%2$s)"),
	                   dev.c_str(), sizeString().c_str() );
	    }
	else if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create encrypted logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create logical volume %1$s (%2$s)"),
			   dev.c_str(), sizeString().c_str() );
	    }
	}
    return( txt );
    }

Text LvmLv::formatText( bool doing ) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting logical volume %1$s (%2$s) with %3$s"),
		       dev.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( !mp.empty() )
	    {
	    if( mp=="swap" )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Format logical volume %1$s (%2$s) for swap"),
			       dev.c_str(), sizeString().c_str() );
		}
	    else if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format logical volume %1$s (%2$s) with %3$s"),
			   dev.c_str(), sizeString().c_str(), 
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

Text LvmLv::resizeText( bool doing ) const
    {
    Text txt;
    if( doing )
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrinking logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extending logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	txt += Text(" ", " ");
	// text displayed during action
	txt += _("(progress bar might not move)");
        }
    else
        {
	if( needShrink() )
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrink logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extend logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );

        }
    return( txt );
    }

void LvmLv::getInfo( LvmLvInfo& tinfo ) const
    {
    Volume::getInfo(info.v);
    info.stripes = stripe;
    info.stripeSizeK = stripe_size;
    info.uuid = vol_uuid;
    info.status = status;
    info.allocation = allocation;
    info.dm_table = tname;
    info.dm_target = target;

    info.sizeK = num_le * pec()->peSize();
    info.origin = origin;

    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const LvmLv &p )
    {
    s << dynamic_cast<const Dm&>(p);
    if( !p.vol_uuid.empty() )
      s << " UUID:" << p.vol_uuid;
    if( !p.status.empty() )
      s << " " << p.status;
    if( !p.allocation.empty() )
      s << " " << p.allocation;
    return( s );
    }


bool LvmLv::equalContent( const LvmLv& rhs ) const
    {
    return( Dm::equalContent(rhs) &&
            vol_uuid==rhs.vol_uuid && status==rhs.status && 
            allocation==rhs.allocation );
    }


    void
    LvmLv::logDifference(std::ostream& log, const LvmLv& rhs) const
    {
	Dm::logDifference(log, rhs);

	logDiff(log, "vol_uuid", vol_uuid, rhs.vol_uuid);
	logDiff(log, "status", status, rhs.status);
	logDiff(log, "alloc", allocation, rhs.allocation);
    }

}
