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
	Dm( d, d.name()+"-"+name )
    {
    init( name );
    setUuid( uuid );
    setStatus( stat );
    setAlloc( alloc );
    setLe( le );
    calcSize();
    getTableInfo();
    y2milestone( "constructed lvm lv %s on vg %s", dev.c_str(),
                 cont->name().c_str() );
    }

LvmLv::LvmLv( const LvmVg& d, const string& name, unsigned long le,
	      unsigned str ) :
	Dm( d, d.name()+"-"+name )
    {
    init( name );
    setLe( le );
    calcSize();
    stripe = str;
    fs = detected_fs = FSNONE;
    alt_names.push_back( "/dev/mapper/" + cont->name() + "-" + name );
    y2milestone( "constructed lvm lv %s on vg %s", dev.c_str(),
                 cont->name().c_str() );
    }

LvmLv::~LvmLv()
    {
    y2milestone( "destructed lvm lv %s", dev.c_str() );
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
	else if( !mp.empty() )
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
	if( !mp.empty() )
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
