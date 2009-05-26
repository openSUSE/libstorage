/*
  Textdomain    "storage"
*/

#include <sstream>

#include <sys/stat.h>

#include "storage/Loop.h"
#include "storage/StorageTypes.h"
#include "storage/Container.h"
#include "storage/AppUtil.h"
#include "storage/ProcPart.h"
#include "storage/Storage.h"
#include "storage/SystemCmd.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


Loop::Loop(const LoopCo& d, const string& LoopDev, const string& LoopFile,
	   bool dmcrypt, const string& dm_dev, ProcPart& ppart,
	   SystemCmd& losetup)
    : Volume(d, 0, 0)
{
    y2mil("constructed loop dev:" << LoopDev << " file:" << LoopFile <<
	  " dmcrypt:" << dmcrypt << " dmdev:" << dm_dev);
    if( d.type() != LOOP )
	y2err("constructed loop with wrong container");
    init();
    lfile = LoopFile;
    loop_dev = fstab_loop_dev = LoopDev;
    if( loop_dev.empty() )
	getLoopData( losetup );
    if( loop_dev.empty() )
	{
	list<unsigned> l;
	d.loopIds( l );
	getFreeLoop( losetup, l );
	}
    string proc_dev;
    if( !dmcrypt )
	{
	dev = loop_dev;
	if( loopStringNum( loop_dev, num ))
	    {
	    setNameDev();
	    getMajorMinor( dev, mjr, mnr );
	    }
	proc_dev = loop_dev;
	}
    else
	{
	numeric = false;
	setEncryption( ENC_LUKS );
	if( !dm_dev.empty() )
	    {
	    setDmcryptDev( dm_dev );
	    proc_dev = alt_names.front();
	    }
	}
    is_loop = true;
    unsigned long long s = 0;
    if( !proc_dev.empty() )
	ppart.getSize( proc_dev, s );
    if( s>0 )
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


Loop::Loop(const LoopCo& d, const string& file, bool reuseExisting,
	   unsigned long long sizeK, bool dmcr)
    : Volume(d, 0, 0)
{
    y2mil("constructed loop file:" << file << " reuseExisting:" << reuseExisting <<
	  " sizek:" << sizeK << " dmcrypt:" << dmcr);
    if( d.type() != LOOP )
	y2err("constructed loop with wrong container");
    init();
    reuseFile = reuseExisting;
    lfile = file;
    getFreeLoop();
    is_loop = true;
    if( !dmcr )
	{
	dev = loop_dev;
	if( loopStringNum( dev, num ))
	    {
	    setNameDev();
	    getMajorMinor( dev, mjr, mnr );
	    }
	}
    else
	{
	numeric = false;
	setEncryption( ENC_LUKS );
	if( dmcrypt_dev.empty() )
	    dmcrypt_dev = getDmcryptName();
	setDmcryptDev( dmcrypt_dev, false );
	}
    checkReuse();
    if( !reuseFile )
	setSize( sizeK );
}


Loop::~Loop()
{
    y2deb("destructed loop " << dev);
}


void
Loop::init()
    {
    reuseFile = delFile = false;
    }

void
Loop::setDmcryptDev( const string& dm_dev, bool active )
    {
    dev = dm_dev;
    nm = dm_dev.substr( dm_dev.find_last_of( '/' )+1);
    if( active )
	{
	getMajorMinor( dev, mjr, mnr );
	replaceAltName( "/dev/dm-", "/dev/dm-"+decString(mnr) );
	}
    Volume::setDmcryptDev( dm_dev, active );
    }

void Loop::setLoopFile( const string& file )
    {
    lfile = file;
    checkReuse();
    }

void Loop::setReuse( bool reuseExisting )
    {
    reuseFile = reuseExisting;
    checkReuse();
    }

void Loop::checkReuse()
    {
    if( reuseFile )
	{
	struct stat st;
	if( stat( lfileRealPath().c_str(), &st )>=0 )
	    setSize( st.st_size/1024 );
	else
	    reuseFile = false;
	y2mil( "reuseFile:" << reuseFile << " size:" << sizeK() );
	}
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
	string pa = lfileRealPath();
	string::size_type pos;
	if( (pos=pa.rfind( '/' ))!=string::npos )
	    {
	    pa.erase( pos );
	    y2mil( "pa:" << pa );
	    createPath( pa );
	    }
	string cmd = DDBIN " if=/dev/zero of=" + quote(lfileRealPath());
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

unsigned Loop::major()
    {
    if( loop_major==0 )
	getLoopMajor();
    return( loop_major );
    }

void Loop::getLoopMajor()
    {
    loop_major = getMajorDevices( "loop" );
    y2mil("loop_major:" << loop_major);
    }

string Loop::loopDeviceName( unsigned num )
    {
    return( "/dev/loop" + decString(num));
    }

int Loop::setEncryption( bool val, storage::EncryptType typ )
    {
    y2mil( "val:" << val << " type:" << typ );
    int ret = Volume::setEncryption( val, typ );
    if( ret==0 && encryption!=orig_encryption )
	{
	numeric = !dmcrypt();
	if( dmcrypt() )
	    {
	    if( dmcrypt_dev.empty() )
		dmcrypt_dev = getDmcryptName();
	    setDmcryptDev( dmcrypt_dev, false );
	    }
	else
	    {
	    dev = loop_dev;
	    }
	}
    y2mil( "ret:" << ret );
    return(ret);
    }

string Loop::removeText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/loop0
	txt = sformat( _("Deleting file-based device %1$s"), d.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete file-based device %1$s (%2$s)"), d.c_str(),
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
	txt = sformat( _("Creating file-based device %1$s of file %2$s"), d.c_str(), lfile.c_str() );
	}
    else
	{
	d.erase( 0, d.find_last_of('/')+1 );
	if( d.find( "loop" )==0 )
	    d.erase( 0, 4 );
	if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create file-based device %1$s of file %2$s (%3$s) as %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(),
			       fsTypeString().c_str(), mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create encrypted file-based device %1$s of file %2$s (%3$s) as %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(),
			       fsTypeString().c_str(), mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name
	    // %2$s is replaced by filename (e.g. /var/adm/secure)
	    // %3$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create file-based device %1$s of file %2$s (%3$s)"),
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
	txt = sformat( _("Formatting file-based device %1$s of %2$s (%3$s) with %4$s "),
		       d.c_str(), lfile.c_str(), sizeString().c_str(),
		       fsTypeString().c_str() );
	}
    else
	{
	d.erase( 0, d.find_last_of('/')+1 );
	if( d.find( "loop" )==0 )
	    d.erase( 0, 4 );
	if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format file-based device %1$s of %2$s (%3$s) as %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(),
			       fsTypeString().c_str(), mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted file-based device %1$s of %2$s (%3$s) as %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(),
			       fsTypeString().c_str(), mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name
	    // %2$s is replaced by filename (e.g. /var/adm/secure)
	    // %3$s is replaced by size (e.g. 623.5 MB)
	    // %4$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format file-based device %1$s of %2$s (%3$s) with %4$s"),
			   d.c_str(), lfile.c_str(), sizeString().c_str(),
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

void Loop::getInfo( LoopInfo& tinfo ) const
    {
    ((Volume*)this)->getInfo( info.v );
    info.nr = num;
    info.file = lfile;
    info.reuseFile = reuseFile;
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Loop& l )
    {
    s << "Loop " << *(Volume*)&l
      << " LoopFile:" << l.lfile;
    if( l.reuseFile )
      s << " reuse";
    if( l.delFile )
      s << " delFile";
    return( s );
    }


bool Loop::equalContent( const Loop& rhs ) const
    {
    return( Volume::equalContent(rhs) &&
	    lfile==rhs.lfile && reuseFile==rhs.reuseFile &&
	    delFile==rhs.delFile );
    }

void Loop::logDifference( const Loop& rhs ) const
    {
    string log = Volume::logDifference( rhs );
    if( lfile!=rhs.lfile )
	log += " LoopFile:" + lfile + "-->" + rhs.lfile;
    if( reuseFile!=rhs.reuseFile )
	{
	if( rhs.reuseFile )
	    log += " -->reuse";
	else
	    log += " reuse-->";
	}
    if( delFile!=rhs.delFile )
	{
	if( rhs.delFile )
	    log += " -->delFile";
	else
	    log += " delFile-->";
	}
    y2mil(log);
    }


Loop& Loop::operator=(const Loop& rhs)
{
    y2deb("operator= from " << rhs.nm);
    *((Volume*)this) = rhs;
    lfile = rhs.lfile;
    reuseFile = rhs.reuseFile;
    delFile = rhs.delFile;
    return *this;
}


Loop::Loop(const LoopCo& d, const Loop& rhs)
    : Volume(d)
{
    y2deb("constructed loop by copy constructor from " << rhs.nm);
    *this = rhs;
}


unsigned Loop::loop_major = 0;

}
