/* 
  Textdomain    "storage"
*/

#include <iostream>
#include <algorithm>
#include <list>

#include "y2storage/Container.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/Storage.h"
#include "y2storage/Md.h"
#include "y2storage/Loop.h"
#include "y2storage/AppUtil.h"

using namespace std;
using namespace storage;

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
		vol.push_back( &(*i) );
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
	y2milestone( "ret:%d stage:%d col:%zd vol:%zd", ret, stage,
	             col.size(), vol.size());
    return( ret );
    }

int Container::commitChanges( CommitStage stage, Volume* vol )
    {
    y2milestone( "name:%s stage %d vol:%s", name().c_str(), stage,
                 vol->name().c_str() );
    y2mil( "vol:" << *vol );
    int ret = 0;
    switch( stage )
	{
	case DECREASE:
	    if( vol->deleted() )
		ret = doRemove( vol );
	    else if( vol->needShrink() )
		ret = doResize( vol );
	    break;
	case INCREASE:
	    if( vol->created() )
		ret = doCreate( vol );
	    else if( vol->needExtend() )
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

void Container::setExtError( const string& txt ) const
    {
    if( sto )
	sto->setExtError(txt);
    }

void Container::setExtError( const SystemCmd& cmd, bool serr ) const
    {
    const string& s = serr ? cmd.stderr() : cmd.stdout();
    if( s.size()>0 )
	{
	sto->setExtError( cmd.cmd() + ":\n" + s );
	}
    else
	y2warning( "called with empty %s cmd:%s",
		   (serr?"stderr":"stdout"), cmd.cmd().c_str());
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

void Container::getInfo( storage::ContainerInfo& info ) const
    {
    Container::ConstVolPair vp = volPair( Volume::notDeleted );
    info.type = type();
    info.name = name();
    info.device = device();
    info.volcnt = vp.length();
    info.usedBy = getUsedByType();
    info.usedByName = usedByName();
    info.readonly = readonly();
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
    s << c.uby;
    return( s );
    }

string 
Container::logDifference( const Container& c ) const
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
	b << uby << "-->" << string(c.uby);
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
	    y2milestone( "%s", logDifference( *c ).c_str() );
	}
    else
	{
	switch( typ )
	    {
	    case DISK:
		ret = ((const Disk*)this)->equalContent( *(const Disk*)c );
		if( !ret && verbose )
		    ((const Disk*)this)->logDifference( *(const Disk*)c );
		break;
	    case MD:
		ret = ((const MdCo*)this)->equalContent( *(const MdCo*)c );
		if( !ret && verbose )
		    ((const MdCo*)this)->logDifference( *(const MdCo*)c );
		break;
	    case LOOP:
		ret = ((const LoopCo*)this)->equalContent( *(const LoopCo*)c );
		if( !ret && verbose )
		    ((const LoopCo*)this)->logDifference( *(const LoopCo*)c );
		break;
	    case LVM:
		ret = ((const LvmVg*)this)->equalContent( *(const LvmVg*)c );
		if( !ret && verbose )
		    ((const LvmVg*)this)->logDifference( *(const LvmVg*)c );
		break;
	    case DM:
		ret = ((const PeContainer*)this)->equalContent( *(const PeContainer*)c );
		if( !ret && verbose )
		    ((const DmCo*)this)->logDifference( *(const DmCo*)c );
		break;
	    case EVMS:
		ret = ((const EvmsCo*)this)->equalContent( *(const EvmsCo*)c );
		if( !ret && verbose )
		    ((const EvmsCo*)this)->logDifference( *(const EvmsCo*)c );
		break;
	    default:
		ret = this->equalContent( *c );
		break;
	    }
	}
    return( ret );
    }


Container& Container::operator= ( const Container& rhs )
    {
    y2milestone( "operator= from %s", rhs.nm.c_str() );
    typ = rhs.typ;
    nm = rhs.nm;
    dev = rhs.dev;
    del = rhs.del;
    create = rhs.create;
    ronly = rhs.ronly;
    silent = rhs.silent;
    uby = rhs.uby;
    return( *this );
    }

Container::Container( const Container& rhs ) : sto(rhs.sto)
    {
    y2milestone( "constructed cont by copy constructor from %s", 
                 rhs.nm.c_str() );
    *this = rhs;
    }

string Container::type_names[] = { "UNKNOWN", "DISK", "MD", "LOOP", "LVM", "DM", "EVMS" };

