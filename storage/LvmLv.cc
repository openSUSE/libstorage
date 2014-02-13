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
		 unsigned long le, const string& uuid, const string& stat,
		 SystemInfo& systeminfo)
	: Dm(c, name, device, makeDmTableName(c.name(), name)), origin(origi), 
          chunk_size(0), pool(false)
{
    Dm::init();
    setUdevData(systeminfo);
    setUuid( uuid );
    setStatus( stat );
    setLe( le );
    calcSize();
    getTableInfo();

    y2deb("constructed lvm lv dev:" << dev << " vg:" << cont->name() << " origin:" << origin);
}


    LvmLv::LvmLv(const LvmVg& c, const string& name, const string& device, const string& origi,
		 unsigned long le, unsigned str)
	: Dm(c, name, device, makeDmTableName(c.name(), name)), origin(origi),
          chunk_size(0), pool(false)
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
        chunk_size=0;
        pool = false;
        getChildValue(node, "pool", pool);
        getChildValue(node, "used_pool", used_pool);
        getChildValue(node, "origin", origin);
        getChildValue(node, "chunk_size", chunk_size);
	y2deb("constructed LvmLv " << dev);
    }


    LvmLv::LvmLv(const LvmVg& c, const LvmLv& v)
	: Dm(c, v), origin(v.origin), vol_uuid(v.vol_uuid), status(v.status), 
	  used_pool(v.used_pool),
          chunk_size(v.chunk_size), pool(v.pool)
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
        if( !used_pool.empty() )
            setChildValue(node, "used_pool", used_pool);
        if( !origin.empty() )
            setChildValue(node, "origin", origin);
        setChildValue(node, "pool", pool);
        setChildValue(node, "chunk_size", chunk_size);
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


    list<string>
    LvmLv::getUsing() const
    {
	list<string> ret;
	ret.push_back(cont->device());
	return ret;
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
	txt += _("(Progress bar will not move. May take very long. DO NOT ABORT!)");
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

int LvmLv::setFormat( bool val, storage::FsType new_fs )
    {
    int ret = 0;
    y2mil("device:" << dev << " val:" << val << " fs:" << toString(new_fs));
    if( isPool() && val )
	ret = LVM_LV_POOL_NO_FORMAT;
    else
	ret = Volume::setFormat( val, new_fs );
    y2mil("ret:" << ret);
    return( ret );
    }

int LvmLv::changeMount( const string& val )
    {
    int ret = 0;
    y2mil("device:" << dev << " val:" << val);
    if( isPool() && !val.empty() )
	ret = LVM_LV_POOL_NO_MOUNT;
    else
	ret = Volume::changeMount( val );
    y2mil("ret:" << ret);
    return( ret );
    }


void LvmLv::getInfo( LvmLvInfo& info ) const
    {
    Volume::getInfo(info.v);
    //info.v.sizeK = isThin() ? size_k : (num_le * pec()->peSize());
    info.stripes = stripe;
    info.stripeSizeK = stripe_size;
    info.uuid = vol_uuid;
    info.status = status;
    info.dm_table = tname;
    info.dm_target = target;
    info.origin = origin;
    info.used_pool = used_pool;
    info.pool = pool;
    }

bool LvmLv::operator< ( const LvmLv& rhs ) const
    {
    if( pool!=rhs.pool )
        return pool;
    else if( isSnapshot()!=rhs.isSnapshot() )
        return !isSnapshot();
    else if( stripe!=rhs.stripe )
        return( stripe>rhs.stripe );
    else if( nm!=rhs.nm )
        return( nm<rhs.nm );
    else
        return( !del );
    }

std::ostream& operator<< (std::ostream& s, const LvmLv &p )
    {
    s << dynamic_cast<const Dm&>(p);
    if( !p.vol_uuid.empty() )
      s << " UUID:" << p.vol_uuid;
    if( !p.status.empty() )
      s << " " << p.status;
    if( !p.origin.empty() )
      s << " " << p.origin;
    if( !p.used_pool.empty() )
      s << " pool:" << p.used_pool;
    if( p.pool )
      s << " pool";
    if( p.chunk_size!=0 )
      s << " chunk:" << p.chunk_size;
    return( s );
    }


bool LvmLv::equalContent( const LvmLv& rhs ) const
    {
    return Dm::equalContent(rhs) &&
            vol_uuid==rhs.vol_uuid && status==rhs.status && 
            pool==rhs.pool && chunk_size==rhs.chunk_size && 
            used_pool==rhs.used_pool && origin==rhs.origin;
    }


    void
    LvmLv::logDifference(std::ostream& log, const LvmLv& rhs) const
    {
	Dm::logDifference(log, rhs);

	logDiff(log, "vol_uuid", vol_uuid, rhs.vol_uuid);
	logDiff(log, "status", status, rhs.status);
	logDiff(log, "origin", origin, rhs.origin);
	logDiff(log, "pool", pool, rhs.pool);
	logDiff(log, "chunk_size", chunk_size, rhs.chunk_size);
	logDiff(log, "used_pool", used_pool, rhs.used_pool);
    }

}
