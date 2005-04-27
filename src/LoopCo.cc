#include <iostream> 
#include <sstream> 

#include "y2storage/LoopCo.h"
#include "y2storage/Loop.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"
#include "y2storage/EtcFstab.h"

using namespace storage;
using namespace std;

LoopCo::LoopCo( Storage * const s, bool detect ) :
    Container(s,"loop",staticType())
    {
    y2milestone( "construcing LoopCo detect:%d", detect );
    init();
    if( detect )
	getLoopData();
    }

LoopCo::LoopCo( Storage * const s, const string& file ) :
    Container(s,"loop",staticType())
    {
    y2milestone( "construcing LoopCo file:%s", file.c_str() );
    init();
    }

LoopCo::~LoopCo()
    {
    y2milestone( "destructed LoopCo" );
    }

void
LoopCo::init()
    {
    }

static bool loopNotDeleted( const Loop& l ) { return( !l.deleted() ); }

void
LoopCo::getLoopData()
    {
    y2milestone( "begin" );
    list<FstabEntry> l;
    EtcFstab* fstab = getStorage()->getFstab();
    fstab->getFileBasedLoops( getStorage()->root(), l );
    for( list<FstabEntry>::const_iterator i=l.begin(); i!=l.end(); ++i )
	{
	string lfile = getStorage()->root() + i->dentry;
	if( findLoop( i->dentry ))
	    y2warning( "duplicate loop file %s", i->dentry.c_str() );
	else if( i->loop_dev.size()>0 && Volume::loopInUse( getStorage(),
	                                                    i->loop_dev ) )
	    y2warning( "duplicate loop_device %s", i->loop_dev.c_str() );
	else if( !checkNormalFile( lfile ))
	    y2warning( "file %s not existent or special", lfile.c_str() );
	else
	    {
	    Loop *l = new Loop( *this, i->loop_dev, lfile );
	    l->setEncryption( i->encr );
	    l->setFs( Volume::toFsType(i->fs) );
	    addToList( l );
	    }
	}
    }

void
LoopCo::checkLoop( Loop* m )
    {
    LoopIter i;
    if( findLoop( m->device(), i ))
	{
	i->setSize( m->sizeK() );
	i->setCreated( false );
	}
    else
	{
	y2warning( "checkLoop does not exist %u", m->nr() ); 
	}
    delete m;
    }

bool
LoopCo::findLoop( unsigned num, LoopIter& i )
    {
    LoopPair p=loopPair(loopNotDeleted);
    i=p.begin();
    while( i!=p.end() && i->nr()!=num )
	++i;
    return( i!=p.end() );
    }

bool
LoopCo::findLoop( unsigned num )
    {
    LoopIter i;
    return( findLoop( num, i ));
    }

bool
LoopCo::findLoop( const string& file, LoopIter& i )
    {
    LoopPair p=loopPair(loopNotDeleted);
    i=p.begin();
    while( i!=p.end() && i->loopFile()!=file )
	++i;
    return( i!=p.end() );
    }

bool
LoopCo::findLoop( const string& file )
    {
    LoopIter i;
    return( findLoop( file, i ));
    }

int 
LoopCo::createLoop( const string& file, bool reuseExisting,
                    unsigned long long sizeK, string& device )
    {
    int ret = 0;
    y2milestone( "file:%s reuseEx:%d sizeK:%llu", file.c_str(), 
                 reuseExisting, sizeK );
    if( readonly() )
	{
	ret = LOOP_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( findLoop( file ))
	    ret = LOOP_DUPLICATE_FILE;
	}
    if( ret==0 )
	{
	Loop* l = new Loop( *this, file, reuseExisting, sizeK );
	l->setCreated( true );
	addToList( l );
	device = l->device();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int 
LoopCo::removeLoop( const string& file, bool removeFile )
    {
    int ret = 0;
    y2milestone( "file:%s removeFile:%d", file.c_str(), removeFile );
    LoopIter i;
    if( readonly() )
	{
	ret = LOOP_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findLoop( file, i ))
	    ret = LOOP_UNKNOWN_FILE;
	}
    if( ret==0 && i->getUsedByType() != UB_NONE )
	{
	ret = LOOP_REMOVE_USED_BY;
	}
    if( ret==0 )
	{
	if( i->created() )
	    {
	    if( !removeFromList( &(*i) ))
		ret = LOOP_REMOVE_CREATE_NOT_FOUND;
	    }
	else
	    {
	    i->setDeleted( true );
	    i->setDelFile( removeFile );
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int LoopCo::removeVolume( Volume* v )
    {
    int ret = 0;
    y2milestone( "name:%s", v->name().c_str() );
    Loop * l = dynamic_cast<Loop *>(v);
    if( l != NULL )
	ret = removeLoop( l->loopFile(), false );
    else
	ret = LOOP_REMOVE_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int 
LoopCo::doCreate( Volume* v ) 
    {
    y2milestone( "name:%s", v->name().c_str() );
    Loop * l = dynamic_cast<Loop *>(v);
    int ret = 0;
    if( l != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->createText(true) );
	    }
	if( !l->createFile() )
	    ret = LOOP_FILE_CREATE_FAILED;
	if( ret==0 )
	    {
	    ret = l->doLosetup();
	    }
	if( ret==0 )
	    {
	    l->setCreated( false );
	    }
	}
    else
	ret = LOOP_CREATE_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int 
LoopCo::doRemove( Volume* v )
    {
    y2milestone( "name:%s", v->name().c_str() );
    Loop * l = dynamic_cast<Loop *>(v);
    int ret = 0;
    if( l != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->removeText(true) );
	    }
	ret = l->prepareRemove();
	if( ret==0 )
	    {
	    if( !l->removeFile() )
		ret = LOOP_REMOVE_FILE_FAILED;
	    if( !removeFromList( l ) && ret==0 )
		ret = LOOP_NOT_IN_LIST;
	    }
	}
    else
	ret = LOOP_REMOVE_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void LoopCo::logData( const string& Dir ) {;}
