/*
  Textdomain    "storage"
*/

#include <ostream>
#include <fstream>

#include "y2storage/MdCo.h"
#include "y2storage/Md.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"
#include "y2storage/EtcRaidtab.h"
#include "y2storage/StorageDefines.h"


namespace storage
{
    using namespace std;


MdCo::MdCo( Storage * const s, bool detect ) :
    Container(s,"md",staticType())
    {
    y2deb("constructing MdCo detect:" << detect);
    init();
    if( detect )
	getMdData();
    }

MdCo::MdCo( Storage * const s, const string& file ) :
    Container(s,"md",staticType())
    {
    y2deb("constructing MdCo file:" << file);
    init();
    }

MdCo::~MdCo()
    {
    y2deb("destructed MdCo");
    delete tab;
    }

void
MdCo::init()
    {
    tab = NULL;
    mjr = Md::mdMajor();
    initTab();
    }

void
MdCo::initTab()
    {
    if( tab==NULL && !getStorage()->instsys() )
	tab = new EtcRaidtab( getStorage()->root() );
    }

void
MdCo::syncRaidtab()
    {
    delete tab;
    string d = getStorage()->root()+"/etc";
    if( !checkDir( d ) )
	createPath( d );
    tab = new EtcRaidtab( getStorage()->root() );
    MdPair p=mdPair(Md::notDeleted);
    for( MdIter i=p.begin(); i!=p.end(); ++i )
	{
	updateEntry( &(*i) );
	}
    }

void MdCo::updateEntry( const Md* m )
    {
    initTab();
    if( tab )
	{
	list<string> lines;
	list<string> devices;
	m->raidtabLines(lines);
	m->getDevs( devices );
	tab->updateEntry( m->nr(), lines, m->mdadmLine(), devices );
	}
    }

void
MdCo::getMdData()
    {
    y2mil("begin");
    string line;
    std::ifstream file( "/proc/mdstat" );
    classic(file);
    unsigned dummy;
    getline( file, line );
    while( file.good() )
	{
	y2mil( "mdstat line:" << line );
	if( Md::mdStringNum(extractNthWord( 0, line ),dummy) ) 
	    {
	    string line2;
	    getline( file, line2 );
	    y2mil( "mdstat line:" << line2 );
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
	y2mil( "raidtab line:" << line );
	if( extractNthWord( 0, line )=="raiddev" )
	    {
	    string md = extractNthWord( 1, line );
	    getline( file, line );
	    y2mil( "raidtab line:" << line );
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
			if( !device.empty() )
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
		y2war("raid " << md << " from /etc/raidtab not found");
	    }
	getline( file, line );
	}
    }

void
MdCo::getMdData( unsigned num )
    {
    y2mil("num:" << num);
    string line;
    std::ifstream file( "/proc/mdstat" );
    classic(file);
    string md = "md" + decString(num);
    getline( file, line );
    while( file.good() )
	{
	y2mil( "mdstat line:" << line );
	if( extractNthWord( 0, line ) == md ) 
	    {
	    string line2;
	    getline( file, line2 );
	    y2mil( "mdstat line2:" << line );
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
	y2war("addMd already exists " << m->nr()); 
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
	i->setMdUuid( m->getMdUuid() );
	i->setCreated( false );
	if( m->personality()!=i->personality() )
	    y2war("inconsistent raid type my:" << i->pName() << " kernel:" << m->pName());
	if( i->parity()!=storage::PAR_NONE && m->parity()!=i->parity() )
	    y2war("inconsistent parity my:" << i->ptName() << " kernel:" << m->ptName());
	if( i->chunkSize()>0 && m->chunkSize()!=i->chunkSize() )
	    y2war("inconsistent chunk size my:" << i->chunkSize() << " kernel:" << m->chunkSize());
	}
    else
	{
	y2war("checkMd does not exist " << m->nr());
	}
    delete m;
    }


bool
MdCo::findMd( unsigned num, MdIter& i )
    {
    MdPair p=mdPair(Md::notDeleted);
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
    unsigned num;
    if( Md::mdStringNum(dev,num) ) 
	{
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
    y2mil( "num:" << num << " type:" << Md::pName(type) << " devs:" << devs );
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 && num>=256 )
	{
	ret = MD_NUMBER_TOO_LARGE;
	}
    if( ret==0 && type==RAID_UNK )
	{
	ret = MD_NO_CREATE_UNKNOWN;
	}
    if( ret==0 )
	{
	if( findMd( num ))
	    ret = MD_DUPLICATE_NUMBER;
	}
    list<string>::const_iterator i=devs.begin();
    while( ret==0 && i!=devs.end() )
	{
	ret = checkUse( normalizeDevice( *i ) );
	++i;
	}
    i=devs.begin();
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	getStorage()->setUsedBy( d, UB_MD, "md"+decString(num) );
	++i;
	}
    if( ret==0 )
	{
	Md* m = new Md( *this, num, type, devs );
	m->setCreated( true );
	addToList( m );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int MdCo::checkUse( const string& dev )
    {
    int ret = 0;
    const Volume* v = getStorage()->getVolume( dev );
    if( v==NULL )
	{
	ret = MD_DEVICE_UNKNOWN;
	}
    else if( !v->canUseDevice() )
	{
	ret = MD_DEVICE_USED;
	}
    y2mil("dev:" << dev << " ret:" << ret);
    return( ret );
    }

int 
MdCo::checkMd( unsigned num )
    {
    int ret = 0;
    y2mil("num:" << num);
    MdIter i;
    if( !findMd( num, i ) )
	ret = MD_DEVICE_UNKNOWN;
    else if( i->created() )
	ret = i->checkDevices();
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::extendMd( unsigned num, const string& dev )
    {
    int ret = 0;
    y2mil("num:" << num << " dev:" << dev);
    MdIter i;
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	ret = checkUse( normalizeDevice( dev ) );
	}
    if( ret==0 )
	{
	if( !findMd( num, i ))
	    ret = MD_UNKNOWN_NUMBER;
	}
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_RESIZE_ON_DISK;
	}
    if( ret==0 )
	{
	ret = i->addDevice( dev );
	}
    if( ret==0 && !getStorage()->isDisk(dev) )
	{
	getStorage()->changeFormatVolume( dev, false, FSNONE );
	}
    if( ret==0 )
	{
	string d = normalizeDevice( dev );
	getStorage()->setUsedBy( d, UB_MD, "md"+decString(num) );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::shrinkMd( unsigned num, const string& dev )
    {
    int ret = 0;
    y2mil("num:" << num << " dev:" << dev);
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
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_RESIZE_ON_DISK;
	}
    if( ret==0 )
	{
	ret = i->removeDevice( dev );
	}
    if( ret==0 )
	{
	string d = normalizeDevice( dev );
	getStorage()->clearUsedBy(d);
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::changeMdType( unsigned num, MdType ptype )
    {
    int ret = 0;
    y2mil("num:" << num << " md_type:" << ptype);
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
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_CHANGE_ON_DISK;
	}
    if( ret==0 )
	{
	i->setPersonality( ptype );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::changeMdChunk( unsigned num, unsigned long chunk )
    {
    int ret = 0;
    y2mil("num:" << num << " chunk:" << chunk);
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
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_CHANGE_ON_DISK;
	}
    if( ret==0 )
	{
	i->setChunkSize( chunk );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
MdCo::changeMdParity( unsigned num, MdParity ptype )
    {
    int ret = 0;
    y2mil("num:" << num << " parity:" << ptype);
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
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_RESIZE_ON_DISK;
	}
    if( ret==0 )
	{
	i->setParity( ptype );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
MdCo::getMdState(unsigned num, MdStateInfo& info)
{
    int ret = 0;
    MdIter i;
    if( ret==0 )
    {
	if( !findMd( num, i ))
	    ret = MD_UNKNOWN_NUMBER;
    }
    if( ret==0 && i->created() )
    {
	ret = MD_STATE_NOT_ON_DISK;
    }
    if( ret==0 )
    {
	i->getState(info);
    }
    y2mil("ret:" << ret);
    return ret;
}

int 
MdCo::removeMd( unsigned num, bool destroySb )
    {
    int ret = 0;
    y2mil("num:" << num);
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
	    getStorage()->clearUsedBy(*s);
	if( i->created() )
	    {
	    if( !removeFromList( &(*i) ))
		ret = MD_REMOVE_CREATE_NOT_FOUND;
	    }
	else
	    {
	    i->setDeleted();
	    i->setDestroySb( destroySb );
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int MdCo::removeVolume( Volume* v )
    {
    int ret = 0;
    y2mil("name:" << v->name());
    Md * m = dynamic_cast<Md *>(v);
    if( m != NULL )
	ret = removeMd( v->nr() );
    else 
	ret = MD_REMOVE_INVALID_VOLUME;
    return( ret );
    }

void MdCo::activate( bool val, const string& tmpDir )
    {
    y2mil("old active:" << active << " val:" << val << " tmp:" << tmpDir);
    if( active!=val )
	{
	SystemCmd c;
	if( val )
	    {
	    string mdconf = tmpDir + "/mdadm.conf";
	    c.execute("echo 1 > /sys/module/md_mod/parameters/start_ro");
	    c.execute(MDADMBIN " --examine --scan --config=partitions > " + mdconf);
	    c.execute(MDADMBIN " --assemble --scan --config=" + mdconf);
	    unlink(mdconf.c_str());
	    }
	else
	    {
	    c.execute(MDADMBIN " --stop --scan");
	    }
	active = val;
	}
    Storage::waitForDevice();
    }

int 
MdCo::doCreate( Volume* v ) 
    {
    y2mil("name:" << v->name());
    Md * m = dynamic_cast<Md *>(v);
    int ret = 0;
    if( m != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( m->createText(true) );
	    }
	ret =  m->checkDevices();
	if( ret==0 )
	    {
	    list<string> devs;
	    m->getDevs( devs );
	    for( list<string>::iterator i = devs.begin(); i!=devs.end(); ++i )
		{
		getStorage()->removeDmTableTo( *i );
		}
	    }
	if( ret==0 )
	    {
	    string cmd = m->createCmd();
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		{
		ret = MD_CREATE_FAILED;
		setExtError( c );
		}
	    }
	if( ret==0 )
	    {
	    Storage::waitForDevice(m->device());
	    getMdData( m->nr() );
	    updateEntry( m );
	    bool used_as_pv = m->getUsedByType() == UB_LVM;
	    y2mil("zeroNew:" << getStorage()->getZeroNewPartitions() << " used_as_pv:" << used_as_pv);
	    if( used_as_pv || getStorage()->getZeroNewPartitions() )
		{
		ret = Storage::zeroDevice(m->device(), m->sizeK());
		}
	    }
	}
    else
	ret = MD_CREATE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::doRemove( Volume* v )
    {
    y2mil("name:" << v->name());
    Md * m = dynamic_cast<Md *>(v);
    int ret = 0;
    if( m != NULL )
	{
	if( !active )
	    activate(true, getStorage()->tmpDir());
	if( !silent )
	    {
	    getStorage()->showInfoCb( m->removeText(true) );
	    }
	ret = m->prepareRemove();
	if( ret==0 )
	    {
	    string cmd = MDADMBIN " --stop " + quote(m->device());
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		{
		ret = MD_REMOVE_FAILED;
		setExtError( c );
		}
	    }
	if( ret==0 && m->destroySb() )
	    {
	    SystemCmd c;
	    list<string> d;
	    m->getDevs( d );
	    for( list<string>::const_iterator i=d.begin(); i!=d.end(); ++i )
		{
		c.execute(MDADMBIN " --zero-superblock " + quote(*i));
		}
	    }
	if( ret==0 )
	    {
	    initTab();
	    if( tab!=NULL )
		{
		tab->removeEntry( m->nr() );
		}
	    if( !removeFromList( m ) )
		ret = MD_NOT_IN_LIST;
	    }
	}
    else
	ret = MD_REMOVE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }

void MdCo::changeDeviceName( const string& old, const string& nw )
    {
    MdPair md=mdPair();
    for( MdIter i=md.begin(); i!=md.end(); ++i )
	{
	i->changeDeviceName( old, nw );
	}
    }


std::ostream& operator<< (std::ostream& s, const MdCo& d )
    {
    s << *((Container*)&d);
    return( s );
    }


void MdCo::logDifference( const Container& d ) const
    {
    y2mil(getDiffString(d));
    const MdCo * p = dynamic_cast<const MdCo*>(&d);
    if( p != NULL )
	{
	ConstMdPair pp=mdPair();
	ConstMdIter i=pp.begin();
	while( i!=pp.end() )
	    {
	    ConstMdPair pc=p->mdPair();
	    ConstMdIter j = pc.begin();
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
	pp=p->mdPair();
	i=pp.begin();
	while( i!=pp.end() )
	    {
	    ConstMdPair pc=mdPair();
	    ConstMdIter j = pc.begin();
	    while( j!=pc.end() && 
		   (i->device()!=j->device() || i->created()!=j->created()) )
		++j;
	    if( j==pc.end() )
		y2mil( "  <--" << *i );
	    ++i;
	    }
	}
    }

bool MdCo::equalContent( const Container& rhs ) const
    {
    const MdCo * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const MdCo*>(&rhs);
    if( ret && p )
	{
	ConstMdPair pp = mdPair();
	ConstMdPair pc = p->mdPair();
	ConstMdIter i = pp.begin();
	ConstMdIter j = pc.begin();
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

MdCo::MdCo( const MdCo& rhs ) : Container(rhs)
    {
    y2deb("constructed MdCo by copy constructor from " << rhs.nm);
    *this = rhs;
    this->tab = NULL;
    ConstMdPair p = rhs.mdPair();
    for( ConstMdIter i=p.begin(); i!=p.end(); ++i )
	 {
	 Md * p = new Md( *this, *i );
	 vols.push_back( p );
	 }
    }


void
MdCo::logData(const string& Dir) const
{
}


bool MdCo::active = false;

}
