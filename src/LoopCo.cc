/*
  Textdomain    "storage"
*/

#include <ostream>
#include <sstream>

#include "y2storage/LoopCo.h"
#include "y2storage/Loop.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/Dm.h"
#include "y2storage/ProcMounts.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"
#include "y2storage/EtcFstab.h"
#include "y2storage/StorageDefines.h"


namespace storage
{
    using namespace std;


LoopCo::LoopCo(Storage * const s, bool detect, ProcPart& ppart)
    : Container(s, "loop", staticType())
{
    y2deb("constructing LoopCo detect:" << detect);
    init();
    if( detect )
	getLoopData( ppart );
}


LoopCo::LoopCo(Storage * const s, const string& file)
    : Container(s, "loop", staticType())
{
    y2deb("constructing LoopCo file:" << file);
    init();
}


LoopCo::~LoopCo()
{
    y2deb("destructed LoopCo");
}


void
LoopCo::init()
    {
    mjr = Loop::major();
    }

void
LoopCo::getLoopData( ProcPart& ppart )
    {
    y2mil("begin");
    list<FstabEntry> l;
    const EtcFstab* fstab = getStorage()->getFstab();
    fstab->getFileBasedLoops( getStorage()->root(), l );
    if( !l.empty() )
	{
	SystemCmd c(LOSETUPBIN " -a");
	for( list<FstabEntry>::const_iterator i=l.begin(); i!=l.end(); ++i )
	    {
	    y2mil( "i:" << *i );
	    string lfile = getStorage()->root() + i->device;
	    if( findLoop( i->dentry ))
		y2war("duplicate loop file " << i->dentry);
	    else if( !i->loop_dev.empty() && Volume::loopInUse( getStorage(),
								i->loop_dev ) )
		y2war("duplicate loop_device " << i->loop_dev);
	    else if( !checkNormalFile( lfile ))
		y2war("not existent or special file " << lfile);
	    else
		{
		Loop *l = new Loop( *this, i->loop_dev, lfile,
				    i->dmcrypt, !i->noauto?i->dentry:"",
				    ppart, c );
		l->setEncryption( i->encr );
		l->setFs( Volume::toFsType(i->fs) );
		y2mil( "l:" << *l );
		addToList( l );
		}
	    }
	LoopPair p=loopPair(Loop::notDeleted);
	LoopIter i=p.begin();
	std::map<string,string> mp = ProcMounts(getStorage()).allMounts();
	while( i!=p.end() )
	    {
	    if( i->dmcrypt() )
		{
		y2mil( "i:" << *i );
		if( i->dmcryptDevice().empty() )
		    {
		    const Dm* dm = 0;
		    if( !i->loopDevice().empty() )
			{
			getStorage()->findDmUsing( i->loopDevice(), dm );
			}
		    if( dm==0 && !i->getMount().empty() &&
			!mp[i->getMount()].empty() )
			{
			y2mil( "mp:" << i->getMount() << " dev:" <<
			       mp[i->getMount()] );
			getStorage()->findDm( mp[i->getMount()], dm );
			}
		    if( dm )
			{
			i->setDmcryptDev( dm->device() );
			i->setSize( dm->sizeK() );
			}
		    y2mil( "i:" << *i );
		    }
		getStorage()->removeDm( i->dmcryptDevice() );
		}
	    ++i;
	    }
	}
    }

void LoopCo::loopIds( std::list<unsigned>& l ) const
    {
    l.clear();
    ConstLoopPair p=loopPair(Loop::notDeleted);
    ConstLoopIter i=p.begin();
    while( i!=p.end() )
	{
	l.push_back( i->nr() );
	++i;
	}
    }

bool
LoopCo::findLoop( unsigned num, LoopIter& i )
    {
    LoopPair p=loopPair(Loop::notDeleted);
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
    LoopPair p=loopPair(Loop::notDeleted);
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

bool
LoopCo::findLoopDev( const string& dev, LoopIter& i )
    {
    LoopPair p=loopPair(Loop::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->device()!=dev )
	++i;
    return( i!=p.end() );
    }

int
LoopCo::createLoop( const string& file, bool reuseExisting,
                    unsigned long long sizeK, bool dmcr, string& device )
    {
    int ret = 0;
    y2mil("file:" << file << " reuse:" << reuseExisting << " sizeK:" <<
	  sizeK << " dmcr:" << dmcr);
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
	Loop* l = new Loop( *this, file, reuseExisting, sizeK, dmcr );
	l->setCreated( true );
	addToList( l );
	device = l->device();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LoopCo::updateLoop( const string& device, const string& file,
                    bool reuseExisting, unsigned long long sizeK )
    {
    int ret = 0;
    y2mil("device:" << device << " reuse:" << reuseExisting << " sizeK:" <<
	  sizeK);
    LoopIter i;
    if( readonly() )
	{
	ret = LOOP_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findLoopDev( device, i ))
	    ret = LOOP_UNKNOWN_FILE;
	}
    if( ret==0 && !i->created() )
	{
	ret = LOOP_MODIFY_EXISTING;
	}
    if( ret==0 )
	{
	i->setLoopFile( file );
	i->setReuse( reuseExisting );
	if( !i->getReuse() )
	    i->setSize( sizeK );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LoopCo::removeLoop( const string& file, bool removeFile )
    {
    int ret = 0;
    y2mil("file:" << file << " removeFile:" << removeFile);
    LoopIter i;
    if( readonly() )
	{
	ret = LOOP_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findLoop( file, i ) && !findLoopDev( file, i ) )
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
	    i->setDeleted();
	    i->setDelFile( removeFile );
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int LoopCo::removeVolume( Volume* v )
    {
    int ret = 0;
    y2mil("name:" << v->name());
    Loop * l = dynamic_cast<Loop *>(v);
    if( l != NULL )
	ret = removeLoop( l->loopFile(), false );
    else
	ret = LOOP_REMOVE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }

int
LoopCo::doCreate( Volume* v )
    {
    y2mil("name:" << v->name());
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
	    ret = l->doCrsetup();
	    }
	if( ret==0 )
	    {
	    l->setCreated( false );
	    }
	}
    else
	ret = LOOP_CREATE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }

int
LoopCo::doRemove( Volume* v )
    {
    y2mil("name:" << v->name());
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
    y2mil("ret:" << ret);
    return( ret );
    }


void
LoopCo::logData(const string& Dir) const
{
}


inline std::ostream& operator<< (std::ostream& s, const LoopCo& d )
    {
    s << *((Container*)&d);
    return( s );
    }


void LoopCo::logDifference( const Container& d ) const
    {
    y2mil(getDiffString(d));
    const LoopCo * p = dynamic_cast<const LoopCo*>(&d);
    if( p != NULL )
	{
	ConstLoopPair pp=loopPair();
	ConstLoopIter i=pp.begin();
	while( i!=pp.end() )
	    {
	    ConstLoopPair pc=p->loopPair();
	    ConstLoopIter j = pc.begin();
	    while( j!=pc.end() &&
		   (i->device()!=j->device() || i->created()!=j->created()) )
		++j;
	    if( j!=pc.end() )
		{
		if( !i->equalContent( *j ) )
		    i->logDifference( *j );
		}
	    else
		y2mil( "  -->" << *i );
	    ++i;
	    }
	pp=p->loopPair();
	i=pp.begin();
	while( i!=pp.end() )
	    {
	    ConstLoopPair pc=loopPair();
	    ConstLoopIter j = pc.begin();
	    while( j!=pc.end() &&
		   (i->device()!=j->device() || i->created()!=j->created()) )
		++j;
	    if( j==pc.end() )
		y2mil( "  <--" << *i );
	    ++i;
	    }
	}
    }

bool LoopCo::equalContent( const Container& rhs ) const
    {
    const LoopCo * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const LoopCo*>(&rhs);
    if( ret && p )
	{
	ConstLoopPair pp = loopPair();
	ConstLoopPair pc = p->loopPair();
	ConstLoopIter i = pp.begin();
	ConstLoopIter j = pc.begin();
	while( ret && i!=pp.end() && j!=pc.end() )
	    {
	    ret = ret && i->equalContent( *j );
	    ++i;
	    ++j;
	    }
	ret = ret && i==pp.end() && j==pc.end();
	}
    return( ret );
    }


LoopCo::LoopCo(const LoopCo& rhs)
    : Container(rhs)
{
    y2deb("constructed LoopCo by copy constructor from " << rhs.nm);
    *this = rhs;
    ConstLoopPair p = rhs.loopPair();
    for( ConstLoopIter i=p.begin(); i!=p.end(); ++i )
    {
	Loop * p = new Loop( *this, *i );
	vols.push_back( p );
    }
}

}
