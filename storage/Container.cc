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


#include <ostream>
#include <algorithm>
#include <list>

#include "storage/Container.h"
#include "storage/SystemCmd.h"
#include "storage/Storage.h"
#include "storage/AppUtil.h"
#include "storage/Device.h"


namespace storage
{
    using namespace std;


    /* This is our constructor for Container used for containers created via
       the storage interface. This is recognisable by the fact that it does
       not have a parameter of type SystemInfo or xmlNode. */
    Container::Container(Storage* s, const string& name, const string& device, CType t)
	: Device(name, device), sto(s), typ(t), ronly(false)
    {
	y2deb("constructed Container " << dev);

	assert(!nm.empty() && !dev.empty());
    }


    /* This is our constructor for Container used during detection. This is
       recognisable by the fact that it has an parameter of type
       SystemInfo. */
    Container::Container(Storage* s, const string& name, const string& device, CType t,
			 SystemInfo& systeminfo)
	: Device(name, device, systeminfo), sto(s), typ(t), ronly(false)
    {
	y2deb("constructed Container " << dev);

	assert(!nm.empty() && !dev.empty());
	assert(!s->testmode());
    }


    /* This is our constructor for Container used during fake detection in
       testmode. This is recognisable by the fact that it has an parameter of
       type xmlNode. */
    Container::Container(Storage* s, CType t, const xmlNode* node)
	: Device(node), sto(s), typ(t), ronly(false)
    {
	getChildValue(node, "readonly", ronly);

	y2deb("constructed Container " << dev);

	assert(s->testmode());
    }


    /* This is our copy-constructor for Container. Every class derived from
       Container needs an equivalent one. */
    Container::Container(const Container& c)
	: Device(c), sto(c.sto), typ(c.typ), ronly(c.ronly)
    {
	y2deb("copy-constructed Container " << dev);
    }


    Container::~Container()
    {
	clearPointerList(vols);
	y2deb("destructed Container " << dev);
    }


    void
    Container::saveData(xmlNode* node) const
    {
	Device::saveData(node);

	if (ronly)
	    setChildValue(node, "readonly", ronly);
    }


    bool
    Container::isEmpty() const
    {
	ConstVolPair p = volPair(Volume::notDeleted);
	return p.empty();
    }

bool Container::isPartitionable() const
    {
    return( typ==DISK || typ==DMRAID || typ==DMMULTIPATH || typ==MDPART );
    }

bool Container::isDeviceUsable() const
    {
    return( typ==DISK || typ==DMRAID || typ==DMMULTIPATH || typ==MDPART );
    }

    bool Container::stageDecrease(const Volume& v)
    {
	return v.deleted() || v.needShrink();
    }

    bool Container::stageIncrease(const Volume& v)
    {
	return v.created() || v.needExtend() || v.needCrsetup();
    }

    bool Container::stageFormat(const Volume& v)
    {
	return v.getFormat() || v.needLabel();
    }

