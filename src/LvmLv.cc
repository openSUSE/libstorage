#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/LvmLv.h"
#include "y2storage/LvmVg.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

LvmLv::LvmLv( const LvmVg& d, const string& name, unsigned long le,
	      const string& uuid, const string& stat, const string& alloc ) :
	Volume( d, name, 0 )
    {
    init();
    setUuid( uuid );
    setStatus( stat );
    setAlloc( alloc );
    setLe( le );
    calcSize();
    getTableInfo();
    if( majorNr()>0 )
	{
	alt_names.push_back( "/dev/dm-" + decString(minorNr()) );
	}
    alt_names.push_back( "/dev/mapper/" + cont->name() + "-" + name );
    y2milestone( "constructed lvm lv %s on vg %s", dev.c_str(),
                 cont->name().c_str() );
    }

LvmLv::LvmLv( const LvmVg& d, const string& name, unsigned long le,
	      unsigned str ) :
	Volume( d, name, 0 )
    {
    init();
    setLe( le );
    calcSize();
    stripe = str;
    fs = detected_fs = FSNONE;
    if( majorNr()>0 )
	{
	alt_names.push_back( "/dev/dm-" + decString(minorNr()) );
	}
    alt_names.push_back( "/dev/mapper/" + cont->name() + "-" + name );
    y2milestone( "constructed lvm lv %s on vg %s", dev.c_str(),
                 cont->name().c_str() );
    }

LvmLv::~LvmLv()
    {
    y2milestone( "destructed lvm lv %s", dev.c_str() );
    }

void
LvmLv::getTableInfo()
    {
    SystemCmd c( "dmsetup table " + cont->name() + "-" + name() );
    if( c.numLines()>0 )
	{
	string line = *c.getLine(0);
	if( extractNthWord( 2, line )=="striped" )
	    extractNthWord( 3, line ) >> stripe;
	}
    pe_map.clear();
    map<string,unsigned long>::iterator mit;
    unsigned long long pesize = vg()->peSize();
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
	    dev = cont->getStorage()->deviceByNumber( majmin );
	    if( dev.size()>0 )
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
		    dev = cont->getStorage()->deviceByNumber( majmin );
		    if( dev.size()>0 )
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

unsigned long 
LvmLv::usingPe( const string& dev ) const
    {
    unsigned long ret = 0;
    map<string,unsigned long>::const_iterator mit = pe_map.find( dev );
    if( mit!=pe_map.end() )
	ret = mit->second;
    return( ret );
    }

bool 
LvmLv::checkConsistency() const
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
LvmLv::setLe( unsigned long le )
    {
    num_le = le;
    }

void LvmLv::init()
    {
    string::size_type pos = nm.rfind( "/" );
    if( pos != string::npos )
	nm.erase( 0, pos+1 );
    num_le = 0;
    stripe = 1;
    }

const LvmVg* const LvmLv::vg() const
    { 
    return(dynamic_cast<const LvmVg* const>(cont));
    }
	        
void LvmLv::calcSize()
    {
    setSize( num_le*vg()->peSize() );
    }

string LvmLv::removeText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	txt = sformat( _("Deleting Logical volume %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete Logical volume %1$s %2$s"), dev.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

string LvmLv::createText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	txt = sformat( _("Creating Logical volume %1$s"), dev.c_str() );
	}
    else
	{
	if( mp=="swap" )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create swap Logical volume %1$s %2$s"),
	                   dev.c_str(), sizeString().c_str() );
	    }
	else if( mp.size()>0 )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create Logical volume %1$s %2$s for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create crypted Logical volume %1$s %2$s for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create Logical volume %1$s %2$s"),
			   dev.c_str(), sizeString().c_str() );
	    }
	}
    return( txt );
    }

string LvmLv::formatText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting Logical volume %1$s %2$s with %3$s "),
		       dev.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( mp.size()>0 )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format Logical volume %1$s %2$s for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format crypted Logical volume %1$s %2$s for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format Logical volume %1$s %2$s with %3$s"),
			   dev.c_str(), sizeString().c_str(), 
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

string LvmLv::resizeText( bool doing ) const
    {
    string txt;
    if( doing )
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrinking Logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extending Logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );

        }
    else
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrink Logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extend Logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );

        }
    return( txt );
    }
