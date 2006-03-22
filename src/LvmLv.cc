/*
  Textdomain    "storage"
*/

#include <sstream>

#include "y2storage/LvmLv.h"
#include "y2storage/LvmVg.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

using namespace storage;
using namespace std;

LvmLv::LvmLv( const LvmVg& d, const string& name, unsigned long le,
	      const string& uuid, const string& stat, const string& alloc ) :
	Dm( d, dupDash(d.name())+"-"+dupDash(name) )
    {
    init( name );
    setUuid( uuid );
    setStatus( stat );
    setAlloc( alloc );
    setLe( le );
    calcSize();
    getTableInfo();
    y2debug( "constructed lvm lv %s on vg %s", dev.c_str(),
	     cont->name().c_str() );
    }

LvmLv::LvmLv( const LvmVg& d, const string& name, unsigned long le,
	      unsigned str ) :
	Dm( d, dupDash(d.name())+"-"+dupDash(name) )
    {
    init( name );
    setLe( le );
    calcSize();
    stripe = str;
    fs = detected_fs = FSNONE;
    alt_names.push_back( "/dev/mapper/" + cont->name() + "-" + dupDash(name) );
    y2debug( "constructed lvm lv %s on vg %s", dev.c_str(),
	     cont->name().c_str() );
    }

LvmLv::~LvmLv()
    {
    y2debug( "destructed lvm lv %s", dev.c_str() );
    }

void LvmLv::init( const string& name )
    {
    nm = name;
    dev = normalizeDevice( cont->name() + "/" + name );
    Dm::init();
    }

string LvmLv::removeText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	txt = sformat( _("Deleting logical volume %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete logical volume %1$s (%2$s)"), dev.c_str(),
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
	txt = sformat( _("Creating logical volume %1$s"), dev.c_str() );
	}
    else
	{
	if( mp=="swap" )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create swap logical volume %1$s (%2$s)"),
	                   dev.c_str(), sizeString().c_str() );
	    }
	else if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create encrypted logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create logical volume %1$s (%2$s)"),
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
	txt = sformat( _("Formatting logical volume %1$s (%2$s) with %3$s"),
		       dev.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( !mp.empty() )
	    {
	    if( mp=="swap" )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Format logical volume %1$s (%2$s) for swap"),
			       dev.c_str(), sizeString().c_str() );
		}
	    else if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format logical volume %1$s (%2$s) with %3$s"),
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
	    txt = sformat( _("Shrinking logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extending logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );

        }
    else
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrink logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extend logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );

        }
    return( txt );
    }

void LvmLv::getInfo( LvmLvInfo& tinfo ) const
    {
    info.stripe = stripe;
    info.stripe_size = stripe_size;
    info.uuid = vol_uuid;
    info.status = status;
    info.allocation = allocation;
    info.dm_table = tname;
    info.dm_target = target;
    tinfo = info;
    }

namespace storage
{

std::ostream& operator<< (std::ostream& s, const LvmLv &p )
    {
    s << *(Dm*)&p;
    if( !p.vol_uuid.empty() )
      s << " UUID:" << p.vol_uuid;
    if( !p.status.empty() )
      s << " " << p.status;
    if( !p.allocation.empty() )
      s << " " << p.allocation;
    return( s );
    }

}

bool LvmLv::equalContent( const LvmLv& rhs ) const
    {
    return( Dm::equalContent(rhs) &&
            vol_uuid==rhs.vol_uuid && status==rhs.status && 
            allocation==rhs.allocation );
    }

void LvmLv::logDifference( const LvmLv& rhs ) const
    {
    string log = stringDifference( rhs );
    if( vol_uuid!=rhs.vol_uuid )
	log += " UUID:" + vol_uuid + "-->" + rhs.vol_uuid;
    if( status!=rhs.status )
	log += " Status:" + status + "-->" + rhs.status;
    if( allocation!=rhs.allocation )
	log += " Alloc:" + allocation + "-->" + rhs.allocation;
    y2milestone( "%s", log.c_str() );
    }

LvmLv& LvmLv::operator= ( const LvmLv& rhs )
    {
    y2debug( "operator= from %s", rhs.nm.c_str() );
    *((Dm*)this) = rhs;
    vol_uuid = rhs.vol_uuid;
    status = rhs.status;
    allocation = rhs.allocation;
    return( *this );
    }

LvmLv::LvmLv( const LvmVg& d, const LvmLv& rhs ) : Dm(d,rhs)
    {
    y2debug( "constructed lvm lv by copy constructor from %s", 
	     rhs.dev.c_str() );
    *this = rhs;
    }

