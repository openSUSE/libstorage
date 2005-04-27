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
    del = silent = ronly = create = false;
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

static bool existingVol( const Volume& v ) 
    { return( !v.deleted()&&!v.created()); }

unsigned Container::numVolumes() const
    {
    ConstVolPair p = volPair( existingVol );
    return( p.length() );
    }

static bool stageDecrease( const Volume& v ) 
    { return( v.deleted()||v.needShrink()); }
static bool stageCreate( const Volume& v )
    { return( v.created()||v.needExtend()); }
static bool stageFormat( const Volume& v ) 
    { return( v.getFormat()||v.needLosetup()||v.needLabel()); }
static bool stageMount( const Volume& v ) 
    { return( v.needRemount()||v.needFstabUpdate()); }

int Container::getToCommit( CommitStage stage, list<Container*>& col, 
                            list<Volume*>& vol )
    {
    int ret = 0;
    unsigned long oco = col.size(); 
    unsigned long ovo = vol.size(); 
    switch( stage )
	{
	case DECREASE:
	    {
	    VolPair p = volPair( stageDecrease );
	    for( VolIterator i=p.begin(); i!=p.end(); ++i )
		vol.push_back( &(*i) );
	    if( deleted() )
		col.push_back( this );
	    }
	    break;
	case INCREASE:
	    {
	    VolPair p = volPair( stageCreate );
	    for( VolIterator i=p.begin(); i!=p.end(); ++i )
		{
		vol.push_back( &(*i) );
		}
	    if( created() )
		col.push_back( this );
	    }
	    break;
	case FORMAT:
	    {
	    VolPair p = volPair( stageFormat );
	    for( VolIterator i=p.begin(); i!=p.end(); ++i )
		vol.push_back( &(*i) );
	    }
	    break;
	case MOUNT:
	    {
	    VolPair p = volPair( stageMount );
	    for( VolIterator i=p.begin(); i!=p.end(); ++i )
		vol.push_back( &(*i) );
	    }
	    break;
	default:
	    break;
	}
    if( col.size()!=oco || vol.size()!=ovo )
	y2milestone( "ret:%d col:%d vol:%d", ret, col.size(), vol.size());
    return( ret );
    }

int Container::commitChanges( CommitStage stage, Volume* vol )
    {
    y2milestone( "name:%s stage %d vol:%s", name().c_str(), stage,
                 vol->name().c_str() );
    int ret = 0;
    switch( stage )
	{
	case DECREASE:
	    if( vol->deleted() )
		ret = doRemove( vol );
	    else
		ret = doResize( vol );
	    break;
	case INCREASE:
	    if( vol->created() )
		ret = doCreate( vol );
	    else
		ret = doResize( vol );
	    break;
	case FORMAT:
	    if( vol->needLosetup() )
		ret = vol->doLosetup();
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Container::commitChanges( CommitStage stage )
    {
    y2milestone( "name %s stage %d", name().c_str(), stage );
    int ret = CONTAINER_INVALID_VIRTUAL_CALL;
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

int Container::doCreate( Volume * v ) 
    { 
    y2warning( "invalid Container:%s name:%s", type_names[typ].c_str(), 
               name().c_str() ); 
    return( CONTAINER_INVALID_VIRTUAL_CALL );
    }

int Container::doRemove( Volume * v ) 
    { 
    y2warning( "invalid Container:%s name:%s", type_names[typ].c_str(), 
               name().c_str() ); 
    return( CONTAINER_INVALID_VIRTUAL_CALL );
    }

int Container::doResize( Volume * v ) 
    { 
    y2warning( "invalid Container:%s name:%s", type_names[typ].c_str(), 
               name().c_str() ); 
    return( CONTAINER_INVALID_VIRTUAL_CALL );
    }

int Container::removeVolume( Volume * v ) 
    { 
    y2warning( "invalid Container:%s name:%s", type_names[typ].c_str(), 
               name().c_str() ); 
    return( CONTAINER_INVALID_VIRTUAL_CALL );
    }

int Container::resizeVolume( Volume * v, unsigned long long )
    {
    return( VOLUME_RESIZE_UNSUPPORTED_BY_CONTAINER );
    }

bool Container::removeFromList( Volume* e )
    {
    bool ret=false;
    PlainIterator i = vols.begin();
    while( i!=vols.end() && *i!=e )
	++i;
    if( i!=vols.end() )
	{
	delete( *i );
	vols.erase( i );
	ret = true;
	}
    y2milestone( "P:%p ret:%d", e, ret );
    return( ret );
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

string Container::type_names[] = { "UNKNOWN", "DISK", "MD", "LOOP", "LVM", "DM", "EVMS" };

