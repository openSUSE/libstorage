#include <iostream> 
#include <sstream> 

#include <ycp/y2log.h>

#include "y2storage/MdCo.h"
#include "y2storage/Md.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"
#include "y2storage/EtcRaidtab.h"

MdCo::MdCo( Storage * const s, bool detect ) :
    Container(s,"md",staticType())
    {
    y2milestone( "construcing MdCo detect:%d", detect );
    init();
    if( detect )
	getMdData();
    }

MdCo::MdCo( Storage * const s, const string& file ) :
    Container(s,"md",staticType())
    {
    y2milestone( "construcing MdCo file:%s", file.c_str() );
    init();
    }

MdCo::~MdCo()
    {
    y2milestone( "destructed MdCo" );
    delete tab;
    }

void
MdCo::init()
    {
    tab = NULL;
    if( !getStorage()->instsys() )
	tab = new EtcRaidtab( getStorage()->root() );
    }

static bool mdNotDeleted( const Md& l ) { return( !l.deleted() ); }

void
MdCo::syncRaidtab()
    {
    delete tab;
    tab = new EtcRaidtab( getStorage()->root() );
    MdPair p=mdPair(mdNotDeleted);
    for( MdIter i=p.begin(); i!=p.end(); ++i )
	{
	list<string> lines;
	i->raidtabLines(lines);
	tab->updateEntry( i->nr(), lines );
	}
    }

void
MdCo::getMdData()
    {
    y2milestone( "begin" );
    string line;
    std::ifstream file( "/proc/mdstat" );
    unsigned dummy;
    getline( file, line );
    while( file.good() )
	{
	if( Md::mdStringNum(extractNthWord( 0, line ),dummy) ) 
	    {
	    string line2;
	    getline( file, line2 );
	    Md* m = new Md( *this, line, line2 );
	    addMd( m );
	    }
	getline( file, line );
	}
    file.close();
    file.clear();
    file.open( (getStorage()->root()+"/etc/raidtab").c_str() );
    MdIter i;
    getline( file, line );
    while( file.good() )
	{
	if( extractNthWord( 0, line )=="raiddev" )
	    {
	    string md = extractNthWord( 1, line );
	    getline( file, line );
	    if( findMd( md, i ))
		{
		string key;
		string device;
		while( file.good() && 
		       (key=extractNthWord( 0, line ))!="raiddev" )
		    {
		    if( key=="device" )
			device = extractNthWord( 1, line );
		    else if( key=="spare-disk" )
			{
			if( device.size()>0 )
			    {
			    normalizeDevice(device);
			    i->addSpareDevice( device );
			    device.clear();
			    }
			}
		    getline( file, line );
		    }
		}
	    else
		y2warning( "raid %s from /etc/raidtab not found", md.c_str() );
	    }
	getline( file, line );
	}
    MdPair p=mdPair(mdNotDeleted);
    for( MdIter i=p.begin(); i!=p.end(); ++i )
	{
	string num = decString(i->nr());
	list<string> devs;
	i->getDevs( devs );
	for( list<string>::iterator s=devs.begin(); s!=devs.end(); ++s )
	    getStorage()->setUsedBy( *s, UB_MD, num );
	}
    }

void
MdCo::getMdData( unsigned num )
    {
    y2milestone( "num:%u", num );
    string line;
    std::ifstream file( "/proc/mdstat" );
    string md = "md" + decString(num);
    getline( file, line );
    while( file.good() )
	{
	if( extractNthWord( 0, line ) == md ) 
	    {
	    string line2;
	    getline( file, line2 );
	    Md* m = new Md( *this, line, line2 );
	    checkMd( m );
	    }
	getline( file, line );
	}
    }

void
MdCo::addMd( Md* m )
    {
    if( !findMd( m->nr() ))
	addToList( m );
    else
	{
	y2warning( "addMd alread exists %u", m->nr() ); 
	delete m;
	}
    }

void
MdCo::checkMd( Md* m )
    {
    MdIter i;
    if( findMd( m->nr(), i ))
	{
	i->setSize( m->sizeK() );
	i->setCreated( false );
	if( m->personality()!=i->personality() )
	    y2warning( "inconsistent raid type my:%s kernel:%s", 
	               i->pName().c_str(), m->pName().c_str() );
	if( i->parity()!=Md::PAR_NONE && m->parity()!=i->parity() )
	    y2warning( "inconsistent parity my:%s kernel:%s", 
	               i->ptName().c_str(), m->ptName().c_str() );
	if( i->chunkSize()>0 && m->chunkSize()!=i->chunkSize() )
	    y2warning( "inconsistent chunk size my:%lu kernel:%lu", 
	               i->chunkSize(), m->chunkSize() );
	}
    else
	{
	y2warning( "checkMd does not exist %u", m->nr() ); 
	}
    delete m;
    }


bool
MdCo::findMd( unsigned num, MdIter& i )
    {
    MdPair p=mdPair(mdNotDeleted);
    i=p.begin();
    while( i!=p.end() && i->nr()!=num )
	++i;
    return( i!=p.end() );
    }

bool
MdCo::findMd( unsigned num )
    {
    MdIter i;
    return( findMd( num, i ));
    }

