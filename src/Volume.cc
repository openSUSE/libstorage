/*
  Textdomain    "storage"
*/

#include <sstream>

#include <features.h>
#include <sys/stat.h>

#include "y2storage/Volume.h"
#include "y2storage/Disk.h"
#include "y2storage/LoopCo.h"
#include "y2storage/Storage.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/Container.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/ProcMounts.h"
#include "y2storage/OutputProcessor.h"
#include "y2storage/EtcFstab.h"

using namespace std;
using namespace storage;

Volume::Volume( const Container& d, unsigned PNr, unsigned long long SizeK ) 
    : cont(&d)
    {
    numeric = true;
    num = PNr;
    size_k = orig_size_k = SizeK;
    init();
    y2milestone( "constructed volume %s on disk %s", (num>0)?dev.c_str():"",
                 cont->name().c_str() );
    }

Volume::Volume( const Container& c, const string& Name, unsigned long long SizeK ) : cont(&c)
    {
    numeric = false;
    nm = Name;
    size_k = orig_size_k = SizeK;
    init();
    y2milestone( "constructed volume \"%s\" on disk %s", dev.c_str(),
                 cont->name().c_str() );
    }

Volume::Volume( const Container& c ) : cont(&c)
    {
    numeric = false;
    size_k = orig_size_k = 0;
    init();
    y2milestone( "constructed late init volume" );
    }

Volume::~Volume()
    {
    y2milestone( "destructed volume %s", dev.c_str() );
    }

void Volume::setNameDev()
    {
    std::ostringstream Buf_Ci;
    if( numeric )
	Buf_Ci << cont->device() << (Disk::needP(cont->device())?"p":"") << num;
    else
	Buf_Ci << cont->device() << "/" << nm;
    dev = Buf_Ci.str();
    if( nm.empty() )
	nm = dev.substr( 5 );
    }

void Volume::init()
    {
    del = create = format = is_loop = loop_active = silent = false;
    is_mounted = ronly = fstab_added = ignore_fstab = false;
    detected_fs = fs = FSUNKNOWN;
    mount_by = orig_mount_by = MOUNTBY_DEVICE;
    encryption = orig_encryption = ENC_NONE;
    mjr = mnr = 0;
    if( numeric||!nm.empty() )
	{
	setNameDev();
	getMajorMinor( dev, mjr, mnr );
	}
    if( !numeric )
	num = 0;
    }

CType Volume::cType() const
    {
    return( cont->type() );
    }

bool Volume::operator== ( const Volume& rhs ) const
    {
    return( (*cont)==(*rhs.cont) && 
            nm == rhs.nm && 
	    del == rhs.del ); 
    }

bool Volume::operator< ( const Volume& rhs ) const
    {
    if( *cont != *rhs.cont )
	return( *cont<*rhs.cont );
    else if( (numeric && num!=rhs.num) || (!numeric && nm != rhs.nm) )
	{
	if( numeric )
	    return( num<rhs.num );
	else
	    return( nm<rhs.nm );
	}
    else
	return( !del );
    }

bool Volume::getMajorMinor( const string& device,
			    unsigned long& Major, unsigned long& Minor )
    {
    bool ret = false;
    string dev = normalizeDevice( device );
    struct stat sbuf;
    if( stat( dev.c_str(), &sbuf )==0 )
	{
	Minor = gnu_dev_minor( sbuf.st_rdev );
	Major = gnu_dev_major( sbuf.st_rdev );
	ret = true;
	}
    return( ret );
    }

void Volume::getFstabData( EtcFstab& fstabData )
    {
    FstabEntry entry;
    bool found = false;
    if( cont->type()==LOOP )
	{
	Loop* l = static_cast<Loop*>(this);
	found = fstabData.findDevice( l->loopFile(), entry );
	}
    else
	{
	found = fstabData.findDevice( device(), entry );
	if( !found )
	    {
	    found = fstabData.findDevice( alt_names, entry );
	    }
	if( !found && !(uuid.empty()&&label.empty()) )
	    {
	    found = fstabData.findUuidLabel( uuid, label, entry );
	    fstabData.setDevice( entry, device() );
	    }
	}
    if( !found && !mp.empty() )
	{
	found = fstabData.findMount( mp, entry );
	}
    if( found )
	{
	std::ostringstream b;
	b << "line[" << device() << "]=";
	b << "noauto:" << entry.noauto;
	if( mp.empty() )
	    {
	    mp = orig_mp = entry.mount;
	    b << " mount:" << mp;
	    }
	mount_by = orig_mount_by = entry.mount_by;
	if( mount_by != MOUNTBY_DEVICE )
	    {
	    b << " mountby:" << mb_names[mount_by];
	    }
	fstab_opt = orig_fstab_opt = mergeString( entry.opts, "," );
	b << " fstopt:" << fstab_opt;
	if( !is_loop && entry.loop )
	    {
	    is_loop = true;
	    orig_encryption = encryption = entry.encr;
	    loop_dev = fstab_loop_dev = entry.loop_dev;
	    b << " loop_dev:" << loop_dev << " encr:" << enc_names[encryption];
	    }
	y2milestone( "%s", b.str().c_str() );
	}
    }

void Volume::getMountData( const ProcMounts& mountData )
    {
    mp = mountData.getMount( mountDevice() );
    if( mp.empty() )
	{
	mp = mountData.getMount( alt_names );
	}
    if( !mp.empty() )
	{
	is_mounted = true;
	y2milestone( "%s mounted on %s", device().c_str(), mp.c_str() );
	}
    orig_mp = mp;
    }

void Volume::getLoopData( SystemCmd& loopData )
    {
    bool found = false;
    if( cont->type()==LOOP )
	{
	Loop* l = static_cast<Loop*>(this);
	found = loopData.select( " (" + l->loopFile() + ") " )>0;
	}
    else
	{
	found = loopData.select( " (" + device() + ")" )>0;
	if( !found )
	    {
	    list<string>::const_iterator an = alt_names.begin();
	    while( !found && an!=alt_names.end() )
		{
		found = loopData.select( " (" + *an + ") " )>0;
		++an;
		}
	    }
	}
    if( found )
	{
	list<string> l = splitString( *loopData.getLine( 0, true ));
	std::ostringstream b;
	b << "line[" << device() << "]=" << l;
	y2milestone( "%s", b.str().c_str() );
	if( !l.empty() )
	    {
	    list<string>::const_iterator el = l.begin();
	    is_loop = loop_active = true;
	    loop_dev = *el;
	    if( !loop_dev.empty() && *loop_dev.rbegin()==':' )
	        loop_dev.erase(--loop_dev.end());
	    fstab_loop_dev = loop_dev;
	    b.str("");
	    b << "loop_dev:" << loop_dev;
	    orig_encryption = encryption = ENC_NONE;
	    if( l.size()>3 )
		{
		++el; ++el; ++el;
		string encr = "encryption=";
		if( el->find( encr )==0 )
		    {
		    encr = el->substr( encr.size() );
		    if( encr == "twofish160" )
			orig_encryption = encryption = ENC_TWOFISH_OLD;
		    else if( encr == "twofish256" )
			orig_encryption = encryption = ENC_TWOFISH256_OLD;
		    else if( encr == "CryptoAPI/twofish-cbc" )
			orig_encryption = encryption = ENC_TWOFISH;
		    else
			orig_encryption = encryption = ENC_UNKNOWN;
		    }
		}
	    b << " encr:" << encryption;
	    y2milestone( "%s", b.str().c_str() );
	    }
	}
    }

