/*
  Textdomain    "storage"
*/

#include <ostream>
#include <sstream> 

#include "storage/DmCo.h"
#include "storage/Dm.h"
#include "storage/SystemCmd.h"
#include "storage/ProcPart.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


DmCo::DmCo( Storage * const s, bool detect, ProcPart& ppart ) :
    PeContainer(s,staticType())
    {
    y2deb("constructing DmCo detect:" << detect);
    init();
    if( detect )
	getDmData( ppart );
    }

DmCo::DmCo( Storage * const s, const string& file ) :
    PeContainer(s,staticType())
    {
    y2deb("constructing DmCo file:" << file);
    init();
    }

DmCo::~DmCo()
    {
    y2deb("destructed DmCo");
    }

void DmCo::updateDmMaps()
    {
    DmPair dp = dmPair();
    bool success;
    do
	{
	success = false;
	for( DmIter i=dp.begin(); i!=dp.end(); ++i )
	    {
	    if( i->getPeMap().empty() )
		{
		y2mil( "dm:" << *i );
		i->getTableInfo();
		if( !i->getPeMap().empty() )
		    {
		    success = true;
		    y2mil( "dm:" << *i );
		    }
		}
	    }
	}
    while( success );
    }

void
DmCo::init()
    {
    nm = "dm";
    dev = "/dev/mapper";
    }


// dev should be something like /dev/mapper/cr_test
storage::EncryptType
DmCo::detectEncryption( const string& dev ) const
{
    storage::EncryptType ret = ENC_UNKNOWN;

    if( dev.substr( 0, 12 ) == "/dev/mapper/")
    {
	string tdev = dev.substr (12);
	SystemCmd c(CRYPTSETUPBIN " status " + quote(tdev));

	string cipher, keysize;
	for( unsigned int i = 0; i < c.numLines(); i++)
	{
	    string line = c.getLine(i);
	    string key = extractNthWord( 0, line );
	    if( key == "cipher:" )
		cipher = extractNthWord( 1, line );
	    if( key == "keysize:" )
		keysize = extractNthWord( 1, line );
	}

	if( cipher == "aes-cbc-essiv:sha256" )
	    ret = ENC_LUKS;
	else if( cipher == "twofish-cbc-plain" )
	    ret = ENC_TWOFISH;
	else if( cipher == "twofish-cbc-null" && keysize == "192" )
	    ret = ENC_TWOFISH_OLD;
	else if( cipher == "twofish-cbc-null" && keysize == "256" )
	    ret = ENC_TWOFISH256_OLD;
    }

    y2mil("ret:" << ret);
    return ret;
}


