#include <iostream>

#include <ycp/y2log.h>

#include "y2storage/Container.h"
#include "y2storage/Md.h"
#include "y2storage/Loop.h"

Container::Container( const Storage * const s, const string& Name, CType t ) :
    sto(s), nm(Name)
    {
    dltd = false;
    rdonly = false;
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

static bool toDelete( const Volume& v ) { return( v.deleted()); }
static bool toCreate( const Volume& v ) { return( v.created()); }
//static bool toFormat( const Volume& v ) { return( v.created()); }

int Container::commitChanges( CommitStage stage )
    {
    y2milestone( "name %s stage %d", name().c_str(), stage );
    int ret = 0;
    switch( stage )
	{
	case DECREASE:
	    {
	    VolPair p = volPair( toDelete );
	    if( !deleted() && !p.empty() )
		{
		VolIterator i=p.end();
		--i;
		do
		    {
		    VolIterator save = i;
		    if( i!=p.begin() )
			--i;
		    ret = doRemove( &(*save) );
		    }
		while( ret==0 && i!=p.begin() );
		}
	    }
	    break;
	case INCREASE:
	    {
	    VolPair p = volPair( toCreate );
	    VolIterator i=p.begin();
	    while( ret==0 && i!=p.end() )
		{
		ret = doCreate( &(*i) );
		++i;
		}
	    }
	    break;
	case FORMAT:
	    break;
	case MOUNT:
	    break;
	default:
	    ret = VOLUME_COMMIT_UNKNOWN_STAGE;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Container::doCreate( Volume * v ) 
    { 
    y2warning( "invalid doCreate Container:%s name:%s",
	       type_names[typ].c_str(), name().c_str() ); 
    return( CONTAINER_INTERNAL_ERROR );
    }

int Container::doRemove( Volume * v ) 
    { 
    y2warning( "invalid doRemove Container:%s name:%s",
	       type_names[typ].c_str(), name().c_str() ); 
    return( CONTAINER_INTERNAL_ERROR );
    }

string Container::type_names[] = { "UNKNOWN", "DISK", "MD", "LOOP", "LVM", "EVMS" };