void Volume::getFsData( SystemCmd& blkidData )
    {
    bool found = blkidData.select( "^" + mountDevice() + ":" )>0;
    if( !found && !is_loop )
	{
	list<string>::const_iterator an = alt_names.begin();
	while( !found && an!=alt_names.end() )
	    {
	    found = blkidData.select( "^" + *an + ":" )>0;
	    ++an;
	    }
	}
    if( found )
	{
	list<string> l = splitString( *blkidData.getLine( 0, true ), " \t\n",
	                              true, true, "\"" );
	std::ostringstream b;
	b << "line[" << device() << "]=" << l;
	y2milestone( "%s", b.str().c_str() );
	if( !l.empty() )
	    {
	    l.pop_front();
	    map<string,string> m = makeMap( l, "=", "\"" );
	    map<string,string>::const_iterator i = m.find( "TYPE" );
	    b.str("");
	    if( i !=m.end() )
		{
		if( i->second == "reiserfs" )
		    {
		    fs = REISERFS;
		    }
		else if( i->second == "swap" )
		    {
		    fs = SWAP;
		    }
		else if( i->second == "ext2" )
		    {
		    fs = (m["SEC_TYPE"]=="ext3")?EXT3:EXT2;
		    }
		else if( i->second == "ext3" )
		    {
		    fs = EXT3;
		    }
		else if( i->second == "vfat" )
		    {
		    fs = VFAT;
		    }
		else if( i->second == "ntfs" )
		    {
		    fs = NTFS;
		    }
		else if( i->second == "jfs" )
		    {
		    fs = JFS;
		    }
		else if( i->second == "hfs" )
		    {
		    fs = HFS;
		    }
		else if( i->second == "xfs" )
		    {
		    fs = XFS;
		    }
		else if( i->second == "(null)" )
		    {
		    fs = FSNONE;
		    }
		detected_fs = fs;
		b << "fs:" << fs_names[fs];
		}
	    i = m.find( "UUID" );
	    if( i != m.end() )
		{
		uuid = i->second;
		b << " uuid:" << uuid;
		}
	    i = m.find( "LABEL" );
	    if( i != m.end() )
		{
		label = orig_label = i->second;
		b << " label:\"" << label << "\"";
		}
	    y2milestone( "%s", b.str().c_str() );
	    }
	}
    }

