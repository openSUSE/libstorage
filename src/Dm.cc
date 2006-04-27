/*
  Textdomain    "storage"
*/

#include <sstream>

#include "y2storage/Dm.h"
#include "y2storage/PeContainer.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Regex.h"
#include "y2storage/Storage.h"

using namespace storage;
using namespace std;

Dm::Dm( const PeContainer& d, const string& tn ) :
	Volume( d ), tname(tn)
    {
    num_le = 0;
    stripe = 1;
    stripe_size = 0;
    inactiv = true;
    y2debug( "constructed dm dev" );
    }

Dm::Dm( const PeContainer& d, const string& tn, unsigned mnum ) :
	Volume( d ), tname(tn)
    {
    y2milestone( "constructed dm dev table \"%s\" %u", tn.c_str(), mnum );
    num_le = 0;
    stripe = 1;
    stripe_size = 0;
    inactiv = true;
    dev = "/dev/dm-" + decString(mnum);
    nm = tn;
    init();
    getTableInfo();
    }

Dm::~Dm()
    {
    y2debug( "destructed dem dev %s", dev.c_str() );
    }

unsigned Dm::dmMajor()
    {
    if( dm_major==0 )
	getDmMajor();
    return( dm_major );
    }

void
Dm::getTableInfo()
    {
    if( dm_major==0 )
	getDmMajor();
    SystemCmd c( "dmsetup table \"" + tname + "\"" );
    inactiv = c.retcode()!=0;
    y2milestone( "table %s retcode:%d numLines:%u inactive:%d", 
                 tname.c_str(), c.retcode(), c.numLines(), inactiv );
    if( c.numLines()>0 )
	{
	string line = *c.getLine(0);
	target = extractNthWord( 2, line );
	if( target=="striped" )
	    extractNthWord( 3, line ) >> stripe;
	}
    pe_map.clear();
    map<string,unsigned long>::iterator mit;
    unsigned long long pesize = pec()->peSize();
    for( unsigned i=0; i<c.numLines(); i++ )
	{
	unsigned long le;
	string dev;
	string majmin;
	string line = *c.getLine(i);
	if( target=="linear" )
	    {
	    extractNthWord( 1, line ) >> le;
	    le /= 2;
	    le += pesize-1;
	    le /= pesize;
	    majmin = extractNthWord( 3, line );
	    dev = getDevice( majmin );
	    if( !dev.empty() )
		{
		if( (mit=pe_map.find( dev ))==pe_map.end() )
		    pe_map[dev] = le;
		else
		    mit->second += le;
		}
	    else
		y2warning( "could not find major/minor pair %s", 
			majmin.c_str());
	    }
	else if( target=="striped" )
	    {
	    unsigned str;
	    extractNthWord( 1, line ) >> le;
	    le /= 2;
	    le += pesize-1;
	    le /= pesize;
	    extractNthWord( 4, line ) >> stripe_size;
	    stripe_size /= 2;
	    extractNthWord( 3, line ) >> str;
	    if( str<2 )
		y2warning( "invalid stripe count %u", str );
	    else
		{
		le = (le+str-1)/str;
		for( unsigned j=0; j<str; j++ )
		    {
		    majmin = extractNthWord( 5+j*2, line );
		    dev = getDevice( majmin );
		    if( !dev.empty() )
			{
			if( (mit=pe_map.find( dev ))==pe_map.end() )
			    pe_map[dev] = le;
			else
			    mit->second += le;
			}
		    else
			y2warning( "could not find major/minor pair %s", 
				majmin.c_str());
		    }
		}
	    }
	else
	    {
	    y2warning( "unknown target type \"%s\"", target.c_str() );
	    }
	}
    }

bool Dm::removeTable()
    {
    return( getContainer()->getStorage()->removeDmTable( tname ));
    }