void
DmCo::getDmData( ProcPart& ppart )
    {
    Storage::ConstLvmLvPair lv = getStorage()->lvmLvPair();
    Storage::ConstDmraidCoPair dmrco = getStorage()->dmraidCoPair();
    Storage::ConstDmraidPair dmr = getStorage()->dmrPair();
    Storage::ConstDmmultipathCoPair dmmco = getStorage()->dmmultipathCoPair();
    Storage::ConstDmmultipathPair dmm = getStorage()->dmmPair();
    y2mil("begin");
    SystemCmd c(DMSETUPBIN " ls | grep \"(.*)\"" );
    for( unsigned i=0; i<c.numLines(); ++i )
	{
	string line = c.getLine(i);
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
	    Storage::ConstDmraidCoIterator i=dmrco.begin();
	    while( !found && i!=dmrco.end() )
		{
		found = i->name()==table;
		++i;
		}
	    }
	if( !found )
	    {
	    Storage::ConstDmraidIterator i=dmr.begin();
	    while( !found && i!=dmr.end() )
		{
		found = i->getTableName()==table;
		++i;
		}
	    }
	if( !found )
	    {
	    Storage::ConstDmmultipathCoIterator i=dmmco.begin();
	    while( !found && i!=dmmco.end() )
		{
		found = i->name()==table;
		++i;
		}
	    }
	if( !found )
	    {
	    Storage::ConstDmmultipathIterator i=dmm.begin();
	    while( !found && i!=dmm.end() )
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
	    y2mil( "minor:\"" << minor << "\" minor:" << min_num );
	    Dm * m = new Dm( *this, table, min_num );
	    y2mil( "new Dm:" << *m  );
	    unsigned long long s = 0;
	    string dev = "/dev/dm-" + decString(min_num);
	    if( ppart.getSize( dev, s ))
		{
		y2mil( "new dm size:" << s );
		m->setSize( s );
		}
	    bool in_use = false;
	    const map<string,unsigned long>& pe = m->getPeMap();
	    bool multipath = m->getTargetName()=="multipath" ||
			     m->getTargetName()=="emc";
	    map<string,unsigned long>::const_iterator it;
	    for( it=pe.begin(); it!=pe.end(); ++it )
		{
		if( !getStorage()->canUseDevice( it->first, true ))
		    in_use = true;
		if( !in_use || multipath )
		    getStorage()->setUsedBy( it->first, UB_DM, table );
		}
	    string tmp = m->device();
	    tmp.erase( 5, 7 );
	    bool skip = getStorage()->knownDevice( tmp, true );
	    y2mil( "in_use:" << in_use << " multipath:" << multipath <<
	           " known " << tmp << " is:" << skip );
	    it=pe.begin();
	    if( !skip && m->getTargetName()=="crypt" && it!=pe.end() &&
		getStorage()->knownDevice( it->first ))
		{
		skip = true;
		getStorage()->setDmcryptData( it->first, m->device(), min_num,
		                              m->sizeK(), detectEncryption (m->device()) );
		getStorage()->clearUsedBy(it->first);
		}
	    if( !skip && m->sizeK()>0 )
		addDm( m );
	    else
		delete( m );
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
	y2war("addDm already exists " << m->nr());
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
    y2mil("tname:" << tname);
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
		getStorage()->clearUsedBy(it->first);
		}
	    i->setDeleted();
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int DmCo::removeVolume( Volume* v )
    {
    int ret = 0;
    y2mil("name:" << v->name());
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
    y2mil("name:" << v->name());
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
	    string cmd = DMSETUPBIN " remove " + quote(m->getTableName());
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		ret = DM_REMOVE_FAILED;
	    else
		Storage::waitForDevice();
	    y2mil( "this:" << *this );
	    getStorage()->logProcData( cmd );
	    }
	if( ret==0 )
	    {
	    if( !removeFromList( m ) )
		ret = DM_NOT_IN_LIST;
	    }
	}
    else
	ret = DM_REMOVE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }


    std::ostream& operator<<(std::ostream& s, const DmCo& d)
    {
    s << *((Container*)&d);
    return( s );
    }


void DmCo::logDifference( const Container& d ) const
    {
    y2mil(getDiffString(d));
    const DmCo * p = dynamic_cast<const DmCo*>(&d);
    if( p != NULL )
	{
	ConstDmPair pp=dmPair();
	ConstDmIter i=pp.begin();
	while( i!=pp.end() )
	    {
	    ConstDmPair pc=p->dmPair();
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
	pp=p->dmPair();
	i=pp.begin();
	while( i!=pp.end() )
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
    }

bool DmCo::equalContent( const Container& rhs ) const
    {
    const DmCo * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const DmCo*>(&rhs);
    if( ret && p )
	{
	ConstDmPair pp = dmPair();
	ConstDmPair pc = p->dmPair();
	ConstDmIter i = pp.begin();
	ConstDmIter j = pc.begin();
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

DmCo::DmCo( const DmCo& rhs ) : PeContainer(rhs)
    {
    y2deb("constructed DmCo by copy constructor from " << rhs.nm);
    ConstDmPair p = rhs.dmPair();
    for( ConstDmIter i = p.begin(); i!=p.end(); ++i )
	{
	Dm * p = new Dm( *this, *i );
	vols.push_back( p );
        }
    }


void
DmCo::logData(const string& Dir) const
{
}


}