int Volume::setFormat( bool val, storage::FsType new_fs )
    {
    int ret = 0;
    y2milestone( "device:%s val:%d fs:%s", dev.c_str(), val, 
                 fs_names[new_fs].c_str() );
    format = val;
    if( !format )
	{
	fs = detected_fs;
	mkfs_opt = "";
	}
    else
	{
	FsCapabilities caps;
	if( uby.t != UB_NONE )
	    {
	    ret = VOLUME_ALREADY_IN_USE;
	    }
	else if( cont->getStorage()->getFsCapabilities( fs, caps ) && 
		 caps.minimalFsSizeK > size_k  )
	    {
	    ret = VOLUME_FORMAT_FS_TOO_SMALL;
	    }
	else
	    {
	    fs = new_fs;
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::changeMount( const string& m )
    {
    int ret = 0;
    y2milestone( "device:%s mount:%s", dev.c_str(), m.c_str() );
    if( (!m.empty() && m[0]!='/' && m!="swap") ||
	m.find_first_of( " \t\n" ) != string::npos )
	{
	ret = VOLUME_MOUNT_POINT_INAVLID;
	}
    else if( uby.t != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else
	{
	mp = m;
	if( m.empty() )
	    {
	    orig_mount_by = mount_by = MOUNTBY_DEVICE;
	    orig_fstab_opt = fstab_opt = "";
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::changeMountBy( MountByType mby )
    {
    int ret = 0;
    y2milestone( "device:%s mby:%s", dev.c_str(), mbyTypeString(mby).c_str() );
    if( !mp.empty() )
	{
	if( mby != MOUNTBY_DEVICE )
	    {
	    FsCapabilities caps;
	    if( encryption != ENC_NONE )
		ret = VOLUME_MOUNTBY_NOT_ENCRYPTED;
	    else if( !cont->getStorage()->getFsCapabilities( fs, caps ) ||
	             (mby==MOUNTBY_LABEL && !caps.supportsLabel) ||
	             (mby==MOUNTBY_UUID && !caps.supportsUuid))
		{
		ret = VOLUME_MOUNTBY_UNSUPPORTED_BY_FS;
		}
	    }
	if( ret==0 )
	    mount_by = mby;
	}
    else if( uby.t != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else
	ret = VOLUME_FSTAB_EMPTY_MOUNT;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::changeFstabOptions( const string& options )
    {
    int ret = 0;
    y2milestone( "device:%s options:%s encr:%s", dev.c_str(), options.c_str(),
                 encTypeString(encryption).c_str() );
    if( !mp.empty() )
	{
	fstab_opt = options;
	if( encryption != ENC_NONE )
	    {
	    list<string> l = splitString( fstab_opt, "," );
	    list<string>::iterator loop = find( l.begin(), l.end(), "loop" );
	    if( loop == l.end() )
		loop = find_if( l.begin(), l.end(), find_begin( "loop=" ) );
	    list<string>::iterator enc = find_if( l.begin(), l.end(), find_begin( "encryption=" ) );
	    if( optNoauto() )
		{
		string lstr = "loop";
		if( !fstab_loop_dev.empty() )
		    lstr += "="+fstab_loop_dev;
		string estr = "encryption=" + Volume::encTypeString(encryption);
		if( enc==l.end() )
		    l.push_front( estr );
		else
		    *enc = estr;
		if( loop==l.end() )
		    l.push_front( lstr );
		else
		    *loop = lstr;
		}
	    else
		{
		if( loop!=l.end() )
		    l.erase( loop );
		if( enc!=l.end() )
		    l.erase( enc );
		}
	    fstab_opt = mergeString( l, "," );
	    y2milestone( "update encrypted (opt_noauto:%d) fstab_opt:%s", 
	                 optNoauto(), fstab_opt.c_str() );
	    }
	}
    else if( uby.t != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else
	ret = VOLUME_FSTAB_EMPTY_MOUNT;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string Volume::formatText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting device %1$s (%2$s) with %3$s "),
		       d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format device %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted device %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format device %1$s (%2$s) with %3$s"),
			   d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

int Volume::doFormat()
    {
    static int fcount=1000;
    int ret = 0;
    bool needMount = false;
    y2milestone( "device:%s", dev.c_str() );
    if( !silent )
	{
	cont->getStorage()->showInfoCb( formatText(true) );
	}
    if( uby.t != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else if( isMounted() )
	{
	ret = umount( orig_mp );
	needMount = ret==0;
	}
    if( ret==0 && !cont->getStorage()->test() )
	{
	ret = checkDevice();
	}
    if( ret==0 &&
        (Storage::arch().find( "sparc" )!=0 || encryption!=ENC_NONE ))
	{
	SystemCmd c;
	string cmd = "/bin/dd if=";
	cmd += (encryption!=ENC_NONE) ? "/dev/urandom" : "/dev/zero";
	cmd += " of=" + dev + " bs=1024 count=";
	cmd += decString(min(50ull,size_k));
	if( c.execute( cmd ) != 0 )
	    ret = VOLUME_FORMAT_DD_FAILED;
	}
    if( ret==0 && mountDevice()!=dev && !cont->getStorage()->test() )
	{
	ret = checkDevice(mountDevice());
	}
    if( ret==0 && mountDevice().find( "/dev/md" )!=0 )
	{
	SystemCmd c;
	c.execute( "mdadm --zero-superblock " + mountDevice() );
	}
    if( ret==0 )
	{
	string cmd;
	string params;
	ScrollBarHandler* p = NULL;
	CallbackProgressBar cb = cont->getStorage()->getCallbackProgressBarTheOne();
	
	switch( fs )
	    {
	    case EXT2:
	    case EXT3:
		cmd = "/sbin/mke2fs";
		params = (fs==EXT2) ? "-v" : "-j -v";
		p = new Mke2fsScrollbar( cb );
		break;
	    case REISERFS:
		cmd = "/sbin/mkreiserfs";
		params = "-f -f";
		p = new ReiserScrollbar( cb );
		break;
	    case VFAT:
		{
		cmd = "/sbin/mkdosfs";
		list<string> l=splitString( mkfs_opt );
		if( find_if( l.begin(), l.end(), find_begin( "-F" ) ) != l.end())
		    params = "-F 32";
		break;
		}
	    case JFS:
		cmd = "/sbin/mkfs.jfs";
		params = "-q";
		break;
	    case HFS:
		cmd = "/usr/bin/hformat";
		break;
	    case XFS:
		cmd = "/sbin/mkfs.xfs";
		params = "-q -f";
		break;
	    case SWAP:
		cmd = "/sbin/mkswap";
		break;
	    default:
		ret = VOLUME_FORMAT_UNKNOWN_FS;
		break;
	    }
	if( ret==0 )
	    {
	    cmd += " ";
	    if( !mkfs_opt.empty() )
		{
		cmd += mkfs_opt + " ";
		}
	    if( !params.empty() )
		{
		cmd += params + " ";
		}
	    cmd += mountDevice();
	    SystemCmd c;
	    c.setOutputProcessor( p );
	    c.execute( cmd );
	    if( c.retcode()!=0 )
		{
		ret = VOLUME_FORMAT_FAILED;
		setExtError( c );
		}
	    }
	delete p;
	}
    if( ret==0 && fs==EXT3 )
	{
	string cmd = "/sbin/tune2fs -c 500 -i 2m " + mountDevice();
	SystemCmd c( cmd );
	if( c.retcode()!=0 )
	    ret = VOLUME_TUNE2FS_FAILED;
	}
    if( ret==0 && !orig_mp.empty() )
	{
	ret = doFstabUpdate();
	}
    if( ret==0 )
	{
	format = false;
	detected_fs = fs;
	if( fs != SWAP && !cont->getStorage()->test() )
	    {
	    SystemCmd Blkid( "BLKID_SKIP_CHECK_MDRAID=1 /sbin/blkid -c /dev/null " + mountDevice() );
	    FsType old=fs;
	    getFsData( Blkid );
	    if( fs != old )
		ret = VOLUME_FORMAT_FS_UNDETECTED;
	    }
	else if( fs != SWAP )
	    {
	    uuid = "testmode-0123-4567-6666-98765432"+decString(fcount++);
	    }
	}
    if( ret==0 && !label.empty() )
	{
	ret = doSetLabel();
	}
    if( needMount )
	{
	int r = mount( orig_mp );
	ret = (ret==0)?r:ret;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::umount( const string& mp )
    {
    SystemCmd cmd;
    y2milestone( "device:%s mp:%s", dev.c_str(), mp.c_str() );
    string cmdline = ((detected_fs != SWAP)?"umount ":"swapoff ") + mountDevice();
    int ret = cmd.execute( cmdline );
    if( ret != 0 && mountDevice()!=dev )
	{
	cmdline = ((detected_fs != SWAP)?"umount ":"swapoff ") + dev;
	ret = cmd.execute( cmdline );
	}
    if( ret!=0 && !mp.empty() && mp!="swap" )
	{
	cmdline = "umount " + mp;
	ret = cmd.execute( cmdline );
	}
    if( ret!=0 && !orig_mp.empty() && orig_mp!="swap" )
	{
	cmdline = "umount " + orig_mp;
	ret = cmd.execute( cmdline );
	}
    if( ret != 0 )
	ret = VOLUME_UMOUNT_FAILED;
    else 
	is_mounted = false;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::loUnsetup()
    {
    int ret=0;
    if( is_loop && loop_active )
	{
	SystemCmd c( "losetup -d " + loop_dev );
	if( c.retcode()!=0 )
	    ret = VOLUME_LOUNSETUP_FAILED;
	else
	    loop_active = false;
	}
    return( ret );
    }

string Volume::mountText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
        {
	if( !mp.empty() )
	    {
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by mount point e.g. /home
	    txt = sformat( _("Mounting %1$s to %2$s"), d.c_str(), mp.c_str() );
	    }
	else
	    {
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Unmounting %1$s"), d.c_str() );
	    }
        }
    else
        {
	if( !orig_mp.empty() && !mp.empty() )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by mount point e.g. /home
	    txt = sformat( _("Change mount point of %1$s to %2$s"), d.c_str(),
	                   mp.c_str() );
	    }
	else if( !mp.empty() )
	    {
	    if( mp != "swap" )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by mount point e.g. /home
		txt = sformat( _("Set mount point of %1$s to %2$s"), d.c_str(), 
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by "swap"
		txt = sformat( _("Use %1$s as %2$s"), d.c_str(), mp.c_str() );
		}
	    }
	else if( !orig_mp.empty() )
	    {
	    string fn = "/etc/fstab";
	    if( encryption!=ENC_NONE && !optNoauto() )
		fn = "/etc/cryptotab";
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by pathname e.g. /etc/fstab
	    txt = sformat( _("Remove %1$s from %2$s"), d.c_str(), fn.c_str() );
	    }
        }
    return( txt );
    }

int Volume::checkDevice()
    {
    return( checkDevice(dev));
    }

int Volume::checkDevice( const string& device )
    {
    struct stat sbuf;
    int ret = 0;
    if( stat(device.c_str(), &sbuf)<0 )
	ret = VOLUME_DEVICE_NOT_PRESENT;
    else if( !S_ISBLK(sbuf.st_mode) )
	ret = VOLUME_DEVICE_NOT_BLOCK;
    y2milestone( "checkDevice:%s ret:%d", device.c_str(), ret );
    return( ret );
    }

int Volume::doMount()
    {
    int ret = 0;
    string lmount;
    if( mp != "swap" )
	lmount += cont->getStorage()->root();
    if( mp!="/" )
	lmount += mp;
    y2milestone( "device:%s mp:%s old mp:%s", dev.c_str(), mp.c_str(),
                 orig_mp.c_str() );
    if( !silent )
	{
	cont->getStorage()->showInfoCb( mountText(true) );
	}
    if( uby.t != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    if( ret==0 && !orig_mp.empty() && isMounted() )
	{
	string um = orig_mp;
	if( um != "swap" )
	    um = cont->getStorage()->root() + um;
	ret = umount( um );
	}
    if( ret==0 && lmount!="swap" && access( lmount.c_str(), R_OK )!=0 )
	{
	createPath( lmount );
	}
    if( ret==0 && !mp.empty() && !cont->getStorage()->test() )
	{
	ret = checkDevice(mountDevice());
	if( ret==0 )
	    ret = mount( lmount );
	}
    if( ret==0 )
	{
	ret = doFstabUpdate();
	orig_mp = mp;
	}
    if( ret==0 && mp=="/" && !cont->getStorage()->root().empty() )
	{
	cont->getStorage()->rootMounted();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::canResize( unsigned long long newSizeK ) const
    {
    int ret=0;
    y2milestone( "val:%llu", newSizeK );
    if( uby.t != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else 
	{
	FsCapabilities caps;
	if( !format && fs!=FSNONE &&
	    (!cont->getStorage()->getFsCapabilities( fs, caps ) || 
	     (newSizeK < size_k && !caps.isReduceable) ||
	     (newSizeK > size_k && !caps.isExtendable)) )
	    {
	    ret = VOLUME_RESIZE_UNSUPPORTED_BY_FS;
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::resizeFs()
    {
    int ret = 0;
    if( !format )
	{
	string cmd;
	SystemCmd c;
	switch( fs )
	    {
	    case SWAP:
		cmd = "mkswap " + mountDevice();
		c.execute( cmd );
		if( c.retcode()!=0 )
		    ret = VOLUME_RESIZE_FAILED;
		break;
	    case REISERFS:
		cmd = "resize_reiserfs -f ";
		if( needShrink() )
		    {
		    cmd = "echo y | " + cmd;
		    cmd += "-s " + decString(size_k) + "K ";
		    }
		cmd += mountDevice();
		c.execute( cmd );
		if( c.retcode()!=0 )
		    {
		    ret = VOLUME_RESIZE_FAILED;
		    setExtError( c );
		    }
		break;
	    case NTFS:
		cmd = "echo y | ntfsresize -f ";
		if( needShrink() )
		    cmd += "-s " + decString(size_k) + "k ";
		cmd += mountDevice();
		c.execute( cmd );
		if( c.retcode()!=0 )
		    {
		    ret = VOLUME_RESIZE_FAILED;
		    setExtError( c );
		    }
		break;
	    case EXT2:
	    case EXT3:
		cmd = "resize2fs -f " + mountDevice();
		if( needShrink() )
		    cmd += " " + decString(size_k) + "K";
		c.execute( cmd );
		if( c.retcode()!=0 )
		    {
		    ret = VOLUME_RESIZE_FAILED;
		    setExtError( c );
		    }
		break;
	    case XFS:
		{
		bool needumount = false;
		bool needrmdir = false;
		string mpoint = orig_mp;
		if( !isMounted() )
		    {
		    mpoint = cont->getStorage()->tmpDir()+"/mp";
		    mkdir( mpoint.c_str(), 0700 );
		    ret = mount( mpoint );
		    needrmdir = true;
		    if( ret==0 )
			needumount = true;
		    }
		if( ret==0 )
		    {
		    cmd = "xfs_growfs " + mpoint;
		    c.execute( cmd );
		    if( c.retcode()!=0 )
			{
			ret = VOLUME_RESIZE_FAILED;
			setExtError( c );
			}
		    }
		if( needumount )
		    {
		    int r = umount( mpoint );
		    ret = (ret!=0)?ret:r;
		    }
		if( needrmdir )
		    {
		    rmdir( mpoint.c_str() );
		    rmdir( cont->getStorage()->tmpDir().c_str() );
		    }
		}
		break;
	    default:
		break;
	    }
	if( cmd.empty() )
	    {
	    ret = VOLUME_RESIZE_UNSUPPORTED_BY_FS;
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::setEncryption( bool val )
    {
    int ret = 0;
    y2milestone( "val:%d", val );
    if( getUsedByType() != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    if( ret==0 )
	{
	if( !val )
	    {
	    is_loop = false;
	    encryption = ENC_NONE;
	    crypt_pwd.erase();
	    }
	else
	    {
	    if( !loop_active && crypt_pwd.empty() )
		ret = VOLUME_CRYPT_NO_PWD;
	    if( ret == 0 && format )
		{
		encryption = ENC_TWOFISH;
		is_loop = true;
		}
	    if( ret == 0 && !format && !loop_active )
		{
		if( cont->getStorage()->instsys() )
		    cont->getStorage()->activateHld(true);
		if( detectLoopEncryption()==ENC_UNKNOWN )
		    ret = VOLUME_CRYPT_NOT_DETECTED;
		if( cont->getStorage()->instsys() )
		    cont->getStorage()->activateHld(false);
		}
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string Volume::losetupText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Setting up encrypted loop device on %1$s"), d.c_str() );
        }
    else
        {
	// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Set up encrypted loop device on %1$s"), d.c_str() );
        }
    return( txt );
    }

bool Volume::loopStringNum( const string& name, unsigned& num )
    {
    bool ret=false;
    string d = undevDevice(name);
    static Regex loop( "^loop[0-9]+$" );
    if( loop.match( d ))
	{
	d.substr( 4 )>>num;
	ret = true;
	}
    return( ret );
    }

bool hasLoopDevice( const Volume& v ) { return( !v.loopDevice().empty() ); }

bool Volume::loopInUse( Storage* sto, const string& loopdev )
    {
    bool ret = false;

    Storage::ConstVolPair p = sto->volPair( hasLoopDevice );
    Storage::ConstVolIterator i=p.begin();
    while( !ret && i!=p.end() )
	{
	ret = i->loop_dev==loopdev;
	++i;
	}
    return( ret );
    }

int Volume::getFreeLoop()
    {
    const int loop_instsys_offset = 2;
    int ret = 0;
    if( loop_dev.empty() )
	{
	list<unsigned> lnum;
	Storage::ConstVolPair p = cont->getStorage()->volPair( hasLoopDevice );
	for( Storage::ConstVolIterator i=p.begin(); i!=p.end(); ++i )
	    {
	    unsigned num;
	    if( loopStringNum( i->loopDevice(), num ))
		lnum.push_back( num );
	    }
	unsigned num = cont->getStorage()->instsys()?loop_instsys_offset:0;
	bool found;
	string ldev;
	SystemCmd c( "losetup -a" );
	do
	    {
	    ldev = "^/dev/loop" + decString(num) + " ";
	    found = c.select( ldev )>0;
	    if( found || find( lnum.begin(), lnum.end(), num )!=lnum.end())
		num++;
	    }
	while( found && num<32 );
	if( found )
	    ret = VOLUME_LOSETUP_NO_LOOP;
	else
	    {
	    loop_dev = "/dev/loop" + decString(num);
	    if( cont->getStorage()->instsys() )
		fstab_loop_dev = "/dev/loop" + decString(num-loop_instsys_offset);
	    else
		fstab_loop_dev = loop_dev;
	    }
	y2milestone( "loop_dev:%s fstab_loop_dev:%s", loop_dev.c_str(), 
	             fstab_loop_dev.c_str() );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string Volume::getLosetupCmd( storage::EncryptType e, const string& pwdfile ) const
    {
    string cmd = "/sbin/losetup";
    if( e!=ENC_NONE )
	cmd += " -e " + encTypeString(e);
    switch( e )
	{
	case ENC_TWOFISH:
	    cmd = "rmmod loop_fish2; modprobe twofish; modprobe cryptoloop; " +
	          cmd;
	    break;
	case ENC_TWOFISH_OLD:
	case ENC_TWOFISH256_OLD:
	    cmd = "rmmod twofish cryptoloop; modprobe loop_fish2; " + cmd;
	    break;
	default:
	    break;
	}
    cmd += " ";
    cmd += loop_dev;
    cmd += " ";
    if( cont->type()!=LOOP )
	cmd += dev;
    else
	{
	const Loop* l = static_cast<const Loop*>(this);
	cmd += l->lfileRealPath();
	}
    cmd += " -p0 < ";
    cmd += pwdfile;
    y2milestone( "cmd:%s", cmd.c_str() );
    return( cmd );
    }

int 
Volume::setCryptPwd( const string& val ) 
    { 
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    y2milestone( "password:%s", val.c_str() );
#endif
    int ret = 0;

    if( ((encryption==ENC_UNKNOWN||encryption==ENC_TWOFISH_OLD||
          encryption==ENC_NONE) && val.size()<5) || 
        ((encryption==ENC_TWOFISH||encryption==ENC_TWOFISH256_OLD) && 
	  val.size()<8) )
	ret = VOLUME_CRYPT_PWD_TOO_SHORT;
    else
	{
	crypt_pwd=val; 
	if( encryption==ENC_UNKNOWN )
	    detectLoopEncryption();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

EncryptType Volume::detectLoopEncryption()
    {
    EncryptType ret = ENC_UNKNOWN;

    if( getContainer()->getStorage()->test() )
	{
	ret = encryption = orig_encryption = ENC_TWOFISH;
	y2milestone( "ret:%s", encTypeString(ret).c_str() );
	return( ret );
	}

    unsigned pos=0;
    static EncryptType try_order[] = { ENC_TWOFISH_OLD, ENC_TWOFISH256_OLD, 
                                       ENC_TWOFISH };
    string fname = cont->getStorage()->tmpDir()+"/pwdf";
    string mpname = cont->getStorage()->tmpDir()+"/mp";
    SystemCmd c;
    y2milestone( "device:%s", dev.c_str() );

    ofstream pwdfile( fname.c_str() );
    pwdfile << crypt_pwd << endl;
    pwdfile.close();
    mkdir( mpname.c_str(), 0700 );
    getFreeLoop();
    detected_fs = fs = FSUNKNOWN;
    is_loop = true;
    do
	{
	c.execute( "losetup -d " + loop_dev );
	c.execute( getLosetupCmd( try_order[pos], fname ));
	if( c.retcode()==0 )
	    {
	    c.execute( "BLKID_SKIP_CHECK_MDRAID=1 /sbin/blkid -c /dev/null " + mountDevice() );
	    getFsData( c );
	    if( detected_fs!=FSUNKNOWN )
		{
		c.execute( "mount -oro -t " + fsTypeString(detected_fs) + " " +
		           loop_dev + " " + mpname );
		if( c.retcode()==0 )
		    {
		    c.execute( "umount " + mpname );
		    string cmd;
		    switch( detected_fs )
			{
			case EXT2:
			case EXT3:
			    cmd = "fsck.ext2 -n -f " + loop_dev + " > /dev/null";
			    break;
			case REISERFS:
			    cmd = "reiserfsck --yes --check -q " + loop_dev;
			    break;
			default:
			    cmd = "fsck -n -t " + fsTypeString(detected_fs) + 
		                  " " + loop_dev;
			    break;
			}
		    c.execute( cmd );
		    }
		if( c.retcode()!=0 )
		    {
		    detected_fs = fs = FSUNKNOWN;
		    label.erase();
		    orig_label.erase();
		    uuid.erase();
		    }
		c.execute( "umount " + mpname );
		}
	    }
	if( fs==FSUNKNOWN )
	    pos++;
	}
    while( detected_fs==FSUNKNOWN && pos<lengthof(try_order) );
    c.execute( "losetup -d " + loop_dev );
    if( detected_fs!=FSUNKNOWN )
	{
	is_loop = true;
	loop_active = false;
	ret = encryption = orig_encryption = try_order[pos];
	}
    unlink( fname.c_str() );
    rmdir( mpname.c_str() );
    rmdir( cont->getStorage()->tmpDir().c_str() );
    y2milestone( "ret:%s", encTypeString(ret).c_str() );
    return( ret );
    }

int Volume::doLosetup()
    {
    int ret = 0;
    y2milestone( "device:%s mp:%s is_loop:%d loop_active:%d", 
                 dev.c_str(), mp.c_str(), is_loop, loop_active );
    if( !silent && is_loop )
	{
	cont->getStorage()->showInfoCb( losetupText(true) );
	}
    if( is_mounted )
	{
	umount( orig_mp );
	}
    if( is_loop )
	{
	if( ret==0 && loop_dev.empty() )
	    {
	    ret = getFreeLoop();
	    }
	if( ret==0 )
	    {
	    string fname = cont->getStorage()->tmpDir()+"/pwdf";
	    ofstream pwdfile( fname.c_str() );
	    pwdfile << crypt_pwd << endl;
	    pwdfile.close();

	    SystemCmd c( getLosetupCmd( encryption, fname ));
	    if( c.retcode()!=0 )
		ret = VOLUME_LOSETUP_FAILED;
	    unlink( fname.c_str() );
	    rmdir( cont->getStorage()->tmpDir().c_str() );
	    }
	if( ret==0 )
	    {
	    loop_active = true;
	    list<string> l = splitString( fstab_opt, "," );
	    list<string>::iterator i = find( l.begin(), l.end(), "loop" );
	    if( i == l.end() )
		i = find_if( l.begin(), l.end(), find_begin( "loop=" ) );
	    if( i!=l.end() )
		*i = "loop=" + fstab_loop_dev;
	    fstab_opt = mergeString( l, "," );
	    }
	}
    else 
	{
	if( loop_dev.size()>0 )
	    {
	    SystemCmd c( "losetup -d " + loop_dev );
	    loop_dev.erase();
	    list<string> l = splitString( fstab_opt, "," );
	    list<string>::iterator i = find( l.begin(), l.end(), "loop" );
	    if( i == l.end() )
		i = find_if( l.begin(), l.end(), find_begin( "loop=" ) );
	    if( i!=l.end() )
		l.erase( i );
	    i = find_if( l.begin(), l.end(), find_begin( "encryption=" ) );
	    if( i!=l.end() )
		l.erase( i );
	    fstab_opt = mergeString( l, "," );
	    }
	loop_active = false;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string Volume::labelText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	// %2$s is replaced by a name e.g. ROOT
        txt = sformat( _("Setting label on %1$s to %2$s"), d.c_str(), label.c_str() );
        }
    else
        {
	// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	// %2$s is replaced by a name e.g. ROOT
	txt = sformat( _("Set label on %1$s to %2$s"), d.c_str(), label.c_str() );
        }
    return( txt );
    }

int Volume::doSetLabel()
    {
    int ret = 0;
    bool remount = false;
    FsCapabilities caps;
    y2milestone( "device:%s mp:%s label:%s",  dev.c_str(), mp.c_str(), 
                 label.c_str() );
    if( !silent )
	{
	cont->getStorage()->showInfoCb( labelText(true) );
	}
    if( !cont->getStorage()->getFsCapabilities( fs, caps ) || 
        !caps.supportsLabel  )
	{
	ret = VOLUME_LABEL_TOO_LONG;
	}
    if( ret==0 && getUsedByType() != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    if( ret==0 && is_mounted && !caps.labelWhileMounted )
	{
	ret = umount( cont->getStorage()->root()+orig_mp );
	if( ret!=0 )
	    ret = VOLUME_LABEL_WHILE_MOUNTED;
	else 
	    remount = true;
	}
    if( ret==0 )
	{
	string cmd;
	switch( fs )
	    {
	    case EXT2:
	    case EXT3:
		cmd = "/sbin/tune2fs -L \"" + label + "\" " + mountDevice();
		break;
	    case REISERFS:
		cmd = "/sbin/reiserfstune -l \"" + label + "\" " + mountDevice();
		break;
	    case XFS:
		cmd = "/usr/sbin/xfs_admin -L " + label + " " + mountDevice();
		break;
	    default:
		ret = VOLUME_MKLABEL_FS_UNABLE;
		break;
	    }
	if( !cmd.empty() )
	    {
	    SystemCmd c( cmd );
	    if( c.retcode()!= 0 )
		ret = VOLUME_MKLABEL_FAILED;
	    }
	}
    if( remount )
	{
	ret = mount( cont->getStorage()->root()+orig_mp );
	}
    if( ret==0 )
	{
	ret = doFstabUpdate();
	}
    if( ret==0 )
	{
	orig_label = label;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::setLabel( const string& val )
    {
    int ret=0;
    y2milestone( "label:%s", val.c_str() );
    FsCapabilities caps;
    if( cont->getStorage()->getFsCapabilities( fs, caps ) && 
        caps.supportsLabel  )
	{
	if( caps.labelLength < val.size() )
	    ret = VOLUME_LABEL_TOO_LONG;
	else if( getUsedByType() != UB_NONE )
	    ret = VOLUME_ALREADY_IN_USE;
	else
	    label = val;
	}
    else
	ret = VOLUME_LABEL_NOT_SUPPORTED;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::mount( const string& m )
    {
    SystemCmd cmd;
    y2milestone( "device:%s mp:%s", dev.c_str(), m.c_str() );
    string cmdline;
    if( fs != SWAP )
	{
	string lmount = (!m.empty())?m:mp;
	y2milestone( "device:%s mp:%s", dev.c_str(), lmount.c_str() );
	cmdline = "modprobe " + fs_names[fs];
	cmd.execute( cmdline );
	cmdline = "mount ";
	string fsn = fs_names[fs];
	if( fs == NTFS )
	    cmdline += "-r ";
	else if( fs == FSUNKNOWN )
	    fsn = "auto";
	cmdline += "-t " + fsn + " " + mountDevice() + " " + lmount;
	}
    else
	{
	cmdline = "swapon " + mountDevice();
	if( cont->getStorage()->instsys() )
	    {
	    ProcMounts mountData;
	    string m = mountData.getMount( mountDevice() );
	    if( m.empty() )
		{
		m = mountData.getMount( alt_names );
		}
	    if( m == "swap" )
		cmdline = "echo " + cmdline;
	    }
	}
    int ret = cmd.execute( cmdline );
    if( ret != 0 )
	{
	ret = VOLUME_MOUNT_FAILED;
	setExtError( cmd );
	}
    else
	is_mounted = true;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::prepareRemove()
    {
    int ret = 0;
    y2milestone( "device:%s", dev.c_str() );
    if( !orig_mp.empty() )
	{
	if( isMounted() )
	    {
	    ret = umount( orig_mp );
	    }
	if( ret==0 )
	    {
	    ret = doFstabUpdate();
	    }
	}
    if( is_loop && loop_active )
	{
	loUnsetup();
	}
    cont->getStorage()->eraseFreeInfo(dev);
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string Volume::getMountByString( MountByType mby, const string& dev, 
				 const string& uuid, const string& label ) const
    {
    string ret = dev;
    if( mby==MOUNTBY_UUID )
	{
	ret = "UUID=" + uuid;
	}
    else if( mby==MOUNTBY_LABEL )
	{
	ret = "LABEL=" + label;
	}
    return( ret );
    }

void Volume::getCommitActions( list<commitAction*>& l ) const
    {
    if( deleted() )
	{
	l.push_back( new commitAction( DECREASE, cont->type(),
				       removeText(false), true ));
	}
    else if( needShrink() )
	{
	l.push_back( new commitAction( DECREASE, cont->type(),
				       resizeText(false), true ));
	}
    else if( created() )
	{
	l.push_back( new commitAction( INCREASE, cont->type(),
				       createText(false), false ));
	}
    else if( needExtend() )
	{
	l.push_back( new commitAction( INCREASE, cont->type(),
				       resizeText(false), true ));
	}
    else if( format )
	{
	l.push_back( new commitAction( FORMAT, cont->type(),
				       formatText(false), true ));
	}
    else if( mp != orig_mp )
	{
	l.push_back( new commitAction( MOUNT, cont->type(),
				       mountText(false), false ));
	}
    else if( label != orig_label )
	{
	l.push_back( new commitAction( MOUNT, cont->type(),
				       labelText(false), false ));
	}
    else if( needFstabUpdate() )
	{
	l.push_back( new commitAction( MOUNT, cont->type(),
				       fstabUpdateText(), false ));
	}
    }

string Volume::fstabUpdateText() const
    {
    string txt;
    EtcFstab* fstab = cont->getStorage()->getFstab();
    if( !orig_mp.empty() && mp.empty() )
	txt = fstab->removeText( false, inCrypto(), orig_mp );
    else
	txt = fstab->updateText( false, inCrypto(), orig_mp );
    return( txt );
    }

int Volume::doFstabUpdate()
    {
    int ret = 0;
    bool changed = false;
    y2milestone( "device:%s mp:%s options:%s ignore_fstab:%d",  dev.c_str(), 
                 mp.c_str(), fstab_opt.c_str(), ignore_fstab );
    if( !ignore_fstab )
	{
	EtcFstab* fstab = cont->getStorage()->getFstab();
	FstabEntry entry;
	y2milestone( "del:%d", deleted() );
	if( !orig_mp.empty() && (deleted()||mp.empty()) &&
	    (fstab->findDevice( dev, entry ) ||
	     fstab->findDevice( alt_names, entry ) ||
	     (cType()==LOOP && fstab->findMount( orig_mp, entry )) ||
	     (cType()==LOOP && fstab->findMount( mp, entry )) ))
	    {
	    changed = true;
	    if( !silent )
		{
		cont->getStorage()->showInfoCb( 
		    fstab->removeText( true, entry.crypto, entry.mount ));
		}
	    y2milestone( "before removeEntry" );
	    ret = fstab->removeEntry( entry );
	    }
	else if( !mp.empty() && !deleted() )
	    {
	    if( fstab->findDevice( dev, entry ) ||
		fstab->findDevice( alt_names, entry ))
		{
		FstabChange che( entry );
		if( orig_mp!=mp )
		    {
		    changed = true;
		    che.mount = mp;
		    }
		if( fstab_opt!=orig_fstab_opt )
		    {
		    changed = true;
		    if( fstab_opt.empty() )
			{
			che.opts.clear();
			che.opts.push_back( is_loop?"noatime":"defaults" );
			}
		    else
			che.opts = splitString( fstab_opt, "," );
		    }
		if( mount_by!=orig_mount_by || 
		    (format && mount_by==MOUNTBY_UUID) ||
		    (orig_label!=label && mount_by==MOUNTBY_LABEL) )
		    {
		    changed = true;
		    che.dentry = getMountByString( mount_by, dev, uuid, label );
		    }
		if( fs != detected_fs )
		    {
		    changed = true;
		    che.fs = fs_names[fs];
		    if( fs==SWAP )
			che.freq = che.passno = 0;
		    else
			{
			che.freq = 1;
			che.passno = (mp=="/") ? 1 : 2;
			}
		    }
		if( encryption != orig_encryption )
		    {
		    changed = true;
		    che.encr = encryption;
		    che.loop_dev = fstab_loop_dev;
		    }
		if( changed )
		    {
		    if( !silent && !fstab_added )
			{
			cont->getStorage()->showInfoCb( 
			    fstab->updateText( true, inCrypto(), che.mount ));
			}
		    y2mil( "update fstab: " << che );
		    ret = fstab->updateEntry( che );
		    }
		}
	    else
		{
		changed = true;
		FstabChange che;
		che.device = dev;
		if( cont->type()!=LOOP )
		    che.dentry = getMountByString( mount_by, dev, uuid, label );
		else
		    {
		    const Loop* l = static_cast<const Loop*>(this);
		    che.dentry = l->loopFile();
		    }
		che.encr = encryption;
		che.loop_dev = fstab_loop_dev;
		che.fs = fs_names[fs];
		string fst = fstab_opt;
		if( fst.empty() )
		    {
		    if( is_loop )
			fst = "noatime";
		    else
			fst = "defaults";
		    }
		che.opts = splitString( fst, "," );
		che.mount = mp;
		if( fs != SWAP && fs != FSUNKNOWN && fs != NTFS && fs != VFAT &&
		    !is_loop && !optNoauto() )
		    {
		    che.freq = 1;
		    che.passno = (mp=="/") ? 1 : 2;
		    }
		if( !silent )
		    {
		    cont->getStorage()->showInfoCb( fstab->addText( true, 
		                                                    inCrypto(),
								    che.mount ));
		    }
		ret = fstab->addEntry( che );
		fstab_added = true;
		}
	    }
	if( changed && ret==0 && cont->getStorage()->isRootMounted() )
	    {
	    ret = fstab->flush();
	    }
	}
    y2milestone( "changed:%d ret:%d", changed, ret );
    return( ret );
    }

void Volume::fstabUpdateDone()
    {
    y2milestone( "begin" );
    orig_fstab_opt = fstab_opt;
    orig_mount_by = mount_by;
    orig_encryption = encryption;
    }

EncryptType Volume::toEncType( const string& val )
    {
    EncryptType ret = ENC_UNKNOWN;
    if( val=="none" || val.empty() )
        ret = ENC_NONE;
    else if( val=="twofish" )
        ret = ENC_TWOFISH_OLD;
    else if( val=="twofishSL92" )
        ret = ENC_TWOFISH256_OLD;
    else if( val=="twofish256" )
        ret = ENC_TWOFISH;
    return( ret );
    }

FsType Volume::toFsType( const string& val )
    {
    enum FsType ret = FSNONE;
    while( ret!=FSUNKNOWN && val!=fs_names[ret] )
	{
	ret = FsType(ret-1);
	}
    return( ret );
    }

MountByType Volume::toMountByType( const string& val )
    {
    enum MountByType ret = MOUNTBY_LABEL;
    while( ret!=MOUNTBY_DEVICE && val!=mb_names[ret] )
	{
	ret = MountByType(ret-1);
	}
    return( ret );
    }

string Volume::sizeString() const
    {
    return( kbyteToHumanString( size_k ));
    }

bool Volume::canUseDevice() const
    {
    bool ret = getUsedByType()==UB_NONE && getMount().empty();
    return( ret );
    }

string Volume::bootMount() const
    {
    if( Storage::arch()=="ia64" )
	return( "/boot/efi" );
    else
	return( "/boot" );
    }

bool Volume::needRemount() const
    {
    bool need = false;
    need = mp!=orig_mp;
    if( !need && !mp.empty() && !isMounted() && !optNoauto() )
	need = true;
    return( need );
    }

bool Volume::optNoauto() const
    {
    list<string> l = splitString( fstab_opt, "," );
    return( find( l.begin(), l.end(), "noauto" )!=l.end() );
    }

void Volume::setExtError( const SystemCmd& cmd, bool serr )
    {
    cont->setExtError( cmd, serr );
    }

string Volume::createText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Creating %1$s"), dev.c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Create %1$s"), dev.c_str() );
        }
    return( txt );
    }

string Volume::resizeText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrinking %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extending %1$s to %2$s"), d.c_str(), sizeString().c_str() );

        }
    else
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrink %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extend %1$s to %2$s"), d.c_str(), sizeString().c_str() );

        }
    return( txt );
    }

string Volume::removeText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Removing %1$s"), dev.c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Remove %1$s"), dev.c_str() );
        }
    return( txt );
    }

void Volume::getInfo( VolumeInfo& tinfo ) const
    {
    info.sizeK = size_k;
    info.major = mjr;
    info.minor = mnr;
    info.name = nm;
    info.device = dev;
    info.mount = mp;
    info.mount_by = mount_by;
    info.usedBy = uby.type();
    info.usedByName = uby.name();
    info.fstab_options = fstab_opt;
    info.uuid = uuid;
    info.label = label;
    info.encryption = encryption;
    info.crypt_pwd = crypt_pwd;
    info.fs = fs;
    info.format = format;
    info.create = create;
    info.mkfs_options = mkfs_opt;
    info.loop = loop_dev;
    info.is_mounted = is_mounted;
    info.resize = size_k!=orig_size_k;
    if( info.resize )
	info.OrigSizeK = orig_size_k;
    else
	info.OrigSizeK = 0;
    tinfo = info;
    }

void Volume::mergeFstabInfo( VolumeInfo& tinfo, const FstabEntry& fste ) const
    {
    info.mount = fste.mount;
    info.mount_by = fste.mount_by;
    info.fstab_options = mergeString( fste.opts, "," );
    info.encryption = fste.encr;
    tinfo = info;
    }

ostream& Volume::logVolume( ostream& file ) const
    {
    file << dev << " fs=" << fs_names[fs];
    if( !uuid.empty() )
	file << " uuid=" << uuid;
    if( !label.empty() )
	file << " label=" << label;
    if( !mp.empty() )
	file << " mount=" << mp;
    if( !fstab_opt.empty() )
	file << " fstopt=" << fstab_opt;
    if( mount_by != MOUNTBY_DEVICE )
	file << " mountby=" << mb_names[mount_by];
    if( is_loop && !fstab_loop_dev.empty() )
	file << " loop=" << fstab_loop_dev;
    if( is_loop && encryption!=ENC_NONE )
	file << " encr=" << enc_names[encryption];
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    if( is_loop && encryption!=ENC_NONE && !crypt_pwd.empty() )
	s << " pwd:" << v.crypt_pwd;
#endif
    file << endl;
    return( file );
    }

void Volume::getTestmodeData( const string& data )
    {
    list<string> l = splitString( data );
    if( l.begin()!=l.end() )
	l.erase( l.begin() );
    map<string,string> m = makeMap( l );
    map<string,string>::const_iterator i = m.find( "fs" );
    if( i!=m.end() )
	fs = detected_fs = toFsType(i->second);
    i = m.find( "uuid" );
    if( i!=m.end() )
	uuid = i->second;
    i = m.find( "label" );
    if( i!=m.end() )
	label = orig_label = i->second;
    i = m.find( "mount" );
    if( i!=m.end() )
	mp = orig_mp = i->second;
    i = m.find( "mountby" );
    if( i!=m.end() )
	mount_by = orig_mount_by = toMountByType(i->second);
    i = m.find( "fstopt" );
    if( i!=m.end() )
	fstab_opt = orig_fstab_opt = i->second;
    i = m.find( "loop" );
    if( i!=m.end() )
	{
	loop_dev = fstab_loop_dev = i->second;
	is_loop = true;
	loop_active = true;
	}
    i = m.find( "encr" );
    if( i!=m.end() )
	encryption = orig_encryption = toEncType(i->second);
    i = m.find( "pwd" );
    if( i!=m.end() )
	crypt_pwd = i->second;
    }

std::ostream& operator<< (std::ostream& s, const Volume &v )
    {
    s << "Device:" << v.dev;
    if( v.numeric )
	s << " Nr:" << v.num;
    else
	s << " Name:" << v.nm;
    s << " SizeK:" << v.size_k;
    if( v.size_k != v.orig_size_k )
	s << " orig_SizeK:" << v.orig_size_k;
    s << " Node <" << v.mjr << ":" << v.mnr << ">";
    if( v.ronly )
	s << " readonly";
    if( v.del )
	s << " deleted";
    if( v.create )
	s << " created";
    if( v.format )
	s << " format";
    s << v.uby;
    if( v.fs != storage::FSUNKNOWN )
	{
	s << " fs:" << Volume::fs_names[v.fs];
	if( v.fs != v.detected_fs && v.detected_fs!=storage::FSUNKNOWN )
	    s << " det_fs:" << Volume::fs_names[v.detected_fs];
	}
    if( v.mp.length()>0 )
	{
	s << " mount:" << v.mp;
	if( v.mp != v.orig_mp && v.orig_mp.length()>0 )
	    s << " orig_mount:" << v.orig_mp;
	if( !v.is_mounted )
	    s << " not_mounted";
	}
    if( v.mount_by != storage::MOUNTBY_DEVICE )
	{
	s << " mount_by:" << Volume::mb_names[v.mount_by];
	if( v.mount_by != v.orig_mount_by )
	    s << " orig_mount_by:" << Volume::mb_names[v.orig_mount_by];
	}
    if( v.uuid.length()>0 )
	{
	s << " uuid:" << v.uuid;
	}
    if( v.label.length()>0 )
	{
	s << " label:" << v.label;
	if( v.label != v.orig_label && v.orig_label.length()>0 )
	    s << " orig_label:" << v.orig_label;
	}
    if( v.fstab_opt.length()>0 )
	{
	s << " fstopt:" << v.fstab_opt;
	if( v.fstab_opt != v.orig_fstab_opt && v.orig_fstab_opt.length()>0 )
	    s << " orig_fstopt:" << v.orig_fstab_opt;
	}
    if( v.mkfs_opt.length()>0 )
	{
	s << " mkfsopt:" << v.mkfs_opt;
	}
    if( v.alt_names.begin() != v.alt_names.end() )
	{
	s << " alt_names:" << v.alt_names;
	}
    if( v.is_loop )
	{
	if( v.loop_active )
	    s << " active";
	s << " loop:" << v.loop_dev;
	if( v.fstab_loop_dev != v.loop_dev )
	    {
	    s << " fstab_loop:" << v.fstab_loop_dev;
	    }
	s << " encr:" << v.enc_names[v.encryption];
	if( v.encryption != v.orig_encryption && v.orig_encryption!=storage::ENC_NONE )
	    s << " orig_encr:" << v.enc_names[v.orig_encryption];
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
	s << " pwd:" << v.crypt_pwd;
#endif
	}
    return( s );
    }

string
Volume::logDifference( const Volume& rhs ) const
    {
    string ret = "Device:" + dev;
    if( dev!=rhs.dev )
	ret += "-->"+rhs.dev;
    if( numeric && num!=rhs.num )
	ret += " Nr:" + decString(num) + "-->" + decString(rhs.num);
    if( !numeric && nm!=rhs.nm )
	ret += " Name:" + nm + "-->" + rhs.nm;
    if( size_k!=rhs.size_k )
	ret += " SizeK:" + decString(size_k) + "-->" + decString(rhs.size_k);
    if( orig_size_k!=rhs.orig_size_k )
	ret += " orig_SizeK:" + decString(orig_size_k) + "-->" + decString(rhs.size_k);
    if( mjr!=rhs.mjr )
	ret += " SizeK:" + decString(mjr) + "-->" + decString(rhs.mjr);
    if( mnr!=rhs.mnr )
	ret += " SizeK:" + decString(mnr) + "-->" + decString(rhs.mnr);
    if( del!=rhs.del )
	{
	if( rhs.del )
	    ret += " -->deleted";
	else
	    ret += " deleted-->";
	}
    if( create!=rhs.create )
	{
	if( rhs.create )
	    ret += " -->created";
	else
	    ret += " created-->";
	}
    if( ronly!=rhs.ronly )
	{
	if( rhs.ronly )
	    ret += " -->readonly";
	else
	    ret += " readonly-->";
	}
    if( format!=rhs.format )
	{
	if( rhs.format )
	    ret += " -->format";
	else
	    ret += " format-->";
	}
    if( uby!=rhs.uby )
	{
	std::ostringstream b;
	b << uby << "-->" << string(rhs.uby);
	ret += b.str();
	}
    if( fs!=rhs.fs )
	ret += " fs:" + Volume::fs_names[fs] + "-->" + Volume::fs_names[rhs.fs];
    if( detected_fs!=rhs.detected_fs )
	ret += " det_fs:" + Volume::fs_names[detected_fs] + "-->" + 
	       Volume::fs_names[rhs.detected_fs];
    if( mp!=rhs.mp )
	ret += " mount:" + mp + "-->" + rhs.mp;
    if( orig_mp!=rhs.orig_mp )
	ret += " orig_mount:" + orig_mp + "-->" + rhs.orig_mp;
    if( is_mounted!=rhs.is_mounted )
	{
	if( rhs.is_mounted )
	    ret += " -->mounted";
	else
	    ret += " mounted-->";
	}
    if( mount_by!=rhs.mount_by )
	ret += " mount_by:" + Volume::mb_names[mount_by] + "-->" + 
	       Volume::mb_names[rhs.mount_by];
    if( orig_mount_by!=rhs.orig_mount_by )
	ret += " orig_mount_by:" + Volume::mb_names[orig_mount_by] + "-->" + 
	       Volume::mb_names[rhs.orig_mount_by];
    if( uuid!=rhs.uuid )
	ret += " uuid:" + uuid + "-->" + rhs.uuid;
    if( label!=rhs.label )
	ret += " label:" + label + "-->" + rhs.label;
    if( orig_label!=rhs.orig_label )
	ret += " orig_label:" + orig_label + "-->" + rhs.orig_label;
    if( fstab_opt!=rhs.fstab_opt )
	ret += " fstopt:" + fstab_opt + "-->" + rhs.fstab_opt;
    if( orig_fstab_opt!=rhs.orig_fstab_opt )
	ret += " orig_fstopt:" + orig_fstab_opt + "-->" + rhs.orig_fstab_opt;
    if( mkfs_opt!=rhs.mkfs_opt )
	ret += " mkfsopt:" + mkfs_opt + "-->" + rhs.mkfs_opt;
    if( is_loop!=rhs.is_loop )
	{
	if( rhs.is_loop )
	    ret += " -->loop";
	else
	    ret += " loop-->";
	}
    if( loop_active!=rhs.loop_active )
	{
	if( rhs.loop_active )
	    ret += " -->loop_active";
	else
	    ret += " loop_active-->";
	}
    if( loop_dev!=rhs.loop_dev )
	ret += " loop:" + loop_dev + "-->" + rhs.loop_dev;
    if( fstab_loop_dev!=rhs.fstab_loop_dev )
	ret += " fstab_loop:" + fstab_loop_dev + "-->" + rhs.fstab_loop_dev;
    if( encryption!=rhs.encryption )
	ret += " encr:" + Volume::enc_names[encryption] + "-->" + 
	       Volume::enc_names[rhs.encryption];
    if( orig_encryption!=rhs.orig_encryption )
	ret += " orig_encr:" + Volume::enc_names[orig_encryption] + "-->" + 
	       Volume::enc_names[rhs.orig_encryption];
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    if( crypt_pwd!=rhs.crypt_pwd )
	ret += " pwd:" + crypt_pwd + "-->" + rhs.crypt_pwd;
#endif
    return( ret );
    }


bool Volume::equalContent( const Volume& rhs ) const
    {
    return( dev==rhs.dev && numeric==rhs.numeric && 
            ((numeric&&num==rhs.num)||(!numeric&&nm==rhs.nm)) &&
	    size_k==rhs.size_k && mnr==rhs.mnr && mjr==rhs.mjr &&
	    ronly==rhs.ronly && create==rhs.create && del==rhs.del && 
	    silent==rhs.silent && format==rhs.format && 
	    fstab_added==rhs.fstab_added && 
	    fs==rhs.fs && mount_by==rhs.mount_by &&
	    uuid==rhs.uuid && label==rhs.label && mp==rhs.mp &&
	    fstab_opt==rhs.fstab_opt && mkfs_opt==rhs.mkfs_opt &&
	    is_loop==rhs.is_loop && loop_active==rhs.loop_active &&
	    is_mounted==rhs.is_mounted && encryption==rhs.encryption &&
	    loop_dev==rhs.loop_dev && fstab_loop_dev==rhs.fstab_loop_dev &&
	    uby==rhs.uby );
    }

Volume& Volume::operator= ( const Volume& rhs )
    {
    y2milestone( "operator= from %s", rhs.dev.c_str() );
    dev = rhs.dev;
    numeric = rhs.numeric;
    num = rhs.num;
    nm = rhs.nm;
    size_k = rhs.size_k;
    orig_size_k = rhs.orig_size_k;
    mnr = rhs.mnr;
    mjr = rhs.mjr;
    ronly = rhs.ronly;
    create = rhs.create;
    del = rhs.del;
    silent = rhs.silent;
    format = rhs.format;
    fstab_added = rhs.fstab_added;
    fs = rhs.fs;
    detected_fs = rhs.detected_fs;
    mount_by = rhs.mount_by;
    orig_mount_by = rhs.orig_mount_by;
    uuid = rhs.uuid;
    label = rhs.label;
    orig_label = rhs.orig_label;
    mp = rhs.mp;
    orig_mp = rhs.orig_mp;
    fstab_opt = rhs.fstab_opt;
    orig_fstab_opt = rhs.orig_fstab_opt;
    mkfs_opt = rhs.mkfs_opt;
    is_loop = rhs.is_loop;
    loop_active = rhs.loop_active;
    is_mounted = rhs.is_mounted;
    encryption = rhs.encryption;
    orig_encryption = rhs.orig_encryption;
    loop_dev = rhs.loop_dev;
    fstab_loop_dev = rhs.fstab_loop_dev;
    crypt_pwd = rhs.crypt_pwd;
    uby = rhs.uby;
    alt_names = rhs.alt_names;
    return( *this );
    }

Volume::Volume( const Volume& rhs ) : cont(rhs.cont)
    {
    y2milestone( "constructed vol by copy constructor from %s", 
                 rhs.dev.c_str() );
    *this = rhs;
    }

string Volume::fs_names[] = { "unknown", "reiserfs", "ext2", "ext3", "vfat",
                              "xfs", "jfs", "hfs", "ntfs", "swap", "none" };

string Volume::mb_names[] = { "device", "uuid", "label" };

string Volume::enc_names[] = { "none", "twofish256", "twofish", 
                               "twofishSL92", "unknown" };