string Dm::getDevice( const string& majmin )
    {
    string ret = cont->getStorage()->deviceByNumber( majmin );
    if( ret.empty() )
	{
	unsigned mj = 0;
	string pair( majmin );
	SystemCmd c;
	do
	    {
	    string::size_type pos = pair.find( ':' );
	    if( pos != string::npos )
		pair[pos] = ' ';
	    pair >> mj;
	    if( mj==dm_major )
		{
		c.execute( "devmap_name " + pair );
		if( c.retcode()==0 && c.numLines()>0 )
		    {
		    string tmp = "/dev/"+*c.getLine(0);
		    if( cont->getStorage()->knownDevice( tmp, true ) )
			{
			ret = tmp;
			}
		    else
			{
			c.execute( "dmsetup table \"" + *c.getLine(0) + "\"" );
			if( c.retcode()==0 && c.numLines()>0 )
			    {
			    pair = extractNthWord( 3, *c.getLine(0) );
			    ret = cont->getStorage()->deviceByNumber( pair );
			    }
			}
		    }
		}
	    }
	while( ret.empty() && mj==dm_major && c.retcode()==0 );
	}
    if( ret.find( "/dev/evms/" )==0 )
	{
	string tmp( ret );
	tmp.erase( 5, 5 );
	if( cont->getStorage()->knownDevice( tmp, true ) )
	    ret = tmp;
	}
    return( ret );
    }

unsigned long long
Dm::usingPe( const string& dev ) const
    {
    unsigned long ret = 0;
    map<string,unsigned long>::const_iterator mit = pe_map.find( dev );
    if( mit!=pe_map.end() )
	ret = mit->second;
    return( ret );
    }

bool
Dm::mapsTo( const string& dev ) const
    {
    bool ret = false;
    map<string,unsigned long>::const_iterator mit;
    if( dev.find_first_of( "[.*^$" ) != string::npos )
	{
	Regex r(dev);
	mit = pe_map.begin();
	while( mit!=pe_map.end() && !r.match( mit->first ))
	    ++mit;
	}
    else
	{
	mit = pe_map.find( dev );
	}
    ret = mit != pe_map.end();
    if( ret )
	{
	y2mil( "map:" << pe_map );
	y2milestone( "table:%s dev:%s ret:%d", tname.c_str(), dev.c_str(), ret );
	}
    return( ret );
    }

bool 
Dm::checkConsistency() const
    {
    bool ret = false;
    unsigned long sum = 0;
    for( map<string,unsigned long>::const_iterator mit=pe_map.begin();
         mit!=pe_map.end(); ++mit )
	 sum += mit->second;
    if( sum != num_le )
	y2warning( "lv:%s sum:%lu num:%llu", dev.c_str(), sum, num_le );
    else
	ret = true;
    return( ret );
    }

void
Dm::setLe( unsigned long long le )
    {
    num_le = le;
    }

void Dm::init()
    {
    string dmn = "/dev/mapper/" + tname;
    if( dev.empty() )
	{
	dev = dmn;
	nm = tname;
	}
    else
	alt_names.push_back( dmn );
    //alt_names.push_back( "/dev/"+tname );
    updateMajorMinor();
    }

void Dm::updateMajorMinor()
    {
    getMajorMinor( dev, mjr, mnr );
    if( majorNr()>0 )
	{
	string d = "/dev/dm-" + decString(minorNr());
	if( d!=dev )
	    {
	    list<string>::iterator i = 
	        find_if( alt_names.begin(), alt_names.end(), 
		         find_begin( "/dev/dm-" ) );
	    if( i!=alt_names.end() )
		{
		if( *i != d )
		    *i = d;
		}
	    else
		alt_names.push_back( d );
	    }
	}
    }

const PeContainer* const Dm::pec() const
    { 
    return(dynamic_cast<const PeContainer* const>(cont));
    }

void Dm::modifyPeSize( unsigned long long old, unsigned long long neww )
    {
    num_le = num_le * old / neww;
    calcSize();
    for( map<string,unsigned long>::iterator mit=pe_map.begin();
         mit!=pe_map.end(); ++mit )
	 mit->second = mit->second * old / neww;
    }
	        
void Dm::calcSize()
    {
    setSize( num_le*pec()->peSize() );
    }