bool
MdCo::findMd( const string& dev, MdIter& i )
    {
    string d(dev);
    if( d.find( "/dev/" )==0 )
	d.erase( 0, 5 );
    if( d.find( "md" )==0 )
	{
	d.erase( 0, 2 );
	unsigned num;
	d >> num;
	return( findMd( num, i ));
	}
    else
	return( false );
    }

bool
MdCo::findMd( const string& dev )
    {
    MdIter i;
    return( findMd( dev, i ));
    }

unsigned 
MdCo::unusedNumber()
    {
    unsigned i=0;
    while( findMd(i) && i<1000 )
	i++;
    return( i );
    }

int 
MdCo::createMd( unsigned num, MdType type, const list<string>& devs )
    {
    int ret = 0;
    ostringstream buf;
    buf << "num:" << num << " type:" << Md::pName(type) << " devs:" << devs;
    y2milestone( "%s", buf.str().c_str() );
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 && num>=256 )
	{
	ret = MD_NUMBER_TOO_LARGE;
	}
    if( ret==0 )
	{
	if( findMd( num ))
	    ret = MD_DUPLICATE_NUMBER;
	}
    unsigned long long sum = 0;
    unsigned long long smallest = 0;
    list<string>::const_iterator i=devs.begin();
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	const Volume* v = getStorage()->getVolume( d );
	if( v==NULL )
	    {
	    ret = MD_DEVICE_UNKNOWN;
	    }
	else if( !v->canUseDevice() )
	    {
	    ret = MD_DEVICE_USED;
	    }
	else 
	    {
	    sum += v->sizeK();
	    if( smallest==0 )
		smallest = v->sizeK();
	    else
		smallest = min( smallest, v->sizeK() );
	    }
	++i;
	}
    unsigned long long rsize = 0;
    unsigned nmin = 2;
    if( ret==0 )
	{
	switch( type )
	    {
	    case RAID0:
		rsize = sum;
		break;
	    case RAID1:
	    case MULTIPATH:
		rsize = smallest;
		break;
	    case RAID5:
		nmin = 3;
		rsize = smallest*(devs.size()-1);
		break;
	    case RAID6:
		nmin = 4;
		rsize = smallest*(devs.size()-2);
		break;
	    case RAID10:
		rsize = smallest*devs.size()/2;
		break;
	    default:
		break;
	    }
	if( devs.size()<nmin )
	    {
	    ret = MD_TOO_FEW_DEVICES;
	    }
	}
    y2milestone( "min:%u smallest:%llu sum:%llu size:%llu", nmin, smallest, 
                 sum, rsize );
    i=devs.begin();
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	getStorage()->setUsedBy( d, UB_MD, decString(num) );
	++i;
	}
    if( ret==0 )
	{
	Md* m = new Md( *this, num, type, devs );
	m->setCreated( true );
	m->setSize( rsize );
	addToList( m );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int 
MdCo::removeMd( unsigned num )
    {
    int ret = 0;
    y2milestone( "num:%u", num );
    MdIter i;
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findMd( num, i ))
	    ret = MD_UNKNOWN_NUMBER;
	}
    if( ret==0 && i->getUsedByType() != UB_NONE )
	{
	ret = MD_REMOVE_USED_BY;
	}
    if( ret==0 )
	{
	list<string> devs;
	i->getDevs( devs );
	for( list<string>::const_iterator s=devs.begin(); s!=devs.end(); ++s )
	    getStorage()->setUsedBy( *s, UB_NONE, "" );
	i->setDeleted( true );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int MdCo::removeVolume( Volume* v )
    {
    return( removeMd( v->nr() ));
    }

void MdCo::activate( bool val )
    {
    y2milestone( "old active:%d val:%d", md_active, val );
    if( md_active!=val )
	{
	SystemCmd c;
	if( md_active )
	    {
	    c.execute( "raidautorun" );
	    }
	else
	    {
	    c.execute( "mdadm --stop --scan" );
	    }
	md_active = val;
	}
    }

int 
MdCo::doCreate( Volume* v ) 
    {
    y2milestone( "name:%s", v->name().c_str() );
    Md * m = dynamic_cast<Md *>(v);
    int ret = 0;
    if( m != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( m->createText(true) );
	    }
	string cmd = m->createCmd();
	SystemCmd c( cmd );
	if( c.retcode()!=0 )
	    ret = MD_CREATE_FAILED;
	if( ret==0 )
	    {
	    getMdData( m->nr() );
	    if( tab!=NULL )
		{
		list<string> lines;
		m->raidtabLines(lines);
		tab->updateEntry( m->nr(), lines );
		}
	    }
	}
    else
	ret = MD_CREATE_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int 
MdCo::doRemove( Volume* v )
    {
    y2milestone( "name:%s", v->name().c_str() );
    Md * m = dynamic_cast<Md *>(v);
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
	    string cmd = "mdadm --stop " + m->device();
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		ret = MD_REMOVE_FAILED;
	    }
	if( ret==0 )
	    {
	    if( tab!=NULL )
		{
		tab->removeEntry( m->nr() );
		}
	    if( !removeFromList( m ) )
		ret = MD_NOT_IN_LIST;
	    }
	}
    else
	ret = MD_CREATE_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void MdCo::logData( const string& Dir ) {;}

bool MdCo::md_active = false;

