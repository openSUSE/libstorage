#include <sstream>

#include <ycp/y2log.h>
#include <sys/stat.h>

#include "y2storage/Loop.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/Container.h"
#include "y2storage/AppUtil.h"
#include "y2storage/ProcPart.h"

Loop::Loop( const Container& d, const string& LoopDev, const string& LoopFile ) : 
    Volume( d, 0, 0 )
    {
    y2milestone( "constructed loop dev:%s file:%s on container %s", 
                 LoopDev.c_str(), LoopFile.c_str(), cont->name().c_str() );
    if( d.type() != LOOP )
	y2error( "constructed loop with wrong container" );
    lfile = LoopFile;
    loop_dev = fstab_loop_dev = LoopDev;
    if( loop_dev.size()==0 )
	getFreeLoop();
    dev = loop_dev;
    if( loopStringNum( dev, num ))
	{
	setNameDev();
	getMajorMinor( dev, mjr, mnr );
	}
    is_loop = true;
    unsigned long long s = 0;
    ProcPart ppart;
    if( ppart.getSize( dev, s ))
	{
	setSize( s );
	}
    else
	{
	struct stat st;
	if( stat( lfile.c_str(), &st )>=0 )
	    setSize( st.st_size/1024 );
	}
    }

Loop::~Loop()
    {
    y2milestone( "destructed loop %s", dev.c_str() );
    }

string Loop::removeText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/loop0
	txt = sformat( _("Deleting file based loop %1$s"), d.c_str() );
	}
    else
	{
	d.erase( 0, 5 );
	// displayed text before action, %1$s is replaced by device name e.g. loop0
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete file based loop %1$s %2$s"), d.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

string Loop::createText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/loop0
	// %2$s is replaced by filename (e.g. /var/adm/secure)
	txt = sformat( _("Creating file based loop %1$s with file %2$s"), d.c_str(), lfile.c_str() );
	}
    else
	{
	d.erase( 0, 5 );
	if( mp.size()>0 )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. loop0
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create file based loop %1$s (%2$s) %3$s for %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(), 
			       fsTypeString().c_str(), mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. loop0
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create crypted file based loop %1$s (%2$s) %3$s for %5$s with %5$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(), 
			       fsTypeString().c_str(), mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. loop0
	    // %2$s is replaced by filename (e.g. /var/adm/secure)
	    // %3$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create file based loop %1$s (%2$s) %3$s"),
			   dev.c_str(), lfile.c_str(), sizeString().c_str() );
	    }
	}
    return( txt );
    }

string Loop::formatText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	// %2$s is replaced by filename (e.g. /var/adm/secure)
	// %3$s is replaced by size (e.g. 623.5 MB)
	// %4$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting file based loop %1$s (%2$s) %3$s with %4$s "),
		       d.c_str(), lfile.c_str(), sizeString().c_str(), 
		       fsTypeString().c_str() );
	}
    else
	{
	d.erase( 0, 5 );
	if( mp.size()>0 )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. loop0
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format file based loop %1$s (%2$s) %3$s for %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(), 
			       fsTypeString().c_str(), mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. loop0 
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format crypted file based loop %1$s (%2$s) %3$s for %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(), 
			       fsTypeString().c_str(), mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. loop0
	    // %2$s is replaced by filename (e.g. /var/adm/secure)
	    // %3$s is replaced by size (e.g. 623.5 MB)
	    // %4$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format file based loop %1$s (%2$s) %3$s with %4$s"),
			   d.c_str(), lfile.c_str(), sizeString().c_str(), 
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

