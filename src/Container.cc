#include <iostream>
#include <algorithm>
#include <list>

#include <ycp/y2log.h>

#include "y2storage/Container.h"
#include "y2storage/Storage.h"
#include "y2storage/Md.h"
#include "y2storage/Loop.h"
#include "y2storage/AppUtil.h"

Container::Container( Storage * const s, const string& Name, CType t ) :
    sto(s), nm(Name)
    {
    del = silent = rdonly = false;
    dev = "/dev/" + nm;
    typ = t;
    y2milestone( "constructed cont %s", nm.c_str() );
    }

Container::~Container()
    {
    for( PlainIterator i=begin(); i!=end(); i++ )
	{
	delete( *i );
	}
    y2milestone( "destructed cont %s", dev.c_str() );
    }

static bool stageDecrease( const Volume& v ) { return( v.deleted()); }
static bool stageCreate( const Volume& v ) { return( v.created()); }
static bool stageFormat( const Volume& v ) 
    { return( v.getFormat()||v.needLosetup()||v.needLabel()); }
static bool stageMount( const Volume& v ) 
    { return( v.needRemount()||v.needFstabUpdate()); }

int Container::commitChanges( CommitStage stage )
    {
    y2milestone( "name %s stage %d", name().c_str(), stage );
    int ret = 0;
    switch( stage )
	{
	case DECREASE:
	    {
	    VolPair p = volPair( stageDecrease );
	    if( !deleted() && !p.empty() )
		{
		list<VolIterator> l;
		for( VolIterator i=p.begin(); i!=p.end(); ++i )
		    l.push_front( i );
		list<VolIterator>::const_iterator i=l.begin();
		while( ret==0 && i!=l.end() )
		    {
		    ret = doRemove( &(**i) );
		    ++i;
		    }
		}
	    }
	    break;
	case INCREASE:
	    {
	    VolPair p = volPair( stageCreate );
	    VolIterator i=p.begin();
	    while( ret==0 && i!=p.end() )
		{
		ret = doCreate( &(*i) );
		++i;
		}
	    }
	    break;
	case FORMAT:
	    {
	    VolPair p = volPair( stageFormat );
	    VolIterator i=p.begin();
	    while( ret==0 && i!=p.end() )
		{
		if( i->needLosetup() )
		    ret = i->doLosetup();
		if( ret==0 && i->getFormat() )
		    ret = i->doFormat();
		if( ret==0 && i->needLabel() )
		    ret = i->doSetLabel();
		++i;
		}
	    }
	    break;
	case MOUNT:
	    {
	    VolPair p = volPair( stageMount );
	    VolIterator i=p.begin();
	    while( ret==0 && i!=p.end() )
		{
		if( i->needRemount() )
		    ret = i->doMount();
		if( ret==0 && i->needFstabUpdate() )
		    ret = i->doFstabUpdate();
		++i;
		}
	    }
	    break;
	default:
	    ret = VOLUME_COMMIT_UNKNOWN_STAGE;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void Container::getCommitActions( list<commitAction*>& l ) const
    {
    ConstVolPair p = volPair();
    for( ConstVolIterator i=p.begin(); i!=p.end(); ++i )
	i->getCommitActions( l );
    }

string Container::createText( bool doing ) const
    {
    string txt;
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

int Container::doCreate( Volume * v ) 
    { 
    y2warning( "invalid doCreate Container:%s name:%s",
	       type_names[typ].c_str(), name().c_str() ); 
    return( CONTAINER_INTERNAL_ERROR );
    }

string Container::removeText( bool doing ) const
    {
    string txt;
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

int Container::doRemove( Volume * v ) 
    { 
    y2warning( "invalid doRemove Container:%s name:%s",
	       type_names[typ].c_str(), name().c_str() ); 
    return( CONTAINER_INTERNAL_ERROR );
    }

string Container::type_names[] = { "UNKNOWN", "DISK", "MD", "LOOP", "LVM", "EVMS" };

