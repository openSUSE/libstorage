/*
  Textdomain    "storage"
*/

#include <iostream> 
#include <sstream> 

#include "y2storage/DmCo.h"
#include "y2storage/Dm.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/ProcPart.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

using namespace std;
using namespace storage;

DmCo::DmCo( Storage * const s, bool detect ) :
    PeContainer(s,staticType())
    {
    y2milestone( "constructing DmCo detect:%d", detect );
    init();
    if( detect )
	getDmData();
    }

DmCo::DmCo( Storage * const s, const string& file ) :
    PeContainer(s,staticType())
    {
    y2milestone( "constructing DmCo file:%s", file.c_str() );
    init();
    }

DmCo::~DmCo()
    {
    y2milestone( "destructed DmCo" );
    }

void
DmCo::init()
    {
    nm = "dm";
    dev = "/dev/dm-";
    }

void
DmCo::getDmData()
    {
    ProcPart ppart;
    Storage::ConstEvmsPair ev = getStorage()->evmsPair();
    Storage::ConstLvmLvPair lv = getStorage()->lvmLvPair();
    y2milestone( "begin" );
    SystemCmd c( "dmsetup ls" );
    for( unsigned i=0; i<c.numLines(); ++i )
	{
	string line = *c.getLine(i);
	string table = extractNthWord( 0, line );
	bool found=false;
	Storage::ConstLvmLvIterator i=lv.begin();
	while( !found && i!=lv.end() )
	    {
	    found = i->getTableName()==table;
	    ++i;
	    }
	if( !found )
	    {
	    Storage::ConstEvmsIterator i=ev.begin();
	    while( !found && i!=ev.end() )
		{
		found = i->getTableName()==table;
		++i;
		}
	    }
	if( !found )
	    {
	    string minor = extractNthWord( 2, line );
	    unsigned min_num;
	    string::size_type pos;
	    if( (pos=minor.find( ")" ))!=string::npos )
		minor.erase( pos );
	    minor >> min_num;
	    Dm * m = new Dm( *this, table, min_num );
	    unsigned long long s = 0;
	    if( ppart.getSize( m->device(), s ))
		{
		m->setSize( s );
		}
	    bool in_use = false;
	    const map<string,unsigned long>& pe = m->getPeMap();
	    for( map<string,unsigned long>::const_iterator it = pe.begin();
		 it!=pe.end(); ++it )
		{
		if( !getStorage()->canUseDevice( it->first, true ))
		    in_use = true;
		else
		    getStorage()->setUsedBy( it->first, UB_DM, 
					     undevDevice(m->device()) );
		}
	    if( !in_use )
		addDm( m );
	    else
		delete m;
	    }
	}
    }

void
DmCo::addDm( Dm* m )
    {
    if( !findDm( m->nr() ))
	addToList( m );
    else
	{
	y2warning( "addDm alread exists %u", m->nr() ); 
	delete m;
	}
    }

bool
DmCo::findDm( unsigned num, DmIter& i )
    {
    DmPair p=dmPair(Dm::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->nr()!=num )
	++i;
    return( i!=p.end() );
    }

bool
DmCo::findDm( unsigned num )
    {
    DmIter i;
    return( findDm( num, i ));
    }

bool
DmCo::findDm( const string& tname, DmIter& i )
    {
    DmPair p=dmPair(Dm::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->getTableName()!=tname )
	++i;
    return( i!=p.end() );
    }

bool
DmCo::findDm( const string& tname )
    {
    DmIter i;
    return( findDm( tname, i ));
    }

int 
DmCo::removeDm( const string& tname )
    {
    int ret = 0;
    y2milestone( "tname:%s", tname.c_str() );
    DmIter i;
    if( readonly() )
	{
	ret = DM_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findDm( tname, i ))
	    ret = DM_UNKNOWN_TABLE;
	}
    if( ret==0 && i->getUsedByType() != UB_NONE )
	{
	ret = DM_REMOVE_USED_BY;
	}
    if( ret==0 )
	{
	if( i->created() )
	    {
	    if( !removeFromList( &(*i) ))
		ret = DM_REMOVE_CREATE_NOT_FOUND;
	    }
	else
	    {
	    const map<string,unsigned long>& pe = i->getPeMap();
	    for( map<string,unsigned long>::const_iterator it = pe.begin();
		 it!=pe.end(); ++it )
		{
		getStorage()->setUsedBy( it->first, UB_NONE, "" );
		}
	    i->setDeleted( true );
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int DmCo::removeVolume( Volume* v )
    {
    int ret = 0;
    y2milestone( "name:%s", v->name().c_str() );
    Dm * m = dynamic_cast<Dm *>(v);
    if( m != NULL )
	ret = removeDm( m->getTableName() );
    else 
	ret = DM_REMOVE_INVALID_VOLUME;
    return( ret );
    }

int 
DmCo::doRemove( Volume* v )
    {
    y2milestone( "name:%s", v->name().c_str() );
    Dm * m = dynamic_cast<Dm *>(v);
    int ret = 0;
    if( m != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( m->removeText(true) );
	    }
	ret = m->prepareRemove();
	if( ret==0 )
	    {
	    string cmd = "dmsetup remove " + m->getTableName();
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		ret = DM_REMOVE_FAILED;
	    }
	if( ret==0 )
	    {
	    if( !removeFromList( m ) )
		ret = DM_NOT_IN_LIST;
	    }
	}
    else
	ret = DM_REMOVE_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

inline std::ostream& operator<< (std::ostream& s, const DmCo& d )
    {
    s << *((Container*)&d);
    return( s );
    }

void DmCo::logDifference( const DmCo& d ) const
    {
    string log = Container::logDifference( d );
    y2milestone( "%s", log.c_str() );
    ConstDmPair p=dmPair();
    ConstDmIter i=p.begin();
    while( i!=p.end() )
	{
	ConstDmPair pc=d.dmPair();
	ConstDmIter j = pc.begin();
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
    p=d.dmPair();
    i=p.begin();
    while( i!=p.end() )
	{
	ConstDmPair pc=dmPair();
	ConstDmIter j = pc.begin();
	while( j!=pc.end() && 
	       (i->device()!=j->device() || i->created()!=j->created()) )
	    ++j;
	if( j==pc.end() )
	    y2mil( "  <--" << *i );
	++i;
	}
    }

bool DmCo::equalContent( const DmCo& rhs ) const
    {
    bool ret = Container::equalContent(rhs);
    if( ret )
	{
	ConstDmPair p = dmPair();
	ConstDmPair pc = rhs.dmPair();
	ConstDmIter i = p.begin();
	ConstDmIter j = pc.begin();
	while( ret && i!=p.end() && j!=pc.end() ) 
	    {
	    ret = ret && i->equalContent( *j );
	    ++i;
	    ++j;
	    }
	ret == ret && i==p.end() && j==pc.end();
	}
    return( ret );
    }

DmCo::DmCo( const DmCo& rhs ) : PeContainer(rhs)
    {
    y2milestone( "constructed DmCo by copy constructor from %s", rhs.nm.c_str() );
    ConstDmPair p = rhs.dmPair();
    for( ConstDmIter i = p.begin(); i!=p.end(); ++i )
	{
	Dm * p = new Dm( *this, *i );
	vols.push_back( p );
        }
    }




void DmCo::logData( const string& Dir ) {;}

