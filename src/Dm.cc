/*
  Textdomain    "storage"
*/

#include <sstream>

#include "storage/Dm.h"
#include "storage/PeContainer.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Regex.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


Dm::Dm( const PeContainer& d, const string& tn ) :
	Volume( d ), tname(tn)
    {
    num_le = 0;
    stripe = 1;
    stripe_size = 0;
    inactiv = true;
    y2deb("constructed dm dev");
    }

Dm::Dm( const PeContainer& d, const string& tn, unsigned mnum ) :
	Volume( d ), tname(tn)
    {
    y2mil("constructed dm dev table \"" << tn << "\" " << mnum);
    num_le = 0;
    stripe = 1;
    stripe_size = 0;
    inactiv = true;
    nm = tn;
    init();
    getTableInfo();

    getContainer()->getStorage()->fetchDanglingUsedBy(dev, uby);
    for (list<string>::const_iterator it = alt_names.begin(); it != alt_names.end(); ++it)
	getContainer()->getStorage()->fetchDanglingUsedBy(*it, uby);
    }


Dm::~Dm()
{
    y2deb("destructed dm dev " << dev);
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
    SystemCmd c(DMSETUPBIN " table " + quote(tname));
    inactiv = c.retcode()!=0;
    y2mil("table:" << tname << " retcode:" << c.retcode() << " numLines:" << c.numLines() <<
	  " inactive:" << inactiv);
    if( c.numLines()>0 )
	{
	string line = c.getLine(0);
	target = extractNthWord( 2, line );
	if( target=="striped" )
	    extractNthWord( 3, line ) >> stripe;
	}
    pe_map.clear();
    map<string,unsigned long>::iterator mit;
    unsigned long long pesize = pec()->peSize();
    y2mil("pesize:" << pesize );
    for( unsigned i=0; i<c.numLines(); i++ )
	{
	unsigned long le;
	string dev;
	string majmin;
	string line = c.getLine(i);
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
		y2war("could not find major/minor pair " << majmin);
	    }
	else if (target == "snapshot-origin")
	{
	    // AFAIS snapshot-origins do not really use space.
	}
	else if (target == "snapshot")
	{
	    // AFAIS think snapshots do not really use space.
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
		y2war("invalid stripe count " << str);
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
			y2war("could not find major/minor pair " << majmin);
		    }
		}
	    }
	else 
	    {
	    if( find( known_types.begin(), known_types.end(), target ) == 
	        known_types.end() )
		y2war("unknown target type \"" << target << "\"");
	    extractNthWord( 1, line ) >> le;
	    y2mil( "le:" << le );
	    le /= 2;
	    y2mil( "le:" << le );
	    if( pesize>0 )
		{
		le += pesize-1;
		y2mil( "le:" << le );
		le /= pesize;
		y2mil( "le:" << le );
		}
	    list<string> sl = splitString( extractNthWord( 2, line, true ));
	    y2mil( "sl:" << sl );
	    Regex devspec( "^[0123456789]+:[0123456789]+$" );
	    for( list<string>::const_iterator i=sl.begin(); i!=sl.end(); ++i )
		{
		if( devspec.match( *i ))
		    {
		    y2mil( "match \"" << *i << "\"" );
		    dev = getDevice( *i );
		    if( !dev.empty() )
			{
			if( (mit=pe_map.find( dev ))==pe_map.end() )
			    pe_map[dev] = le;
			else
			    mit->second += le;
			}
		    else
			y2war("could not find major/minor pair " << majmin);
		    }
		}
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
	unsigned mi = 0;
	string pair( majmin );
	SystemCmd c;
	do
	    {
	    mj = mi = 0;
	    string::size_type pos = pair.find( ':' );
	    if( pos != string::npos )
		pair[pos] = ' ';
	    istringstream i( pair );
	    classic(i);
	    i >> mj >> mi;
	    list<string> ls = splitString(pair);
	    if( cont->majorNr()>0 && mj==cont->majorNr() && mi==cont->minorNr())
		ret = cont->device();
	    if( mj==Loop::major() )
		ret = Loop::loopDeviceName(mi);
	    if( ret.empty() && mj==dmMajor() && ls.size()>=2 )
		{
		c.execute(DMSETUPBIN " info -c --noheadings -j " + *ls.begin() +
			  " -m " + *(++ls.begin()) + " | sed -e \"s/:.*//\"" );
		if( c.retcode()==0 && c.numLines()>0 )
		    {
		    string tmp = "/dev/" + c.getLine(0);
		    if( cont->getStorage()->knownDevice( tmp, true ) )
			{
			ret = tmp;
			}
		    else
			{
			c.execute(DMSETUPBIN " table " + quote(c.getLine(0)));
			if( c.retcode()==0 && c.numLines()>0 )
			    {
			    pair = extractNthWord( 3, c.getLine(0) );
			    ret = cont->getStorage()->deviceByNumber( pair );
			    }
			}
		    }
		}
	    }
	while( ret.empty() && mj==dm_major && c.retcode()==0 );
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
	while( mit!=pe_map.end() && !r.match( mit->first ) && 
	       !pec()->addedPv(mit->first) )
	    ++mit;
	}
    else
	{
	mit = pe_map.find( dev );
	if( mit != pe_map.end() && pec()->addedPv(mit->first) )
	    mit = pe_map.end();
	}
    ret = mit != pe_map.end();
    if( ret )
	{
	y2mil("map:" << pe_map);
	y2mil("table:" << tname << " dev:" << dev << " ret:" << ret);
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
	y2war("lv:" << dev << " sum:" << sum << " num:" << num_le);
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
    else if( dmn != dev )
	alt_names.push_back( dmn );
    //alt_names.push_back( "/dev/"+tname );
    updateMajorMinor();
    }

