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


#include <ostream>
#include <algorithm>
#include <list>

#include "storage/Container.h"
#include "storage/SystemCmd.h"
#include "storage/Storage.h"
#include "storage/AppUtil.h"
#include "storage/Device.h"
#include "storage/AsciiFile.h"


namespace storage
{
    using namespace std;


    Container::Container(Storage* s, const string& name, CType t)
	: Device(name, "/dev/" + name), sto(s), typ(t), del(false),
	  create(false), silent(false), ronly(false)
    {
	y2deb("constructed Container " << dev);
    }


    Container::Container(Storage* s, CType t, const AsciiFile& file)
	: Device(file), sto(s), typ(t), del(false), create(false), silent(false),
	  ronly(false)
    {
	const vector<string>& lines = file.lines();
	vector<string>::const_iterator it;

	if ((it = find_if(lines, string_starts_with("Readonly:"))) != lines.end())
	    extractNthWord(1, *it) >> ronly;

	y2deb("constructed Container " << dev << " from file " << file.name());
    }


    Container::Container(const Container& c)
	: Device(c), sto(c.sto), typ(c.typ), del(c.del), create(c.create),
	  silent(c.silent), ronly(c.ronly)
    {
	y2deb("copy-constructed Container from " << c.dev);
    }


    Container::~Container()
    {
	clearPointerList(vols);
	y2deb("destructed Container " << dev);
    }


    bool
    Container::isEmpty() const
    {
	ConstVolPair p = volPair(Volume::notDeleted);
	return p.empty();
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
	    if (vol->needCrsetup())
		ret = vol->doCrsetup();
	    break;

	case FORMAT:
            //if( vol->needCrsetup() )
	//	ret = vol->doCrsetup();
	    if( ret==0 && vol->getFormat() )
		ret = vol->doFormat();
	    if( ret==0 && vol->needLabel() )
		ret = vol->doSetLabel();
	    break;

	case MOUNT:
	    if( vol->needRemount() )
		ret = vol->doMount();
	    if( ret==0 && vol->needFstabUpdate() )
		{
		ret = vol->doFstabUpdate();
		if( ret==0 )
		    vol->fstabUpdateDone();
		}
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
	if( !i->silent() )
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
    y2war("invalid container:" << type_names[typ] << " name:" << name());
    return( CONTAINER_INVALID_VIRTUAL_CALL );
}

int Container::doRemove( Volume * v )
{
    y2war("invalid container:" << type_names[typ] << " name:" << name());
    return( CONTAINER_INVALID_VIRTUAL_CALL );
}

int Container::doResize( Volume * v )
{
    y2war("invalid container:" << type_names[typ] << " name:" << name());
    return( CONTAINER_INVALID_VIRTUAL_CALL );
}

int Container::removeVolume( Volume * v )
{
    y2war("invalid container:" << type_names[typ] << " name:" << name());
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
    const string& s = boost::join(serr ? cmd.stderr() : cmd.stdout(), "\n");
    if( s.size()>0 )
	{
	sto->setExtError( cmd.cmd() + ":\n" + s );
	}
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
    s << "Type:" << Container::type_names[c.typ]
        << " Name:" << c.nm
        << " Device:" << c.dev
        << " Vcnt:" << c.vols.size();
    if( c.del )
        s << " deleted";
    if( c.create )
        s << " created";
    if( c.ronly )
      s << " readonly";
    if( c.silent )
      s << " silent";
    if (!c.uby.empty())
	s << " usedby:" << c.uby;
    return s;
    }


void
Container::logDifference( const Container& c ) const
{
    y2mil(getDiffString(c));
}
    
string
Container::getDiffString( const Container& c ) const
    {
    string ret = "Name:" + nm;
    if( nm!=c.nm )
	ret += "-->"+c.nm;
    if( typ!=c.typ )
	ret += " Type:" + Container::type_names[typ] + "-->" +
	       Container::type_names[c.typ];
    if( dev!=c.dev )
	ret += " Device:" + dev + "-->" + c.dev;
    if( del!=c.del )
	{
	if( c.del )
	    ret += " -->deleted";
	else
	    ret += " deleted-->";
	}
    if( create!=c.create )
	{
	if( c.create )
	    ret += " -->created";
	else
	    ret += " created-->";
	}
    if( ronly!=c.ronly )
	{
	if( c.ronly )
	    ret += " -->readonly";
	else
	    ret += " readonly-->";
	}
    if( silent!=c.silent )
	{
	if( c.silent )
	    ret += " -->silent";
	else
	    ret += " silent-->";
	}
    if( uby!=c.uby )
	{
	std::ostringstream b;
	classic(b);
	b << " usedby:" << uby << "-->" << c.uby;
	ret += b.str();
	}
    return( ret );
    }

bool Container::equalContent( const Container& rhs ) const
    {
    return( typ==rhs.typ && nm==rhs.nm && dev==rhs.dev && del==rhs.del &&
            create==rhs.create && ronly==rhs.ronly && silent==rhs.silent &&
	    uby==rhs.uby );
    }

bool Container::compareContainer( const Container* c, bool verbose ) const
    {
    bool ret = typ == c->typ;
    if( !ret )
	{
	if( verbose )
	    y2mil(getDiffString( *c ));
	}
    else
	{
	ret = equalContent( *c );
	if( !ret && verbose )
	    logDifference( *c );
	if( typ==COTYPE_LAST_ENTRY || typ==CUNKNOWN )
	    y2err( "Unknown Container:" << *c ); 
	}
    return( ret );
    }


const string Container::type_names[] = { "UNKNOWN", "DISK", "MD", "LOOP", "LVM", 
					 "DM", "DMRAID", "NFS", "DMMULTIPATH" };

}
