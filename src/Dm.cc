#include <sstream>

#include "y2storage/Dm.h"
#include "y2storage/PeContainer.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

using namespace storage;
using namespace std;

Dm::Dm( const PeContainer& d, const string& tn ) :
	Volume( d ), tname(tn)
    {
    num_le = 0;
    stripe = 1;
    y2milestone( "constructed dm dev" );
    }

Dm::~Dm()
    {
    y2milestone( "destructed dem dev %s", dev.c_str() );
    }

void
Dm::getTableInfo()
    {
    if( dm_major==0 )
	getDmMajor();
    SystemCmd c( "dmsetup table \"" + tname + "\"" );
    if( c.numLines()>0 )
	{
	string line = *c.getLine(0);
	if( extractNthWord( 2, line )=="striped" )
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
	string type = extractNthWord( 2, line );
	if( type=="linear" )
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
	else if( type=="striped" )
	    {
	    unsigned str;
	    extractNthWord( 1, line ) >> le;
	    extractNthWord( 3, line ) >> str;
	    le /= 2;
	    le += pesize-1;
	    le /= pesize;
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
	    y2warning( "unknown table type \"%s\"", type.c_str() );
	    }
	}
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
		    c.execute( "dmsetup table \"" + *c.getLine(0) + "\"" );
		    if( c.retcode()==0 && c.numLines()>0 )
			{
			pair = extractNthWord( 3, *c.getLine(0) );
			ret = cont->getStorage()->deviceByNumber( pair );
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

unsigned long 
Dm::usingPe( const string& dev ) const
    {
    unsigned long ret = 0;
    map<string,unsigned long>::const_iterator mit = pe_map.find( dev );
    if( mit!=pe_map.end() )
	ret = mit->second;
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
	y2warning( "lv:%s sum:%lu num:%lu", dev.c_str(), sum, num_le );
    else
	ret = true;
    return( ret );
    }

void
Dm::setLe( unsigned long le )
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
    getMajorMinor( dev, mjr, mnr );
    if( majorNr()>0 )
	{
	alt_names.push_back( "/dev/dm-" + decString(minorNr()) );
	}
    }

const PeContainer* const Dm::pec() const
    { 
    return(dynamic_cast<const PeContainer* const>(cont));
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
	txt = sformat( _("Deleting Device mapper volume %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete Device mapper volume %1$s %2$s"), dev.c_str(),
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
	txt = sformat( _("Formatting Device mapper volume %1$s %2$s with %3$s "),
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
		txt = sformat( _("Format Device mapper volume %1$s %2$s for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format crypted Device mapper volume %1$s %2$s for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format Device mapper volume %1$s %2$s with %3$s"),
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

void Dm::getDmMajor()
    {
    SystemCmd c( "grep device-mapper /proc/devices" );
    if( c.numLines()>0 )
	{
	extractNthWord( 0, *c.getLine(0)) >> dm_major;
	y2milestone( "dm_major:%u", dm_major );
	}
    }

bool Dm::active = false;
unsigned Dm::dm_major = 0;