void Dm::updateMajorMinor()
    {
    getMajorMinor( dev, mjr, mnr );
    if( majorNr()==Dm::dmMajor() )
	{
	string d = "/dev/dm-" + decString(minorNr());
	if( d!=dev )
	    replaceAltName( "/dev/dm-", d );
	}
    num = mnr;
    }

const PeContainer* Dm::pec() const
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


void Dm::setPeMap( const std::map<string,unsigned long>& m )
{
    pe_map = m;

    // remove usused entries
    // TODO: do not generate pe_maps with unused entries in the first place
    for(std::map<string,unsigned long>::iterator i = pe_map.begin(); i != pe_map.end(); ) {
	if (i->second == 0)
	    pe_map.erase(i++);
	else
	    ++i;
    }

    y2mil("device:" << device() << " pe_map:" << pe_map);
}


void Dm::changeDeviceName( const string& old, const string& nw )
    {
    map<string,unsigned long>::const_iterator mit = pe_map.find( old );
    if( mit != pe_map.end() )
	{
        pe_map[nw] = mit->second;
	pe_map.erase(mit->first);
	}
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
    y2mil("old active:" << active << " val:" << val);
    if( active!=val )
	{
	SystemCmd c;
	if( val )
	    {
	    c.execute(DMSETUPBIN " version");
	    if( c.retcode()!=0 )
		{
		c.execute("grep \"^dm[-_]mod[ \t]\" /proc/modules");
		if( c.numLines()<=0 )
		    c.execute(MODPROBEBIN " dm-mod");
		c.execute("grep \"^dm[-_]snapshot[ \t]\" /proc/modules");
		if( c.numLines()<=0 )
		    c.execute(MODPROBEBIN " dm-snapshot");
		c.execute("/sbin/devmap_mknod.sh");
		}
	    }
	else
	    {
	    c.execute(DMSETUPBIN " remove_all");
	    }
	active = val;
	}
    }

string Dm::devToTable( const string& dev )
    {
    string ret = boost::replace_all_copy(undevDevice(dev), "/", "|");
    if( dev!=ret )
	y2mil("dev:" << dev << " ret:" << ret);
    return( ret );
    }

string Dm::dmName( const string& table )
    {
    string ret = "";
    int num = Dm::dmNumber( table );
    if( num>=0 )
	ret = "dm-" + decString(num);
    y2mil( "table:" << table << " ret:" << ret );
    return( ret );
    }

string Dm::dmDeviceName( unsigned long num )
    {
    return( "/dev/dm-" + decString(num));
    }

int Dm::dmNumber( const string& table )
    {
    int ret = -1;
    SystemCmd c(DMSETUPBIN " -c --noheadings info " + quote(table));
    if( c.retcode()==0 && c.numLines()>0 )
	{
	list<string> sl = splitString( c.getLine(0), ":" );
	if( sl.size()>=3 )
	    {
	    list<string>::const_iterator ci = sl.begin();
	    ++ci;
	    ++ci;
	    *ci >> ret;
	    }
	}
    y2mil( "table:" << table << " ret:" << ret );
    return( ret );
    }

string Dm::sysfsPath() const
    {
    string ret = SYSFSDIR "/";
    list<string>::const_iterator i = 
	find_if( alt_names.begin(), alt_names.end(), string_starts_with( "/dev/dm-" ) );
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
    y2mil("dm_major:" << dm_major);
    }

void Dm::getInfo( DmInfo& tinfo ) const
    {
    ((Volume*)this)->getInfo( info.v );
    info.nr = num;
    info.table = tname;
    info.target = target;
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Dm &p )
    {
    s << p.shortPrintedName() << " ";
    s << *(Volume*)&p;
    if( p.num_le>0 )
	s << " LE:" << p.num_le;
    s << " Table:" << p.tname;
    if( !p.target.empty() )
	s << " Target:" << p.target;
    if( p.inactiv )
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
    

bool Dm::equalContent( const Dm& rhs ) const
    {
    return( Volume::equalContent(rhs) &&
            num_le==rhs.num_le && stripe==rhs.stripe && inactiv==rhs.inactiv &&
            stripe_size==rhs.stripe_size && pe_map==rhs.pe_map );
    }

void Dm::logDifference( const Dm& rhs ) const
{
    string log = stringDifference(rhs);
    y2mil(log);
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
	classic(b);
	b << " pe_map:" << pe_map << "-->" << rhs.pe_map;
	ret += b.str();
	}
    return( ret );
    }

Dm& Dm::operator= ( const Dm& rhs )
    {
    y2deb("operator= from " << rhs.nm);
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
    y2deb("constructed dm by copy constructor from " << rhs.dev);
    *this = rhs;
    }

bool Dm::active = false;
unsigned Dm::dm_major = 0;
static const char* elem[] = { "crypt" };
list<string> Dm::known_types( elem, elem+lengthof(elem) );

}
