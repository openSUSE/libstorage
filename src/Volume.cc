#include <sstream>

#include <features.h>
#include <sys/stat.h>
#include <ycp/y2log.h>


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
    nm = dev.substr( 5 );
    }

void Volume::init()
    {
    del = create = format = is_loop = loop_active = silent = false;
    is_mounted = ronly = false;
    detected_fs = fs = FSUNKNOWN;
    mount_by = orig_mount_by = MOUNTBY_DEVICE;
    encryption = orig_encryption = ENC_NONE;
    setNameDev();
    mjr = mnr = 0;
    getMajorMinor( dev, mjr, mnr );
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
    else if( nm != rhs.nm )
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
	if( !found && (uuid.size()>0||label.size()>0) )
	    {
	    found = fstabData.findUuidLabel( uuid, label, entry );
	    fstabData.setDevice( entry, device() );
	    }
	}
    if( !found && mp.size()>0 )
	{
	found = fstabData.findMount( mp, entry );
	}
    if( found )
	{
	std::ostringstream b;
	b << "line[" << device() << "]=";
	b << "noauto:" << entry.noauto;
	if( mp.size()==0 )
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
    if( mp.size()==0 )
	{
	mp = mountData.getMount( alt_names );
	}
    if( mp.size()>0 )
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
	if( l.size()>0 )
	    {
	    list<string>::const_iterator el = l.begin();
	    is_loop = loop_active = true;
	    loop_dev = *el;
	    if( loop_dev.size()>0 && *loop_dev.rbegin()==':' )
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
	list<string> l = splitString( *blkidData.getLine( 0, true ));
	std::ostringstream b;
	b << "line[" << device() << "]=" << l;
	y2milestone( "%s", b.str().c_str() );
	if( l.size()>0 )
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
	fs = detected_fs;
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
    if( (m.size()>0 && m[0]!='/' && m!="swap") ||
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
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::changeMountBy( MountByType mby )
    {
    int ret = 0;
    y2milestone( "device:%s mby:%s", dev.c_str(), mbyTypeString(mby).c_str() );
    if( mp.size()>0 )
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
    if( mp.size()>0 )
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
		if( fstab_loop_dev.size()>0 )
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
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting device %1$s %2$s with %3$s "),
		       dev.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	string d = dev.substr( 5 );
	if( mp.size()>0 )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format device %1$s %2$s for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format crypted device %1$s %2$s for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format device %1$s %2$s with %3$s"),
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
    if( ret==0 )
	{
	string cmd;
	string params;
	ScrollBarHandler* p = NULL;
	CallbackProgressBar cb = cont->getStorage()->getCallbackProgressBar();
	
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
	    if( mkfs_opt.size()>0 )
		{
		cmd += mkfs_opt + " ";
		}
	    if( params.size()>0 )
		{
		cmd += params + " ";
		}
	    cmd += mountDevice();
	    SystemCmd c;
	    c.setOutputProcessor( p );
	    c.execute( cmd );
	    if( c.retcode()!=0 )
		ret = VOLUME_FORMAT_FAILED;
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
    if( ret==0 )
	{
	format = false;
	detected_fs = fs;
	if( fs != SWAP && !cont->getStorage()->test() )
	    {
	    SystemCmd Blkid( "/sbin/blkid -c /dev/null " + mountDevice() );
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
    if( ret==0 && label.size()>0 )
	{
	ret = doSetLabel();
	}
    if( needMount )
	{
	int r = mount( orig_mp );
	ret = (ret==0)?r:ret;
	}
    if( ret==0 && orig_mp.size()>0 )
	{
	ret = doFstabUpdate();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::umount( const string& mp )
    {
    SystemCmd cmd;
    y2milestone( "device:%s mp:%s", dev.c_str(), mp.c_str() );
    string cmdline = ((fs != SWAP)?"umount ":"swapoff ") + mountDevice();
    int ret = cmd.execute( cmdline );
    if( fs != SWAP && ret != 0 && mp.size()>0 )
	{
	cmdline = "umount " + mp;
	ret = cmd.execute( cmdline );
	}
    if( ret != 0 )
	ret = VOLUME_UMOUNT_FAILED;
    else 
	is_mounted = false;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string Volume::mountText( bool doing ) const
    {
    string txt;
    if( doing )
        {
	if( mp.size()>0 )
	    {
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by mount point e.g. /home
	    txt = sformat( _("Mounting %1$s to %2$s"), dev.c_str(), mp.c_str() );
	    }
	else
	    {
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Umounting %1$s"), dev.c_str() );
	    }
        }
    else
        {
	string d = dev.substr( 5 );
	if( orig_mp.size()>0 && mp.size()>0 )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. hda1
	    // %2$s is replaced by mount point e.g. /home
	    txt = sformat( _("Change mount point of %1$s to %2$s"), d.c_str(),
	                   mp.c_str() );
	    }
	else if( mp.size()>0 )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. hda1
	    // %2$s is replaced by mount point e.g. /home
	    txt = sformat( _("Set mount point of %1$s to %2$s"), d.c_str(), 
	                   mp.c_str() );
	    }
	else if( orig_mp.size()>0 )
	    {
	    string fn = "/etc/fstab";
	    if( encryption!=ENC_NONE && !optNoauto() )
		fn = "/etc/cryptotab";
	    // displayed text before action, %1$s is replaced by device name e.g. hda1
	    // %2$s is replaced by pathname e.g. /etc/fstab
	    txt = sformat( _("Remove %1$s from %2$s"), d.c_str(), fn.c_str() );
	    }
        }
    return( txt );
    }

int Volume::doMount()
    {
    int ret = 0;
    string lmount = cont->getStorage()->root()+mp;
    y2milestone( "device:%s mp:%s old mp:%s",  dev.c_str(), mp.c_str(),
                 orig_mp.c_str() );
    if( !silent )
	{
	cont->getStorage()->showInfoCb( mountText(true) );
	}
    if( uby.t != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    if( ret==0 && orig_mp.size()>0 && isMounted() )
	{
	ret = umount( cont->getStorage()->root()+orig_mp );
	}
    if( ret==0 && access( lmount.c_str(), R_OK )!=0 )
	{
	createPath( lmount );
	}
    if( ret==0 && mp.size()>0 )
	{
	ret = mount( lmount );
	}
    if( ret==0 )
	{
	ret = doFstabUpdate();
	orig_mp = mp;
	}
    if( ret==0 && mp=="/" && cont->getStorage()->root().size()>0 )
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
		    ret = VOLUME_RESIZE_FAILED;
		break;
	    case NTFS:
		cmd = "echo y | ntfsresize -f ";
		if( needShrink() )
		    cmd += "-s " + decString(size_k) + "k ";
		cmd += mountDevice();
		c.execute( cmd );
		if( c.retcode()!=0 )
		    ret = VOLUME_RESIZE_FAILED;
		break;
	    case EXT2:
	    case EXT3:
		cmd = "resize2fs -f " + mountDevice();
		if( needShrink() )
		    cmd += " " + decString(size_k) + "K";
		c.execute( cmd );
		if( c.retcode()!=0 )
		    ret = VOLUME_RESIZE_FAILED;
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
			ret = VOLUME_RESIZE_FAILED;
		    }
		if( needumount )
		    {
		    int r = umount( mpoint );
		    ret = (ret!=0)?ret:r;
		    }
		if( needrmdir )
		    {
		    rmdir( mpoint.c_str() );
		    }
		}
		break;
	    default:
		break;
	    }
	if( cmd.size()==0 )
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
	    if( loop_dev.size()>0 && loop_active )
		{
		SystemCmd c( "losetup -d " + loop_dev );
		loop_dev.erase();
		fstab_loop_dev = loop_dev;
		}
	    is_loop = loop_active = false;
	    encryption = orig_encryption = ENC_NONE;
	    crypt_pwd.erase();
	    }
	else
	    {
	    if( crypt_pwd.empty() )
		ret = VOLUME_CRYPT_NO_PWD;
	    if( ret == 0 && format )
		{
		encryption = ENC_TWOFISH;
		is_loop = true;
		}
	    if( ret == 0 && !format && !loop_active )
		{
		if( detectLoopEncryption()==ENC_UNKNOWN )
		    ret = VOLUME_CRYPT_NOT_DETECTED;
		}
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string Volume::losetupText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Setting up encrypted loop device on %1$s"), dev.c_str() );
        }
    else
        {
	string d = dev.substr( 5 );
	// displayed text before action, %1$s is replaced by device name e.g. hda1
        txt = sformat( _("Set up encrypted loop device on %1$s"), d.c_str() );
        }
    return( txt );
    }

bool Volume::loopStringNum( const string& name, unsigned& num )
    {
    bool ret=false;
    string d(name);
    if( d.find( "/dev/" )==0 )
	d.erase( 0, 5 );
    static Regex loop( "^loop[0-9]+$" );
    if( loop.match( d ))
	{
	d.substr( 4 )>>num;
	ret = true;
	}
    return( ret );
    }

bool hasLoopDevice( const Volume& v ) { return( v.loopDevice().size()>0 ); }

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
    if( loop_dev.size()==0 )
	{
	list<unsigned> lnum;
	Storage::ConstVolPair p = cont->getStorage()->volPair( hasLoopDevice );
	for( Storage::ConstVolIterator i=p.begin(); i!=p.end(); ++i )
	    {
	    unsigned num;
	    if( loopStringNum( i->loopDevice(), num ))
		lnum.push_back( num );
	    }
	cout << "lnum:" << lnum << endl;
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
    cmd += dev;
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

    if( val.size()<5 )
	ret = VOLUME_CRYPT_PWD_TOO_SHORT;
    else
	crypt_pwd=val; 

    y2milestone( "ret:%d", ret );
    return( ret );
    }

EncryptType Volume::detectLoopEncryption()
    {
    EncryptType ret = ENC_UNKNOWN;

    if( getContainer()->getStorage()->test() )
    {
	ret = ENC_TWOFISH;
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
	    c.execute( "/sbin/blkid -c /dev/null " + mountDevice() );
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
			    cmd = "reiserfsck --check -q " + loop_dev;
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
    while( detected_fs==FSUNKNOWN && pos<sizeof(try_order)/sizeof(try_order[0]) );
    if( detected_fs!=FSUNKNOWN )
	{
	is_loop = true;
	loop_active = false;
	ret = encryption = orig_encryption = try_order[pos];
	}
    c.execute( "losetup -d " + loop_dev );
    unlink( fname.c_str() );
    rmdir( mpname.c_str() );
    y2milestone( "ret:%s", encTypeString(ret).c_str() );
    return( ret );
    }

int Volume::doLosetup()
    {
    int ret = 0;
    y2milestone( "device:%s mp:%s", dev.c_str(), mp.c_str() );
    if( !silent )
	{
	cont->getStorage()->showInfoCb( losetupText(true) );
	}
    if( ret==0 && loop_dev.size()==0 )
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string Volume::labelText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	// %2$s is replaced by a name e.g. ROOT
        txt = sformat( _("Setting label on %1$s to %2$s"), dev.c_str(), label.c_str() );
        }
    else
        {
	string d = dev.substr( 5 );
	// displayed text before action, %1$s is replaced by device name e.g. hda1
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
	if( cmd.size()>0 )
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
	string lmount = (m.size()>0)?m:mp;
	y2milestone( "device:%s mp:%s", dev.c_str(), lmount.c_str() );
	cmdline = "modprobe " + fs_names[fs];
	cmd.execute( cmdline );
	cmdline = "mount -t " + fs_names[fs] + " " + mountDevice() + " " + 
	          lmount;
	}
    else
	{
	cmdline = "swapon " + mountDevice();
	}
    int ret = cmd.execute( cmdline );
    if( ret != 0 )
	ret = VOLUME_MOUNT_FAILED;
    else
	is_mounted = true;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::prepareRemove()
    {
    int ret = 0;
    y2milestone( "device:%s", dev.c_str() );
    if( orig_mp.size()>0 )
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
    if( orig_mp.size()>0 && mp.size()==0 )
	txt = fstab->removeText( false, inCrypto(), orig_mp );
    else
	txt = fstab->updateText( false, inCrypto(), orig_mp );
    return( txt );
    }

int Volume::doFstabUpdate()
    {
    int ret = 0;
    bool changed = false;
    y2milestone( "device:%s mp:%s options:%s",  dev.c_str(), mp.c_str(), 
                 fstab_opt.c_str() );
    EtcFstab* fstab = cont->getStorage()->getFstab();
    FstabEntry entry;
    y2milestone( "del:%d", deleted() );
    if( orig_mp.size()>0 && (deleted()||mp.size()==0) &&
        (fstab->findDevice( dev, entry ) ||
	 fstab->findDevice( alt_names, entry ) ))
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
    else if( mp.size()>0 && !deleted() )
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
		che.opts = splitString( fstab_opt, "," );
		}
	    if( mount_by!=orig_mount_by || (format && mount_by==MOUNTBY_UUID) ||
	        (orig_label!=label && mount_by==MOUNTBY_LABEL) )
		{
		changed = true;
		che.dentry = getMountByString( mount_by, dev, uuid, label );
		}
	    if( fs != detected_fs )
		{
		changed = true;
		che.fs = fs_names[fs];
		}
	    if( encryption != orig_encryption )
		{
		changed = true;
		che.encr = encryption;
		che.loop_dev = fstab_loop_dev;
		}
	    if( changed )
		{
		if( !silent )
		    {
		    cont->getStorage()->showInfoCb( 
			fstab->updateText( true, inCrypto(), che.mount ));
		    }
		ret = fstab->updateEntry( che );
		}
	    }
	else
	    {
	    changed = true;
	    FstabChange che;
	    che.device = dev;
	    che.dentry = getMountByString( mount_by, dev, uuid, label );
	    che.encr = encryption;
	    che.loop_dev = fstab_loop_dev;
	    che.fs = fs_names[fs];
	    string fst = fstab_opt;
	    if( fst.size()==0 )
		{
		if( is_loop )
		    fst = "noatime";
		else
		    fst = (mp=="swap") ? "pri=42" : "defaults";
		}
	    che.opts = splitString( fst, "," );
	    che.mount = mp;
	    if( fs != FSUNKNOWN && fs != NTFS && fs != VFAT && !is_loop &&
	        !optNoauto() )
		{
		che.freq = 1;
		che.passno = (mp=="/") ? 1 : 2;
		}
	    if( !silent )
		{
		cont->getStorage()->showInfoCb( fstab->addText( true, inCrypto(),
		                                                che.mount ));
		}
	    ret = fstab->addEntry( che );
	    }
	}
    if( changed && ret==0 && cont->getStorage()->isRootMounted() )
	{
	ret = fstab->flush();
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
    if( val=="none" || val.size()==0 )
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
    bool ret = getUsedByType()==UB_NONE && getMount().size()==0;
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
    if( !need && mp.size()>0 && !isMounted() && !optNoauto() )
	need = true;
    return( need );
    }

bool Volume::optNoauto() const
    {
    list<string> l = splitString( fstab_opt, "," );
    return( find( l.begin(), l.end(), "noauto" )!=l.end() );
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
    if( doing )
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrinking %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extending %1$s to %2$s"), dev.c_str(), sizeString().c_str() );

        }
    else
        {
	string d = dev.substr( 5 );
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

ostream& Volume::logVolume( ostream& file ) const
    {
    file << dev << " fs=" << fs_names[fs];
    if( uuid.size()>0 )
	file << " uuid=" << uuid;
    if( label.size()>0 )
	file << " label=" << label;
    if( mp.size()>0 )
	file << " mount=" << mp;
    if( fstab_opt.size()>0 )
	file << " fstopt=" << fstab_opt;
    if( mount_by != MOUNTBY_DEVICE )
	file << " mountby=" << mb_names[mount_by];
    if( is_loop && fstab_loop_dev.size()>0 )
	file << " loop=" << fstab_loop_dev;
    if( is_loop && encryption!=ENC_NONE )
	file << " encr=" << enc_names[encryption];
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    if( is_loop && encryption!=ENC_NONE && crypt_pwd.size()>0 )
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

string Volume::fs_names[] = { "unknown", "reiserfs", "ext2", "ext3", "vfat",
                              "xfs", "jfs", "ntfs", "swap", "none" };

string Volume::mb_names[] = { "device", "uuid", "label" };

string Volume::enc_names[] = { "none", "twofish256", "twofish", 
                               "twofishSL92", "unknown" };