    bool Container::stageMount(const Volume& v)
    {
	return v.needRemount() || v.needFstabUpdate();
    }


void
Container::getToCommit(CommitStage stage, list<const Container*>& col, list<const Volume*>& vol) const
    {
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    switch( stage )
	{
	case DECREASE:
	    {
	    ConstVolPair p = volPair( stageDecrease );
	    for( ConstVolIterator i=p.begin(); i!=p.end(); ++i )
		vol.push_back( &(*i) );
	    if( deleted() )
		col.push_back( this );
	    }
	    break;
	case INCREASE:
	    {
	    ConstVolPair p = volPair(stageIncrease);
	    for( ConstVolIterator i=p.begin(); i!=p.end(); ++i )
		vol.push_back( &(*i) );
	    if( created() )
		col.push_back( this );
	    }
	    break;
	case FORMAT:
	    {
	    ConstVolPair p = volPair( stageFormat );
	    for( ConstVolIterator i=p.begin(); i!=p.end(); ++i )
		vol.push_back( &(*i) );
	    }
	    break;
	case MOUNT:
	    {
	    ConstVolPair p = volPair( stageMount );
	    for( ConstVolIterator i=p.begin(); i!=p.end(); ++i )
		vol.push_back( &(*i) );
	    }
	    break;
	case SUBVOL:
	    break;
	}
    if( col.size()!=oco || vol.size()!=ovo )
	y2mil("stage:" << stage << " col:" << col.size() << " vol:" << vol.size());
}


int Container::commitChanges( CommitStage stage, Volume* vol )
    {
    y2mil("name:" << name() << " stage:" << stage << " vol:" << vol->name());
    y2mil("vol:" << *vol);
    int ret = 0;
    switch( stage )
	{
	case DECREASE:
	    if( vol->deleted() )
	    {
		if (vol->getEncryption() != ENC_NONE)
		    vol->doFstabUpdate();
		ret = doRemove( vol );
	    }
	    else if( vol->needShrink() )
		ret = doResize( vol );
	    break;

	case INCREASE:
	    if( vol->created() )
		ret = doCreate( vol );
	    else if( vol->needExtend() )
		ret = doResize( vol );
	    if (vol->needCrsetup(false))
		ret = vol->doCrsetup();
	    break;

	case FORMAT:
	    if (vol->needCrsetup(true))
		ret = vol->doCrsetup();
	    if( ret==0 && vol->getFormat() )
		ret = vol->doFormat();
	    if( ret==0 && vol->needLabel() )
		ret = vol->doSetLabel();
	    break;

	case MOUNT:
	    if( vol->needRemount() )
		{
		if (vol->needCrsetup(true))
		    vol->doCrsetup();
		ret = vol->doMount();
		}
	    if( ret==0 && vol->needFstabUpdate() )
		{
		ret = vol->doFstabUpdate();
		if( ret==0 )
		    vol->fstabUpdateDone();
		}
	    break;
	case SUBVOL:
	    break;

	default:
	    ret = VOLUME_COMMIT_UNKNOWN_STAGE;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Container::commitChanges( CommitStage stage )
    {
    y2mil("name " << name() << " stage " << stage);
    int ret = CONTAINER_INVALID_VIRTUAL_CALL;
    y2mil("ret:" << ret);
    return( ret );
    }


void
Container::getCommitActions(list<commitAction>& l) const
    {
    ConstVolPair p = volPair();
    for( ConstVolIterator i=p.begin(); i!=p.end(); ++i )
	if( !i->isSilent() )
	    i->getCommitActions( l );
    }


Text Container::createText( bool doing ) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	txt = sformat( _("Creating %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	txt = sformat( _("Create %1$s"), dev.c_str() );
	}
    return( txt );
    }

Text Container::removeText( bool doing ) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	txt = sformat( _("Removing %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	txt = sformat( _("Remove %1$s"), dev.c_str() );
	}
    return( txt );
    }

int Container::doCreate( Volume * v )
{
    y2war("invalid container:" << toString(typ) << " name:" << name());
    return( CONTAINER_INVALID_VIRTUAL_CALL );
}

int Container::doRemove( Volume * v )
{
    y2war("invalid container:" << toString(typ) << " name:" << name());
    return( CONTAINER_INVALID_VIRTUAL_CALL );
}

int Container::doResize( Volume * v )
{
    y2war("invalid container:" << toString(typ) << " name:" << name());
    return( CONTAINER_INVALID_VIRTUAL_CALL );
}

int Container::removeVolume( Volume * v )
{
    y2war("invalid container:" << toString(typ) << " name:" << name());
    return( CONTAINER_INVALID_VIRTUAL_CALL );
}

int Container::resizeVolume( Volume * v, unsigned long long )
{
    return( VOLUME_RESIZE_UNSUPPORTED_BY_CONTAINER );
}


    void
    Container::addToList(Volume* e)
    {
	pointerIntoSortedList<Volume>(vols, e);
    }


bool Container::removeFromList( Volume* e )
    {
    bool ret=false;
    PlainIterator i = vols.begin();
    while( i!=vols.end() && *i!=e )
	++i;
    if( i!=vols.end() )
	{
	delete *i;
	vols.erase( i );
	ret = true;
	}
    y2mil("P:" << e << " ret:" << ret);
    return( ret );
    }

void Container::setExtError( const string& txt ) const
    {
    if( sto )
	sto->setExtError(txt);
    }

void Container::setExtError( const SystemCmd& cmd, bool serr ) const
    {
    string s = boost::join(serr ? cmd.stderr() : cmd.stdout(), "\n");
    if (!s.empty())
	sto->setExtError( cmd.cmd() + ":\n" + s );
    else
	y2war("called with empty " << (serr?"stderr":"stdout") << " cmd:" << cmd.cmd());
    }

bool Container::findVolume( const string& device, Volume*& vol )
    {
    string d = normalizeDevice( device );
    VolPair p = volPair( Volume::notDeleted );
    VolIterator v = p.begin();
    const list<string>& al( v->altNames() );
    while( v!=p.end() && v->device()!=d &&
           find( al.begin(), al.end(), d )==al.end() )
	{
	++v;
        }
    if( v!=p.end() )
	vol = &(*v);
    return( v!=p.end() );
    }


void Container::getInfo(storage::ContainerInfo& tinfo) const
{
    info.type = type();
    info.name = name();
    info.device = device();

    info.udevPath = udevPath();
    info.udevId = boost::join(udevId(), " ");

    info.usedBy = list<UsedByInfo>(uby.begin(), uby.end());

    if (uby.empty())
    {
	info.usedByType = UB_NONE;
	info.usedByDevice = "";
    }
    else
    {
	info.usedByType = uby.front().type();
	info.usedByDevice = uby.front().device();
    }

    info.readonly = readonly();
    tinfo = info;
}


std::ostream& operator<< ( std::ostream& s, const Container &c )
    {
    s << "CType:" << toString(c.typ)
      << " " << dynamic_cast<const Device&>(c);
    if( c.ronly )
      s << " readonly";
    if (!c.uby.empty())
	s << " usedby:" << c.uby;
    if (!c.alt_names.empty())
	s << " alt_names:" << c.alt_names;
    return s;
    }


    void
    Container::logDifference(std::ostream& log, const Container& rhs) const
    {
	Device::logDifference(log, rhs);

	logDiffEnum(log, "type", typ, rhs.typ);

	logDiff(log, "readonly", ronly, rhs.ronly);
	logDiff(log, "silent", silent, rhs.silent);

	logDiff(log, "usedby", uby, rhs.uby);
    }


bool Container::equalContent( const Container& rhs ) const
    {
    return( typ==rhs.typ && nm==rhs.nm && dev==rhs.dev && del==rhs.del &&
            create==rhs.create && ronly==rhs.ronly && silent==rhs.silent &&
	    uby==rhs.uby );
    }


    bool
    Container::compareContainer(const Container& rhs, bool verbose) const
    {
	assert(typ != CUNKNOWN);
	if (typ == CUNKNOWN)
	{
	    y2err("unknown container type lhs:" << *this);
	    return false;
	}

	assert(typ == rhs.typ);
	if (typ != rhs.typ)
	{
	    y2err("comparing different container types lhs:" << *this << " rhs:" << rhs);
	    return false;
	}

	bool ret = equalContent(rhs);
	if (!ret && verbose)
	{
	    std::ostringstream log;
	    prepareLogStream(log);
	    logDifferenceWithVolumes(log, rhs);
	    y2mil(log.str());
	}

	return ret;
    }

}
