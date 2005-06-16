/*
  Textdomain    "storage"
*/

#include <sstream>

#include <sys/stat.h>

#include "y2storage/Loop.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/Container.h"
#include "y2storage/AppUtil.h"
#include "y2storage/ProcPart.h"
#include "y2storage/Storage.h"
#include "y2storage/SystemCmd.h"

using namespace storage;
using namespace std;

Loop::Loop( const Container& d, const string& LoopDev, const string& LoopFile ) : 
    Volume( d, 0, 0 )
    {
    y2milestone( "constructed loop dev:%s file:%s", 
                 LoopDev.c_str(), LoopFile.c_str() );
    if( d.type() != LOOP )
	y2error( "constructed loop with wrong container" );
    init();
    lfile = LoopFile;
    loop_dev = fstab_loop_dev = LoopDev;
    if( loop_dev.empty() )
	getFreeLoop();
    dev = loop_dev;
    if( loopStringNum( loop_dev, num ))
	{
	setNameDev();
	getMajorMinor( dev, mjr, mnr );
	}
    is_loop = true;
    unsigned long long s = 0;
    ProcPart ppart;
    if( ppart.getSize( loop_dev, s ))
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

Loop::Loop( const Container& d, const string& file, bool reuseExisting,
	    unsigned long long sizeK ) : Volume( d, 0, 0 )
    {
    y2milestone( "constructed loop file:%s reuseExisting:%d sizek:%llu", 
                 file.c_str(), reuseExisting, sizeK );
    if( d.type() != LOOP )
	y2error( "constructed loop with wrong container" );
    init();
    reuseFile = reuseExisting;
    lfile = file;
    getFreeLoop();
    dev = loop_dev;
    if( loopStringNum( dev, num ))
	{
	setNameDev();
	getMajorMinor( dev, mjr, mnr );
	}
    is_loop = true;
    if( reuseFile )
	{
	struct stat st;
	if( stat( lfileRealPath().c_str(), &st )>=0 )
	    setSize( st.st_size/1024 );
	else
	    reuseFile = false;
	}
    if( !reuseFile )
	setSize( sizeK );
    }

Loop::~Loop()
    {
    y2milestone( "destructed loop %s", dev.c_str() );
    }

void
Loop::init()
    {
    reuseFile = delFile = false;
    }

bool
Loop::removeFile()
    {
    bool ret = true;
    if( delFile )
	{
	ret = unlink( lfileRealPath().c_str() )==0;
	}
    return( ret );
    }

bool
Loop::createFile()
    {
    bool ret = true;
    if( !reuseFile )
	{
	string cmd = "dd if=/dev/zero of=" + lfileRealPath();
	cmd += " bs=1k count=" + decString( sizeK() );
	SystemCmd c( cmd );
	ret = c.retcode()==0;
	}
    return( ret );
    }

string 
Loop::lfileRealPath() const
    {
    return( cont->getStorage()->root() + lfile );
    }

string Loop::removeText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/loop0
	txt = sformat( _("Deleting file-based loop %1$s"), d.c_str() );
	}
    else
	{
	d.erase( 0, 5 );
	// displayed text before action, %1$s is replaced by device name e.g. loop0
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete file-based loop %1$s (%2$s)"), d.c_str(),
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
	txt = sformat( _("Creating file-based loop %1$s of file %2$s"), d.c_str(), lfile.c_str() );
	}
    else
	{
	d.erase( 0, 5 );
	if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. loop0
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create file-based loop %1$s of %2$s (%3$s) as %5$s with %4$s"),
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
		txt = sformat( _("Create encrypted file-based loop %1$s of %2$s (%3$s) as %5$s with %5$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(), 
			       fsTypeString().c_str(), mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. loop0
	    // %2$s is replaced by filename (e.g. /var/adm/secure)
	    // %3$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create file-based loop %1$s of %2$s (%3$s)"),
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
	txt = sformat( _("Formatting file-based loop %1$s of %2$s (%3$s) with %4$s "),
		       d.c_str(), lfile.c_str(), sizeString().c_str(), 
		       fsTypeString().c_str() );
	}
    else
	{
	d.erase( 0, 5 );
	if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. loop0
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format file-based loop %1$s of %2$s (%3$s) as %5$s with %4$s"),
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
		txt = sformat( _("Format encrypted file-based loop %1$s of %2$s (%3$s) as %5$s with %4$s"),
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
	    txt = sformat( _("Format file-based loop %1$s of %2$s (%3$s) with %4$s"),
			   d.c_str(), lfile.c_str(), sizeString().c_str(), 
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

void Loop::getInfo( LoopInfo& info ) const
    {
    info.nr = num;
    info.file = lfile;
    }