string Dm::removeText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/mapper/system
	txt = sformat( _("Deleting device mapper volume %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete device mapper volume %1$s (%2$s)"), dev.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

string Dm::formatText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/mapper/system
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting device mapper volume %1$s (%2$s) with %3$s "),
		       dev.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format device mapper volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted device mapper volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format device mapper volume %1$s (%2$s) with %3$s"),
			   dev.c_str(), sizeString().c_str(), 
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

void Dm::activate( bool val )
    {
    y2milestone( "old active:%d val:%d", active, val );
    if( active!=val )
	{
	SystemCmd c;
	if( val )
	    {
	    c.execute( "dmsetup version" );
	    if( c.retcode()!=0 )
		{
		c.execute(" grep \"^dm[-_]mod[ 	]\" /proc/modules" );
		if( c.numLines()<=0 )
		    c.execute( "modprobe dm-mod" );
		c.execute(" grep \"^dm[-_]snapshot[ 	]\" /proc/modules" );
		if( c.numLines()<=0 )
		    c.execute( "modprobe dm-snapshot" );
		c.execute( "/sbin/devmap_mknod.sh" );
		}
	    }
	else
	    {
	    c.execute( "dmsetup remove_all" );
	    }
	active = val;
	}
    }

string Dm::devToTable( const string& dev )
    {
    string ret(undevDevice(dev));
    string::iterator it = ret.begin();
    while( it!=ret.end() )
	{
	if( *it == '/' )
	    *it = '|';
	++it;
	}
    if( dev!=ret )
	y2milestone( "dev:%s --> %s", dev.c_str(), ret.c_str() );
    return( ret );
    }

string Dm::sysfsPath() const
    {
    string ret = Storage::sysfsDir() + "/";
    list<string>::const_iterator i = 
	find_if( alt_names.begin(), alt_names.end(), find_begin( "/dev/dm-" ) );
    if( i != alt_names.end() )
	{
	string::size_type pos = i->rfind( '/' ) + 1;
	ret += i->substr( pos );
	}
    else
	{
	y2mil( "no dm device found " << *this );
	}
    y2mil( "ret:" << ret );
    return( ret );
    }

void Dm::getDmMajor()
    {
    dm_major = getMajorDevices( "device-mapper" );
    y2milestone( "dm_major:%u", dm_major );
    }

void Dm::getInfo( DmInfo& tinfo ) const
    {
    info.nr = num;
    info.table = tname;
    info.target = target;
    tinfo = info;
    }

namespace storage
{

std::ostream& operator<< (std::ostream& s, const Dm &p )
    {
    s << p.shortPrintedName() << " ";
    s << *(Volume*)&p;
    s << " LE:" << p.num_le;
    s << " Table:" << p.tname;
    if( p.inactiv>1 )
      {
      s << " inactive";
      }
    if( p.stripe>1 )
      {
      s << " Stripes:" << p.stripe;
      if( p.stripe_size>0 )
	s << " StripeSize:" << p.stripe_size;
      }
    if( !p.pe_map.empty() )
      s << " pe_map:" << p.pe_map;
    return( s );
    }
    
}

bool Dm::equalContent( const Dm& rhs ) const
    {
    return( Volume::equalContent(rhs) &&
            num_le==rhs.num_le && stripe==rhs.stripe && inactiv==rhs.inactiv &&
            stripe_size==rhs.stripe_size && pe_map==rhs.pe_map );
    }

void Dm::logDifference( const Dm& rhs ) const
    {
    string log = stringDifference( rhs );
    y2milestone( "%s", log.c_str() );
    }

string Dm::stringDifference( const Dm& rhs ) const
    {
    string ret = Volume::logDifference( rhs );
    if( num_le!=rhs.num_le )
	ret += " LE:" + decString(num_le) + "-->" + decString(rhs.num_le);
    if( stripe!=rhs.stripe )
	ret += " Stripes:" + decString(stripe) + "-->" + decString(rhs.stripe);
    if( stripe_size!=rhs.stripe_size )
	ret += " StripeSize:" + decString(stripe_size) + "-->" + 
	       decString(rhs.stripe_size);
    if( pe_map!=rhs.pe_map )
	{
	std::ostringstream b;
	b << " pe_map:" << pe_map << "-->" << rhs.pe_map;
	ret += b.str();
	}
    return( ret );
    }

Dm& Dm::operator= ( const Dm& rhs )
    {
    y2debug( "operator= from %s", rhs.nm.c_str() );
    *((Volume*)this) = rhs;
    num_le = rhs.num_le;
    stripe = rhs.stripe;
    stripe_size = rhs.stripe_size;
    inactiv = rhs.inactiv;
    tname = rhs.tname;
    pe_map = rhs.pe_map;
    return( *this );
    }

Dm::Dm( const PeContainer& d, const Dm& rhs ) : Volume(d)
    {
    y2debug( "constructed dm by copy constructor from %s", rhs.dev.c_str() );
    *this = rhs;
    }

bool Dm::active = false;
unsigned Dm::dm_major = 0;

