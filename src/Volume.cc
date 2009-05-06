/*
  Textdomain    "storage"
*/

#include <features.h>
#include <sys/stat.h>
#include <sstream>
#include <fstream>

#include "y2storage/Volume.h"
#include "y2storage/Disk.h"
#include "y2storage/LoopCo.h"
#include "y2storage/Storage.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/Container.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/ProcMounts.h"
#include "y2storage/ProcPart.h"
#include "y2storage/OutputProcessor.h"
#include "y2storage/EtcFstab.h"
#include "y2storage/StorageDefines.h"
#include "y2storage/HumanString.h"


namespace storage
{
    using namespace std;


Volume::Volume(const Container& d, unsigned PNr, unsigned long long SizeK)
    : cont(&d)
{
    numeric = true;
    num = PNr;
    size_k = orig_size_k = SizeK;
    init();
    y2deb("constructed volume " << ((num>0)?dev:"") << " on disk " << cont->name());
}


Volume::Volume(const Container& c, const string& Name, unsigned long long SizeK)
    : cont(&c)
{
    numeric = false;
    nm = Name;
    size_k = orig_size_k = SizeK;
    init();
    y2deb("constructed volume " << dev << " on disk " << cont->name());
}


Volume::Volume(const Container& c)
    : cont(&c)
{
    numeric = false;
    size_k = orig_size_k = 0;
    init();
    y2deb("constructed late init volume");
}


Volume::~Volume()
{
    y2deb("destructed volume " << dev);
}


void Volume::setNameDev()
    {
    std::ostringstream Buf_Ci;
    classic(Buf_Ci);
    if( numeric )
	Buf_Ci << cont->device() << Disk::partNaming(cont->device()) << num;
    else
	Buf_Ci << cont->device() << "/" << nm;
    dev = Buf_Ci.str();
    if( nm.empty() )
	nm = dev.substr( 5 );
    }

// TODO: maybe obsoleted by function setDmcryptDevEnc
void Volume::setDmcryptDev( const string& dm, bool active )
    {
    y2mil( "dev:" << dev << " dm:" << dm << " active:" << active );
    dmcrypt_dev = dm;
    dmcrypt_active = active;
    y2mil( "this:" << *this );
    }

void Volume::setDmcryptDevEnc( const string& dm, storage::EncryptType typ, bool active )
    {
    y2mil( "dev:" << dev << " dm:" << dm << " type:" << typ << " active:" << active );
    dmcrypt_dev = dm;
    encryption = orig_encryption = typ;
    dmcrypt_active = active;
    y2mil( "this:" << *this );
    }

bool Volume::sameDevice( const string& device ) const
    {
    string d = normalizeDevice(device);
    return( d==dev ||
            find( alt_names.begin(), alt_names.end(), d )!=alt_names.end() );
    }

const string& Volume::mountDevice() const
    {
    if( dmcrypt() && !dmcrypt_dev.empty() )
	return( dmcrypt_dev );
    else
	return( is_loop?loop_dev:dev );
    }


storage::MountByType
Volume::defaultMountBy(const string& mp) const
{
    MountByType mb = cont->getStorage()->getDefaultMountBy();
    y2mil( "mby:" << mb_names[mb] << " type:" << cType() );
    if ((cType() != DISK && cType() != DMRAID && cType() != DMMULTIPATH) && (mb == MOUNTBY_ID || mb == MOUNTBY_PATH))
	mb = MOUNTBY_DEVICE;
    if (cType() == NFSC && mb != MOUNTBY_DEVICE)
	mb = MOUNTBY_DEVICE;
    if( mp=="swap" && mb==MOUNTBY_UUID )
	mb = MOUNTBY_DEVICE;
    y2mil( "path:" << udevPath() << " id:" << udevId() );
    if( (mb==MOUNTBY_PATH && udevPath().empty()) ||
        (mb==MOUNTBY_ID && udevId().empty()) )
	mb = MOUNTBY_DEVICE;
    if( encryption != ENC_NONE &&
	(mb==MOUNTBY_UUID || mb==MOUNTBY_LABEL) )
	mb = MOUNTBY_DEVICE;
    y2mil( "dev:" << dev << " mp:" << mp << " mby:" << mb_names[mb] );
    return mb;
}


// TODO: allowedMountBy is never used
bool 
Volume::allowedMountBy(storage::MountByType mby, const string& mp) const
{
    bool ret = true;
    if ((cType() != DISK && cType() != DMRAID && cType() != DMMULTIPATH) && (mby == MOUNTBY_ID || mby == MOUNTBY_PATH))
	ret = false;
    if (mp == "swap" && mby == MOUNTBY_UUID)
	ret = false;
    if (cType() == NFSC && mby != MOUNTBY_DEVICE)
	ret = false;
    if( (mby==MOUNTBY_PATH && udevPath().empty()) ||
	(mby==MOUNTBY_ID && udevId().empty()) )
	ret = false;
    if( ret && encryption != ENC_NONE &&
        (mby==MOUNTBY_UUID || mby==MOUNTBY_LABEL) )
	ret = false;
    y2mil( "mby:" << mb_names[mby] << " mp:" << mp << " ret:" << ret );
    return ret;
}


void Volume::init()
    {
    del = create = format = is_loop = loop_active = silnt = false;
    is_mounted = ronly = fstab_added = ignore_fstab = ignore_fs = false;
    dmcrypt_active = false;
    detected_fs = fs = FSUNKNOWN;
    encryption = orig_encryption = ENC_NONE;
    mjr = mnr = 0;
    if( numeric||!nm.empty() )
	{
	setNameDev();
	getMajorMinor( dev, mjr, mnr );
	}
    if( !numeric )
	num = 0;
    mount_by = orig_mount_by = defaultMountBy();
    }

CType Volume::cType() const
    {
    return( cont->type() );
    }

bool Volume::operator== ( const Volume& rhs ) const
    {
    return( (*cont)==(*rhs.cont) &&
            ((numeric && num == rhs.num) || (!numeric && nm == rhs.nm)) &&
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

void Volume::getFsInfo( const Volume* source )
    {
    setFs( source->getFs() );
    setFormat( source->getFormat(), source->getFs() );
    setUuid( source->getUuid() );
    initLabel( source->getLabel() );
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
	if( !found && !(udevId().empty()&&udevPath().empty()) )
	    {
	    found = fstabData.findIdPath( udevId(), udevPath(), entry );
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
	classic(b);
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
	fstab_opt = orig_fstab_opt = boost::join( entry.opts, "," );
	b << " fstopt:" << fstab_opt;
	if( !is_loop && entry.loop )
	    {
	    is_loop = true;
	    orig_encryption = encryption = entry.encr;
	    loop_dev = fstab_loop_dev = entry.loop_dev;
	    b << " loop_dev:" << loop_dev << " encr:" << enc_names[encryption];
	    }
	y2mil(b.str());
	}
    }

void Volume::getStartData()
    {
    if( fs==FSUNKNOWN && encryption==ENC_NONE )
	{
	char buf[10];
	ifstream file( dev.c_str() );
	classic(file);
	file.read( buf, sizeof(buf) );
	if( file.good() && strncmp( buf, "LUKS", 4 )==0 )
	    setEncryption( ENC_LUKS );
	file.close();
	}
    }

void Volume::getMountData( const ProcMounts& mountData, bool swap_only )
    {
    y2mil( "this:" << *this );
    y2mil( "swap_only:" << swap_only << " mountDevice:" << mountDevice() );
    mp = mountData.getMount( mountDevice() );
    if( mp.empty() )
	{
	mp = mountData.getMount( alt_names );
	}
    if( !mp.empty() )
	{
	is_mounted = true;
	if( swap_only && mp!="swap" )
	    {
	    is_mounted = false;
	    mp.clear();
	    }
	if( is_mounted )
	    y2mil(device() << " mounted on " << mp);
	}
    orig_mp = mp;
    }

void Volume::getLoopData( SystemCmd& loopData )
    {
    bool found = false;
    if( cont->type()==LOOP )
	{
	if( !dmcrypt() )
	    {
	    Loop* l = static_cast<Loop*>(this);
	    found = loopData.select( " (" + l->loopFile() + ")" )>0;
	    }
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
	list<string> l = splitString( loopData.getLine( 0, true ));
	std::ostringstream b;
	classic(b);
	b << "line[" << device() << "]=" << l;
	y2mil(b.str());
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
	    y2mil(b.str());
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
	list<string> l = splitString( blkidData.getLine( 0, true ), " \t\n",
	                              true, true, "\"" );
	std::ostringstream b;
	classic(b);
	b << "line[" << device() << "]=" << l;
	y2mil(b.str());
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
		else if( i->second == "ext4" )
		    {
		    fs = EXT4;
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
		else if( i->second == "hfsplus" )
		    {
		    fs = HFSPLUS;
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
		alt_names.remove_if(string_contains("/by-uuid/"));
		alt_names.push_back( "/dev/disk/by-uuid/" + uuid );
		}
	    i = m.find( "LABEL" );
	    if( i != m.end() )
		{
		// ignore label for hfs since we cannot set it (bnc #447782)
		if (fs != HFS)
		    label = orig_label = i->second;
		b << " label:\"" << label << "\"";
		alt_names.remove_if(string_contains("/by-label/"));
		alt_names.push_back( "/dev/disk/by-label/" + label );
		}
	    y2mil(b.str());
	    }
	}
    }

int Volume::setFormat( bool val, storage::FsType new_fs )
    {
    int ret = 0;
    y2mil("device:" << dev << " val:" << val << " fs:" << fs_names[new_fs]);
    format = val;
    if( !format )
	{
	fs = detected_fs;
	mkfs_opt = "";
	tunefs_opt = "";
	}
    else
	{
	FsCapabilities caps;
	if( uby.type() != UB_NONE )
	    {
	    ret = VOLUME_ALREADY_IN_USE;
	    }
	else if( cont->getStorage()->getFsCapabilities( new_fs, caps ) &&
		 caps.minimalFsSizeK > size_k  )
	    {
	    ret = VOLUME_FORMAT_FS_TOO_SMALL;
	    }
	else if( new_fs == NFS )
	    {
	    ret = VOLUME_FORMAT_NFS_IMPOSSIBLE;
	    }
	else
	    {
	    fs = new_fs;
	    FsCapabilities caps;
	    if( !cont->getStorage()->getFsCapabilities( fs, caps ) ||
	        !caps.supportsLabel )
		{
		eraseLabel();
		}
	    else if( caps.labelLength < label.size() )
		{
		label.erase( caps.labelLength );
		}
	    uuid.erase();
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }


int
Volume::changeMount(const string& m)
{
    int ret = 0;
    y2mil("device:" << dev << " mount:" << m);
    if( (!m.empty() && m[0]!='/' && m!="swap") ||
	m.find_first_of( " \t\n" ) != string::npos )
	{
	ret = VOLUME_MOUNT_POINT_INVALID;
	}
    else if( uby.type() != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else
	{
	mp = m;
	if( m.empty() )
	    {
	    orig_fstab_opt = fstab_opt = "";
	    orig_mount_by = mount_by = defaultMountBy(m);
	    }
	/*
	else
	    mount_by = defaultMountBy(m);
	*/
	}
    y2mil("ret:" << ret);
    return ret;
}


int
Volume::changeMountBy(MountByType mby)
{
    int ret = 0;
    y2mil("device:" << dev << " mby:" << mbyTypeString(mby));
    y2mil( "vorher:" << *this );
    if( uby.type() != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else
	{
	if( mby == MOUNTBY_LABEL || mby == MOUNTBY_UUID )
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
	else if( mby == MOUNTBY_ID || mby == MOUNTBY_PATH )
	    {
	    // TODO: why not use allowedMountBy()?
	    if (cType() != DISK && cType() != DMRAID && cType() != DMMULTIPATH)
		ret = VOLUME_MOUNTBY_UNSUPPORTED_BY_VOLUME;
	    }
	if( ret==0 )
	    mount_by = mby;
	}
    y2mil( "nachher:" << *this );
    y2mil( "needFstabUdpate:" << needFstabUpdate() );
    y2mil("ret:" << ret);
    return ret;
}


void Volume::updateFstabOptions()
    {
    list<string> l;
    getFstabOpts( l );
    fstab_opt = boost::join( l, "," );
    }

int Volume::changeFstabOptions( const string& options )
    {
    int ret = 0;
    y2mil("device:" << dev << " options:" << options << " encr:" << encTypeString(encryption));
    if( uby.type() != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else
	{
	fstab_opt = options;
	updateFstabOptions();
	}
    y2mil("ret:" << ret);
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
    y2mil("device:" << dev);
    if( !silent() )
	{
	cont->getStorage()->showInfoCb( formatText(true) );
	}
    if( uby.type() != UB_NONE )
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
    if( ret==0 )
	{
	cont->getStorage()->removeDmTableTo( *this );
	}
    if( ret==0 && encryption!=ENC_NONE )
	{
	ret = Storage::zeroDevice(mountDevice(), size_k, true);
	}
    if( ret==0 && mountDevice()!=dev && !cont->getStorage()->test() )
	{
	ret = checkDevice(mountDevice());
	}
    if( ret==0 && mountDevice().find( "/dev/md" )!=0 &&
        mountDevice().find( "/dev/loop" )!=0 )
	{
	SystemCmd c;
	c.execute(MDADMBIN " --zero-superblock " + quote(mountDevice()));
	}
    if( ret==0 )
	{
	string cmd;
	string params;
	ProgressBar* progressbar = NULL;
	CallbackProgressBar cb = cont->getStorage()->getCallbackProgressBarTheOne();

	switch( fs )
	    {
	    case EXT2:	
		cmd = "/sbin/mke2fs";
		params = "-t ext2 -v";
		progressbar = new Mke2fsProgressBar( cb );
		break;
	    case EXT3:
		cmd = "/sbin/mke2fs";
		params = "-t ext3 -v";
		progressbar = new Mke2fsProgressBar( cb );
		break;
	    case EXT4:
		cmd = "/sbin/mke2fs";
		params = "-t ext4 -v";
		progressbar = new Mke2fsProgressBar( cb );
		break;
	    case REISERFS:
		cmd = "/sbin/mkreiserfs";
		params = "-f -f";
		progressbar = new ReiserProgressBar( cb );
		break;
	    case VFAT:
		cmd = "/sbin/mkdosfs";
		break;
	    case JFS:
		cmd = "/sbin/mkfs.jfs";
		params = "-q";
		break;
	    case HFS:
		cmd = "/usr/bin/hformat";
		break;
	    case HFSPLUS:
		ret = VOLUME_FORMAT_NOT_IMPLEMENTED;
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
	    cmd += quote(mountDevice());
	    SystemCmd c;
	    c.setOutputProcessor(progressbar);
	    c.execute( cmd );
	    if( c.retcode()!=0 )
		{
		ret = VOLUME_FORMAT_FAILED;
		setExtError( c );
		}
	    }
	delete progressbar;
	}

    if (ret == 0)
    {
	switch (fs)
	{
	    case EXT3:
	    case EXT4:
	    {
		if (!tunefs_opt.empty())
		{
		    string cmd = "/sbin/tune2fs " + tunefs_opt + " " + quote(mountDevice());
		    SystemCmd c( cmd );
		    if( c.retcode()!=0 )
			ret = VOLUME_TUNE2FS_FAILED;
		}

		if( ret==0 && mp=="/" &&
		    (fstab_opt.find( "data=writeback" )!=string::npos ||
		     fstab_opt.find( "data=journal" )!=string::npos) )
		{
		    string cmd = "/sbin/tune2fs -o ";
		    if( fstab_opt.find( "data=writeback" )!=string::npos )
			cmd += "journal_data_writeback ";
		    else
			cmd += "journal_data ";
		    cmd += quote(mountDevice());
		    SystemCmd c( cmd );
		    if( c.retcode()!=0 )
			ret = VOLUME_TUNE2FS_FAILED;
		}
	    }
	    break;

	    case REISERFS:
	    {
		if (!tunefs_opt.empty())
		{
		    string cmd = "/sbin/reiserfstune " + tunefs_opt + " " + quote(mountDevice());
		    SystemCmd c( cmd );
		    if( c.retcode()!=0 )
			ret = VOLUME_TUNEREISERFS_FAILED;
		}
	    }
	    break;

	    default:
		break;
	}
    }

    if( ret==0 )
	{
	triggerUdevUpdate();
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
	    FsType old=fs;
	    updateFsData();
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
	// possible change of mp is handled later in doMount
	int r = mount( orig_mp );
	ret = (ret==0)?r:ret;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

void Volume::updateFsData()
    {
    SystemCmd Blkid("BLKID_SKIP_CHECK_MDRAID=1 /sbin/blkid -c /dev/null " + quote(mountDevice()));
    getFsData( Blkid );
    }

string Volume::sysfsPath() const
    {
    string ret = SYSFSDIR "/";
    string::size_type pos = dev.rfind( '/' ) + 1;
    ret += dev.substr( pos );
    y2mil( "ret:" << ret );
    return( ret );
    }

string Volume::getFilesysSysfsPath() const
    {
    string ret;
    if( is_loop )
	{
	ret = SYSFSDIR "/";
	string::size_type pos = loop_dev.rfind( '/' ) + 1;
	ret += loop_dev.substr( pos );
	}
    else
	ret = sysfsPath();
    y2mil( "ret:" << ret );
    return( ret );
    }

void Volume::triggerUdevUpdate() const
    {
    string path = getFilesysSysfsPath() + "/uevent";
    if( access( path.c_str(), R_OK )==0 )
	{
	ofstream file( path.c_str() );
	classic(file);
	if( file.good() )
	    {
	    y2mil( "writing \"add\" to " << path );
	    file << "add" << endl;
	    file.close();
	    Storage::waitForDevice();
	    }
	else
	    y2mil( "error opening " << path << " err:" <<
	           hex << file.rdstate() );
	}
    else
	y2mil( "no access to " << path );
    }

int Volume::umount( const string& mp )
    {
    SystemCmd cmd;
    y2mil("device:" << dev << " mp:" << mp);
    string d = mountDevice();
    if( dmcrypt_active )
	d = dmcrypt_dev;
    else if( loop_active )
	d = loop_dev;
    string cmdline = ((detected_fs != SWAP)?UMOUNTBIN " ":SWAPOFFBIN " ") + quote(d);
    int ret = cmd.execute( cmdline );
    if( ret != 0 && mountDevice()!=dev )
	{
	cmdline = ((detected_fs != SWAP)?UMOUNTBIN " ":SWAPOFFBIN " ") + quote(dev);
	ret = cmd.execute( cmdline );
	}
    if( ret!=0 && !mp.empty() && mp!="swap" )
	{
	cmdline = UMOUNTBIN " " + quote(mp);
	ret = cmd.execute( cmdline );
	}
    if( ret!=0 && !orig_mp.empty() && orig_mp!="swap" )
	{
	cmdline = UMOUNTBIN " " + quote(orig_mp);
	ret = cmd.execute( cmdline );
	}
    if( ret != 0 )
	ret = VOLUME_UMOUNT_FAILED;
    else
	is_mounted = false;
    y2mil("ret:" << ret);
    return( ret );
    }

int Volume::loUnsetup( bool force )
    {
    int ret=0;
    if( (is_loop && loop_active) || force )
	{
	if( !loop_dev.empty() )
	    {
	    SystemCmd c(LOSETUPBIN " -d " + quote(loop_dev));
	    if( c.retcode()!=0 )
		ret = VOLUME_LOUNSETUP_FAILED;
	    else
		loop_active = false;
	    }
	else
	    {
	    loop_active = false;
	    ret = VOLUME_LOUNSETUP_FAILED;
	    }
	}
    return( ret );
    }

int Volume::cryptUnsetup( bool force )
    {
    int ret=0;
    if( dmcrypt_active || force )
	{
	string table = dmcrypt_dev;
	if( table.find( '/' )!=string::npos )
	    table.erase( 0, table.find_last_of( '/' )+1 );
	if( !table.empty() )
	    {
	    SystemCmd c(CRYPTSETUPBIN " remove " + quote(table));
	    if( c.retcode()!=0 )
		ret = VOLUME_CRYPTUNSETUP_FAILED;
	    else
		dmcrypt_active = false;
	    }
	else
	    {
	    ret = VOLUME_CRYPTUNSETUP_FAILED;
	    dmcrypt_active = false;
	    }
	}
    return( ret );
    }

int Volume::crUnsetup( bool force )
    {
    int ret = cryptUnsetup( force );
    if( ret==0 || force )
	ret = loUnsetup( force );
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
	if( !orig_mp.empty() && !mp.empty() && 
	    (!cont->getStorage()->instsys()||mp!=orig_mp||mp!="swap") )
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
	    if( inCrypttab() )
		fn = "/etc/crypttab";
	    if( inCryptotab() )
		fn = "/etc/cryptotab";
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by pathname e.g. /etc/fstab
	    txt = sformat( _("Remove %1$s from %2$s"), d.c_str(), fn.c_str() );
	    }
        }
    return( txt );
    }


int Volume::checkDevice() const
    {
    return( checkDevice(dev));
    }

int Volume::checkDevice(const string& device) const
    {
    struct stat sbuf;
    int ret = 0;
    if( stat(device.c_str(), &sbuf)<0 )
	ret = VOLUME_DEVICE_NOT_PRESENT;
    else if( !S_ISBLK(sbuf.st_mode) )
	ret = VOLUME_DEVICE_NOT_BLOCK;
    y2mil("checkDevice:" << device << " ret:" << ret);
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
    y2mil("device:" << dev << " mp:" << mp << " old mp:" << orig_mp);
    if( !silent() )
	{
	cont->getStorage()->showInfoCb( mountText(true) );
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
    if( ret==0 && !mp.empty() && uby.type() != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    if( ret==0 && !mp.empty() && !cont->getStorage()->test() )
	{
	if( fs!=NFS )
	    {
	    cont->getStorage()->removeDmTableTo( *this );
	    ret = checkDevice(mountDevice());
	    }
	if( ret==0 )
	    {
	    ret = mount( lmount );
	    if( ret!=0 && cont->getStorage()->instsys() && fs==NTFS )
		ret = mount( lmount, true );
	    }
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Volume::canResize( unsigned long long newSizeK ) const
    {
    int ret=0;
    y2mil("val:" << newSizeK);
    if( uby.type() != UB_NONE )
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else
	{
	FsCapabilities caps;
	if( !format && fs!=FSNONE && !ignore_fs &&
	    (!cont->getStorage()->getFsCapabilities( fs, caps ) ||
	     (newSizeK < size_k && !caps.isReduceable) ||
	     (newSizeK > size_k && !caps.isExtendable)) )
	    {
	    ret = VOLUME_RESIZE_UNSUPPORTED_BY_FS;
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Volume::resizeFs()
    {
    SystemCmd c;
    string cmd;
    int ret = 0;
    if( encryption!=ENC_NONE && !dmcrypt_dev.empty() )
	{
	cmd = CRYPTSETUPBIN " resize ";
	cmd += dmcrypt_dev.substr(dmcrypt_dev.rfind( '/' )+1);
	c.execute( cmd );
	}
    if( !format && !ignore_fs )
	{
	switch( fs )
	    {
	    case SWAP:
		cmd = "/sbin/mkswap";
		if (!label.empty())
		    cmd += " -L " + quote(label);
		if (!uuid.empty())
		    cmd += " -U " + quote(uuid);
		cmd += " " + quote(mountDevice());	
		c.execute(cmd);
		if( c.retcode()!=0 )
		    ret = VOLUME_RESIZE_FAILED;
		break;
	    case REISERFS:
		cmd = "/sbin/resize_reiserfs -f ";
		if( needShrink() )
		    {
		    cmd = "echo y | " + cmd;
		    cmd += "-s " + decString(size_k) + "K ";
		    }
		cmd += quote(mountDevice());
		c.execute( cmd );
		if( c.retcode()!=0 )
		    {
		    ret = VOLUME_RESIZE_FAILED;
		    setExtError( c );
		    }
		break;
	    case NTFS:
		cmd = "echo y | /usr/sbin/ntfsresize -f ";
		if( needShrink() )
		    cmd += "-s " + decString(size_k) + "k ";
		cmd += quote(mountDevice());
		c.setCombine();
		c.execute( cmd );
		if( c.retcode()!=0 )
		    {
		    ret = VOLUME_RESIZE_FAILED;
		    setExtError( c, false );
		    }
		c.setCombine(false);
		break;
	    case EXT2:
	    case EXT3:
	    case EXT4:
		cmd = "/sbin/resize2fs -f " + quote(mountDevice());
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
		    cmd = "/usr/sbin/xfs_growfs " + quote(mpoint);
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
    ignore_fs = false;
    y2mil("ret:" << ret);
    return( ret );
    }

int Volume::setEncryption( bool val, EncryptType typ )
    {
    int ret = 0;
    y2mil("val:" << val << " typ:" << typ);
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
	    if( !loop_active && !isTmpCryptMp(mp) && crypt_pwd.empty() )
		ret = VOLUME_CRYPT_NO_PWD;
	    if( ret == 0 && cType()==NFSC )
		ret = VOLUME_CRYPT_NFS_IMPOSSIBLE;
	    if( ret==0 && (format||loop_active) )
		{
		encryption = typ;
		is_loop = cont->type()==LOOP;
		}
	    if( ret==0 && !format && !loop_active )
		{
		if( detectEncryption()==ENC_UNKNOWN )
		    ret = VOLUME_CRYPT_NOT_DETECTED;
		}
	    }
	}
    if( ret==0 )
	{
	updateFstabOptions();
	}
    y2mil("ret:" << ret);
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

string Volume::crsetupText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Setting up encrypted dm device on %1$s"), d.c_str() );
        }
    else
        {
	// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Set up encrypted dm device on %1$s"), d.c_str() );
        }
    return( txt );
    }

bool Volume::loopStringNum( const string& name, unsigned& num )
    {
    bool ret=false;
    string d = undevDevice(name);
    static Regex loop( "^loop[0123456789]+$" );
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

int Volume::getFreeLoop( SystemCmd& loopData, const list<unsigned>& ids )
    {
    int ret = 0;
    y2mil( "ids:" << ids );
    const int loop_instsys_offset = 2;
    list<unsigned> lnum;
    Storage::ConstVolPair p = cont->getStorage()->volPair( hasLoopDevice );
    for( Storage::ConstVolIterator i=p.begin(); i!=p.end(); ++i )
	{
	y2mil( "lvol:" << *i );
	unsigned num;
	if( loopStringNum( i->loopDevice(), num ))
	    lnum.push_back( num );
	}
    y2mil( "lnum:" << lnum );
    unsigned num = cont->getStorage()->instsys()?loop_instsys_offset:0;
    bool found;
    string ldev;
    do
	{
	ldev = "^/dev/loop" + decString(num) + ":";
	found = loopData.select( ldev )>0 ||
		find( lnum.begin(), lnum.end(), num )!=lnum.end() ||
		find( ids.begin(), ids.end(), num )!=ids.end();
	if( found )
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
    y2mil("loop_dev:" << loop_dev << " fstab_loop_dev:" << fstab_loop_dev);
    return( ret );
    }

int Volume::getFreeLoop( SystemCmd& loopData )
    {
    int ret = 0;
    if( loop_dev.empty() )
	{
	list<unsigned> ids;
	ret = getFreeLoop( loopData, ids );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Volume::getFreeLoop()
    {
    int ret = 0;
    if( loop_dev.empty() )
	{
	SystemCmd c(LOSETUPBIN " -a");
	ret = getFreeLoop( c );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

string Volume::getLosetupCmd( storage::EncryptType, const string& pwdfile ) const
    {
    string cmd = LOSETUPBIN " " + quote(loop_dev) + " ";
    const Loop* l = static_cast<const Loop*>(this);
    cmd += quote(l->lfileRealPath());
    y2mil("cmd:" << cmd);
    return( cmd );
    }

string Volume::getCryptsetupCmd( storage::EncryptType e, const string& dmdev,
				 const string& mount, const string& pwdf,
				 bool format, bool empty_pwd ) const
    {
    string table = dmdev;
    y2mil( "enctype:" << e << " dmdev:" << dmdev << " mount:" << mount <<
	   " format:" << format << " pwempty:" << empty_pwd );
    if( table.find( '/' )!=string::npos )
	table.erase( 0, table.find_last_of( '/' )+1 );
    string cmd = CRYPTSETUPBIN " -q";

    if( format )
    {
	switch( e )
	{
	    case ENC_LUKS:
		if( isTmpCryptMp(mount) && empty_pwd )
		{
		    cmd += " --key-file /dev/urandom create";
		    cmd += ' ';
		    cmd += quote(table);
		    cmd += ' ';
		    cmd += quote(is_loop?loop_dev:dev);
		}
		else
		{
		    cmd += " luksFormat";
		    cmd += ' ';
		    cmd += quote(is_loop?loop_dev:dev);
		    cmd += ' ';
		    cmd += pwdf;
		}
		break;

	    case ENC_TWOFISH:
	    case ENC_TWOFISH_OLD:
	    case ENC_TWOFISH256_OLD:
		cmd = "";
		break;

	    case ENC_NONE:
	    case ENC_UNKNOWN:
		cmd = "";
		break;
	}
    }
    else
    {
	switch( e )
	{
	    case ENC_LUKS:
		cmd += " --key-file " + pwdf;
		cmd += " luksOpen ";
		cmd += quote(is_loop?loop_dev:dev);
		cmd += ' ';
		cmd += quote(table);
		break;

	    case ENC_TWOFISH:
		cmd += " --hash sha512 --cipher twofish";
		cmd += " create ";
		cmd += quote(table);
		cmd += ' ';
		cmd += quote(is_loop?loop_dev:dev);
		cmd += " < " + pwdf;
		break;

	    case ENC_TWOFISH_OLD:
		cmd += " --hash ripemd160:20 --cipher twofish-cbc-null --key-size 192";
		cmd += " create ";
		cmd += quote(table);
		cmd += ' ';
		cmd += quote(is_loop?loop_dev:dev);
		cmd += " < " + pwdf;
		break;

	    case ENC_TWOFISH256_OLD:
		cmd += " --hash sha512 --cipher twofish-cbc-null --key-size 256";
		cmd += " create ";
		cmd += quote(table);
		cmd += ' ';
		cmd += quote(is_loop?loop_dev:dev);
		cmd += " < " + pwdf;
		break;

	    case ENC_NONE:
	    case ENC_UNKNOWN:
		cmd = "";
		break;
	}
    }

    y2mil("cmd:" << cmd);
    return( cmd );
    }

int
Volume::setCryptPwd( const string& val )
    {
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    y2mil("password:" << val);
#endif
    int ret = 0;

    if( ((encryption==ENC_UNKNOWN||encryption==ENC_TWOFISH_OLD||
          encryption==ENC_NONE) && val.size()<5) ||
        ((encryption==ENC_TWOFISH||encryption==ENC_TWOFISH256_OLD) &&
	 val.size()<8) ||
	(encryption==ENC_LUKS && val.size()<1))
	{
	if( !isTmpCryptMp(mp) )
	    ret = VOLUME_CRYPT_PWD_TOO_SHORT;
	}
    else
	{
	crypt_pwd=val;
	if( encryption==ENC_UNKNOWN )
	    detectEncryption();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

bool Volume::needLosetup() const
    {
    return( (is_loop!=loop_active) &&
            (encryption==ENC_NONE || !crypt_pwd.empty() ||
	     (dmcrypt()&&cont->type()==LOOP)) );
    }

bool Volume::needCryptsetup() const
    {
    if (dmcrypt() && encryption != orig_encryption)
	return true;

    return( dmcrypt()!=dmcrypt_active &&
            (encryption==ENC_NONE || !crypt_pwd.empty() || isTmpCryptMp(mp)));
    }

bool Volume::needCrsetup() const
    {
    return( needLosetup()||needCryptsetup() );
    }

bool Volume::needFstabUpdate() const
    {
    bool ret = !ignore_fstab && !(mp.empty() && orig_mp.empty()) &&
	       ((cont->getStorage()->instsys()&&mp=="swap"&&mp==orig_mp) ||
	        fstab_opt!=orig_fstab_opt || mount_by!=orig_mount_by ||
		encryption!=orig_encryption);
    return( ret );
    }

EncryptType Volume::detectEncryption()
    {
    EncryptType ret = ENC_UNKNOWN;

    if( getContainer()->getStorage()->test() )
	{
	ret = encryption = orig_encryption = ENC_TWOFISH;
	y2mil("ret:" << encTypeString(ret));
	return( ret );
	}

    unsigned pos=0;
    static EncryptType try_order[] = { ENC_LUKS, ENC_TWOFISH_OLD,
                                       ENC_TWOFISH256_OLD, ENC_TWOFISH };
    string fname = cont->getStorage()->tmpDir()+"/pwdf";
    string mpname = cont->getStorage()->tmpDir()+"/mp";
    SystemCmd c;
    y2mil("device:" << dev);

    mkdir( mpname.c_str(), 0700 );
    getFreeLoop();
    detected_fs = fs = FSUNKNOWN;
    do
	{
	ofstream pwdfile( fname.c_str() );
	classic(pwdfile);
	pwdfile << crypt_pwd;
	pwdfile.close();
	encryption = orig_encryption = try_order[pos];
	is_loop = cont->type()==LOOP;
	dmcrypt_dev = getDmcryptName();
	crUnsetup( true );
	if( is_loop )
	    {
	    string lfile;
	    if( getLoopFile( lfile ))
		c.execute(LOSETUPBIN " " + quote(loop_dev) + " " +
			  quote(cont->getStorage()->root() + lfile));
	    }
	string cmd = getCryptsetupCmd( try_order[pos], dmcrypt_dev, "", fname, false );
	c.execute(MODPROBEBIN " dm-crypt");
	c.execute( cmd );
        string use_dev = dmcrypt_dev;
	if( c.retcode()==0 )
	    {
	    Storage::waitForDevice(use_dev);
	    updateFsData();
	    if( detected_fs!=FSUNKNOWN )
		{
		string cmd;
		switch( detected_fs )
		    {
		    case EXT2:
			cmd = "/sbin/fsck.ext2 -n -f " + quote(use_dev);
			break;
		    case EXT3:
			cmd = "/sbin/fsck.ext3 -n -f " + quote(use_dev);
			break;
		    case EXT4:
			cmd = "/sbin/fsck.ext4 -n -f " + quote(use_dev);
			break;
		    case REISERFS:
			cmd = "reiserfsck --yes --check -q " + quote(use_dev);
			break;
		    default:
			cmd = "fsck -n -t " + fsTypeString(detected_fs) + " " + quote(use_dev);
			break;
		    }
		bool excTime, excLines;
		c.executeRestricted( cmd, 15, 500, excTime, excLines );
		bool ok = c.retcode()==0 || (excTime && !excLines);
		y2mil("ok:" << ok << " retcode:" << c.retcode() << " excTime:" << excTime <<
		      " excLines:" << excLines);
		if( ok )
		    {
		    c.execute(MODPROBEBIN " " + fs_names[detected_fs]);
		    c.execute(MOUNTBIN " -oro -t " + fsTypeString(detected_fs) + " " +
			      quote(use_dev) + " " + quote(mpname));
		    ok = c.retcode()==0;
		    c.execute(UMOUNTBIN " " + quote(mpname));
		    }
		if( !ok )
		    {
		    detected_fs = fs = FSUNKNOWN;
		    eraseLabel();
		    uuid.erase();
		    }
		}
	    }
	if( fs==FSUNKNOWN )
	    pos++;
	}
    while( detected_fs==FSUNKNOWN && pos<lengthof(try_order) );
    crUnsetup( true );
    if( detected_fs!=FSUNKNOWN )
	{
	is_loop = cont->type()==LOOP;
	ret = encryption = orig_encryption = try_order[pos];
	}
    else
	{
	is_loop = false;
	dmcrypt_dev.erase();
	loop_dev.erase();
	ret = encryption = orig_encryption = ENC_UNKNOWN;
	}
    unlink( fname.c_str() );
    rmdir( mpname.c_str() );
    rmdir( cont->getStorage()->tmpDir().c_str() );
    y2mil("ret:" << encTypeString(ret));
    return( ret );
    }

int Volume::doLosetup()
    {
    int ret = 0;
    y2mil("device:" << dev << " mp:" << mp << " is_loop:" << is_loop << " loop_active:" << loop_active);
    if( !silent() && is_loop && !dmcrypt() )
	{
	cont->getStorage()->showInfoCb( losetupText(true) );
	}
    if( is_mounted )
	{
	umount( orig_mp );
	}
    if( is_loop )
	{
	cont->getStorage()->removeDmTableTo( *this );
	if( ret==0 && loop_dev.empty() )
	    {
	    ret = getFreeLoop();
	    }
	if( ret==0 )
	    {
	    string fname;
	    if( !dmcrypt() )
		{
		fname = cont->getStorage()->tmpDir()+"/pwdf";
		ofstream pwdfile( fname.c_str() );
		classic(pwdfile);
		pwdfile << crypt_pwd << endl;
		pwdfile.close();
		}
	    SystemCmd c( getLosetupCmd( encryption, fname ));
	    if( c.retcode()!=0 )
		ret = VOLUME_LOSETUP_FAILED;
	    if( !fname.empty() )
		{
		unlink( fname.c_str() );
		rmdir( cont->getStorage()->tmpDir().c_str() );
		}
	    Storage::waitForDevice(loop_dev);
	    }
	if( ret==0 )
	    {
	    loop_active = true;
	    if( !dmcrypt() )
		{
		list<string> l = splitString( fstab_opt, "," );
		list<string>::iterator i = find( l.begin(), l.end(), "loop" );
		if( i == l.end() )
		    i = find_if( l.begin(), l.end(), string_starts_with( "loop=" ) );
		if( i!=l.end() )
		    *i = "loop=" + fstab_loop_dev;
		fstab_opt = boost::join( l, "," );
		}
	    }
	}
    else
	{
	if( loop_dev.size()>0 )
	    {
	    SystemCmd c(LOSETUPBIN " -d " + quote(loop_dev));
	    loop_dev.erase();
	    }
	updateFstabOptions();
	loop_active = false;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

string Volume::getDmcryptName() const
    {
    string nm;
    if( cont->type()!=LOOP )
	{
	nm = dev;
	}
    else
	{
	const Loop* l = static_cast<const Loop*>(this);
	nm = l->loopFile();
	}
    if( nm.find( '/' )!=string::npos )
	nm.erase( 0, nm.find_last_of( '/' )+1 );
    nm = "/dev/mapper/cr_" + nm;
    return( nm );
    }

void Volume::replaceAltName( const string& prefix, const string& newn )
    {
    y2mil( "prefix:" << prefix << " new:" << newn );
    list<string>::iterator i =
	find_if( alt_names.begin(), alt_names.end(), string_starts_with( prefix ) );
    if( i!=alt_names.end() )
	{
	if( !newn.empty() )
	    *i = newn;
	else
	    alt_names.erase(i);
	}
    else if( !newn.empty() )
	alt_names.push_back(newn);
    }


int Volume::doCryptsetup()
    {
    int ret = 0;
    y2mil("device:" << dev << " mp:" << mp << " dmcrypt:" << dmcrypt() << " active:" << dmcrypt_active);
    if( !silent() && dmcrypt() )
	{
	cont->getStorage()->showInfoCb( crsetupText(true) );
	}
    if( is_mounted )
	{
	umount( orig_mp );
	}
    if( dmcrypt() )
	{
	cont->getStorage()->removeDmTableTo( *this );
	if( ret==0 && dmcrypt_dev.empty() )
	    {
	    dmcrypt_dev = getDmcryptName();
	    }
	if( ret==0 )
	    {
	    ret = cryptUnsetup();
	    }
	if( ret==0 )
	    {
	    string fname = cont->getStorage()->tmpDir()+"/pwdf";
	    ofstream pwdfile( fname.c_str() );
	    classic(pwdfile);
	    pwdfile << crypt_pwd;
	    pwdfile.close();
	    SystemCmd cmd;
	    if( format || (isTmpCryptMp(mp)&&crypt_pwd.empty()) )
		{
		string cmdline = getCryptsetupCmd( encryption, dmcrypt_dev, mp, fname, true,
						   crypt_pwd.empty() );
		if( !cmdline.empty() )
		    {
		    cmd.execute( cmdline );
		    if( cmd.retcode()!=0 )
		    ret = VOLUME_CRYPTFORMAT_FAILED;
		if( ret==0 && mp=="swap" )
		    cmd.execute("/sbin/mkswap " + quote(dmcrypt_dev));
		}
		}
	    if( ret==0 && (!isTmpCryptMp(mp)||!crypt_pwd.empty()) )
		{
		string cmdline = getCryptsetupCmd( encryption, dmcrypt_dev, mp, fname, false );
		if( !cmdline.empty() )
		    {
		    cmd.execute( cmdline );
		    if( cmd.retcode()!=0 )
		    ret = VOLUME_CRYPTSETUP_FAILED;
		}
		}
	    unlink( fname.c_str() );
	    rmdir( cont->getStorage()->tmpDir().c_str() );
	    Storage::waitForDevice(dmcrypt_dev);
	    }
	if( ret==0 )
	    {
	    dmcrypt_active = true;
	    unsigned long dummy, minor;
	    if( cont->type()==LOOP )
		{
		getMajorMinor( dev, mjr, mnr );
		minor = mnr;
		replaceAltName( "/dev/dm-", Dm::dmDeviceName(mnr) );
		}
	    else
		getMajorMinor( dmcrypt_dev, dummy, minor );
	    ProcPart p;
	    unsigned long long sz;
	    if( p.getSize( Dm::dmDeviceName(minor), sz ))
		setSize( sz );
	    }
	}
    else
	{
	cryptUnsetup();
	updateFstabOptions();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Volume::doCrsetup()
    {
    int ret = 0;
    bool losetup_done = false;
    if( needLosetup() )
	{
	ret = doLosetup();
	losetup_done = ret==0;
	}
    if( ret==0 && needCryptsetup() )
	{
	ret = doCryptsetup();
	if( ret!=0 && losetup_done )
	    loUnsetup();
	}
    if( ret==0 )
	updateFsData();
    y2mil("ret:" << ret);
    return( ret );
    }

string Volume::labelText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
    {
	if( label.empty() )
	{
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Clearing label on %1$s"), d.c_str() );
	}
	else
	{
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by a name e.g. ROOT
	    txt = sformat( _("Setting label on %1$s to %2$s"), d.c_str(), label.c_str() );
	}
    }
    else
    {
	if( label.empty() )
	{
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Clear label on %1$s"), d.c_str() );
	}
	else
	{  
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by a name e.g. ROOT
	    txt = sformat( _("Set label on %1$s to %2$s"), d.c_str(), label.c_str() );
	}
    }
    return( txt );
    }

int Volume::doSetLabel()
    {
    int ret = 0;
    bool remount = false;
    FsCapabilities caps;
    y2mil("device:" << dev << " mp:" << mp << " label:" << label);
    if( !silent() )
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
	    case EXT4:
		cmd = "/sbin/tune2fs -L " + quote(label) + " " + quote(mountDevice());
		break;
	    case REISERFS:
		cmd = "/sbin/reiserfstune -l " + quote(label) + " " + quote(mountDevice());
		break;
	    case JFS:
		cmd = "/sbin/jfs_tune -L " + quote(label) + " " + quote(mountDevice());
		break;
	    case XFS:
		{
		string tlabel = label;
		if( label.empty() )
		    tlabel = "--";
		cmd = "/usr/sbin/xfs_admin -L " + quote(tlabel) + " " + quote(mountDevice());
		}
		break;
	    case SWAP:
		cmd = "/sbin/mkswap -L " + quote(label);
		if (!uuid.empty())
		    cmd += " -U " + quote(uuid);
		cmd += " " + quote(mountDevice());
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
    if( ret==0 )
	{
	triggerUdevUpdate();
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Volume::setLabel( const string& val )
    {
    int ret=0;
    y2mil("label:" << val);
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Volume::mount( const string& m, bool ro )
    {
    SystemCmd cmd;
    y2mil("device:" << dev << " mp:" << m << " ro:" << ro);
    string cmdline;
    if( fs != SWAP )
	{
	string lmount = (!m.empty())?m:mp;
	y2mil("device:" << dev << " mp:" << lmount);
	string fsn = fs_names[fs];
	switch( fs )
	    {
	    case NTFS:
		if( !ro )
		    fsn = "ntfs-3g";
		break;
	    case FSUNKNOWN:
		fsn = "auto";
		break;
	    default:
		cmdline = MODPROBEBIN " " + fsn;
		cmd.execute( cmdline );
		break;
	    }
	if( fs == VFAT )
	    {
	    cmdline = MODPROBEBIN " nls_cp437";
	    cmd.execute( cmdline );
	    cmdline = MODPROBEBIN " nls_iso8859-1";
	    cmd.execute( cmdline );
	    }
	cmdline = MOUNTBIN " ";
	if( ro )
	    cmdline += "-r ";
	const char * ign_opt[] = { "defaults", "" };
	const char * ign_beg[] = { "loop", "encryption=", "phash=",
	                           "itercountk=", "" };
	if( cont->getStorage()->instsys() )
	    ign_opt[lengthof(ign_opt)-1] = "ro";
	if( fsn=="ntfs" )
	    ign_beg[lengthof(ign_beg)-1] = "locale=";
	list<string> l = splitString( fstab_opt, "," );
	y2mil( "l before:" << l );
	for( unsigned i=0; i<lengthof(ign_opt) && *ign_opt[i]!=0; i++ )
	    l.remove(ign_opt[i]);
	for( unsigned i=0; i<lengthof(ign_beg) && *ign_beg[i]!=0; i++ )
	    l.remove_if(string_starts_with(ign_beg[i]));
	y2mil( "l  after:" << l );
	string opts = " ";
	if( !l.empty() )
	    {
	    opts += "-o";
	    opts += boost::join( l, "," );
	    opts += " ";
	    }
	cmdline += "-t " + fsn + opts + quote(mountDevice()) + " " + quote(lmount);
	}
    else
	{
	cmdline = SWAPONBIN " " + quote(mountDevice());
	if( cont->getStorage()->instsys() )
	    {
	    ProcMounts mountData( cont->getStorage() );
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Volume::prepareRemove()
    {
    int ret = 0;
    y2mil("device:" << dev);
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
    if( loop_active || dmcrypt_active )
	{
	crUnsetup();
	}
    cont->getStorage()->eraseCachedFreeInfo(dev);
    cont->getStorage()->removeDmTableTo(*this);
    y2mil("ret:" << ret);
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
    else if( mby==MOUNTBY_ID )
	{
	if( !udevId().empty() )
	    ret = udevId().front();
	}
    else if( mby==MOUNTBY_PATH )
	{
	ret = udevPath();
	}
    return( ret );
    }


void
Volume::getCommitActions(list<commitAction>& l) const
    {
    if( deleted() )
	{
	l.push_back(commitAction(DECREASE, cont->type(), removeText(false), this, true));
	}
    else if( needShrink() )
	{
	l.push_back(commitAction(DECREASE, cont->type(), resizeText(false), this, true));
	}
    else if( created() )
	{
	l.push_back(commitAction(INCREASE, cont->type(), createText(false), this, false));
	}
    else if( needExtend() )
	{
	l.push_back(commitAction(INCREASE, cont->type(), resizeText(false), this, true));
	}
    else if( format )
	{
	l.push_back(commitAction(FORMAT, cont->type(), formatText(false), this, true));
	}
    else if( mp != orig_mp || 
             (cont->getStorage()->instsys()&&mp=="swap") )
	{
	l.push_back(commitAction(MOUNT, cont->type(), mountText(false), this, false));
	}
    else if( label != orig_label )
	{
	l.push_back(commitAction(MOUNT, cont->type(), labelText(false), this, false));
	}
    else if( needFstabUpdate() )
	{
	l.push_back(commitAction(MOUNT, cont->type(), fstabUpdateText(), this, false));
	}
    }


string Volume::fstabUpdateText() const
    {
    string txt;
    const EtcFstab* fstab = cont->getStorage()->getFstab();
    if( !orig_mp.empty() && mp.empty() )
	txt = fstab->removeText( false, inCryptotab(), orig_mp );
    else
	txt = fstab->updateText( false, inCryptotab(), orig_mp );
    return( txt );
    }

string Volume::getFstabDevice() const
    {
    string ret = dev;
    const Loop* l = NULL;
    if( cont->type()==LOOP )
	l = static_cast<const Loop*>(this);
    if( l && dmcrypt() && !optNoauto() )
	ret = l->loopFile();
    y2mil( "ret:" << ret );
    return( ret );
    }

string Volume::getFstabDentry() const
    {
    string ret;
    const Loop* l = NULL;
    if( cont->type()==LOOP )
	l = static_cast<const Loop*>(this);
    if( cont->type()!=LOOP )
	{
	if( dmcrypt() )
	    ret = (inCryptotab()||optNoauto())?dev:dmcrypt_dev;
	else
	    ret = getMountByString( mount_by, dev, uuid, label );
	}
    else
	{
	if( dmcrypt() && !optNoauto() )
	    ret = dmcrypt_dev;
	else
	    ret = l->loopFile();
	}
    return( ret );
    }

void Volume::getFstabOpts(list<string>& l) const
    {
    if( fstab_opt.empty() )
	{
	l.clear();
	l.push_back( (is_loop&&!dmcrypt())?"noatime":"defaults" );
	}
    else
	l = splitString( fstab_opt, "," );
    list<string>::iterator loop = find( l.begin(), l.end(), "loop" );
    if( loop == l.end() )
	loop = find_if( l.begin(), l.end(), string_starts_with( "loop=" ) );
    list<string>::iterator enc =
	find_if( l.begin(), l.end(), string_starts_with( "encryption=" ) );
    string lstr;
    if( optNoauto() &&
        ((encryption!=ENC_NONE && !dmcrypt()) || cont->type()==LOOP ))
	{
	lstr = "loop";
	if(  !fstab_loop_dev.empty() && !dmcrypt() )
	    lstr += "="+fstab_loop_dev;
	}
    string estr;
    if( optNoauto() && encryption!=ENC_NONE && !dmcrypt() )
	estr = "encryption=" + Volume::encTypeString(encryption);
    if( loop!=l.end() )
	l.erase( loop );
    if( enc!=l.end() )
	l.erase( enc );
    if( !estr.empty() )
	l.push_front( estr );
    if( !lstr.empty() )
	l.push_front( lstr );
    if( l.size()>1 && (enc=find( l.begin(), l.end(), "defaults" ))!=l.end() )
	l.erase(enc);
    }

bool Volume::getLoopFile( string& fname ) const
    {
    const Loop* l = NULL;
    if( cont->type()==LOOP )
	l = static_cast<const Loop*>(this);
    if( l )
	fname = l->loopFile();
    return( l!=NULL );
    }

static bool haveQuota( const string& fstopt )
    {
    bool ret = false;
    list<string> opt = splitString( fstopt, "," );
    list<string>::const_iterator i = opt.begin();
    while( !ret && i!=opt.end() )
	{
	ret = (*i=="usrquota") || (*i=="usrquota") ||
	      i->find("usrjquota=")==0 || i->find("grpjquota=")==0;
	++i;
	}
    y2mil( "fstopt:" << fstopt << " ret:" << ret );
    return( ret );
    }

int Volume::doFstabUpdate()
    {
    int ret = 0;
    bool changed = false;
    y2mil( "dev:" << *this );
    if( !ignore_fstab )
	{
	EtcFstab* fstab = cont->getStorage()->getFstab();
	FstabEntry entry;
	if( !orig_mp.empty() && (deleted()||mp.empty()) &&
	    (fstab->findDevice( dev, entry ) ||
	     fstab->findDevice( alt_names, entry ) ||
	     (cType()==LOOP && fstab->findMount( orig_mp, entry )) ||
	     (cType()==LOOP && fstab->findMount( mp, entry )) ))
	    {
	    changed = true;
	    if( !silent() )
		{
		cont->getStorage()->showInfoCb(
		    fstab->removeText( true, entry.crypto, entry.mount ));
		}
	    y2mil("before removeEntry");
	    ret = fstab->removeEntry( entry );
	    }
	else if( !mp.empty() && !deleted() )
	    {
	    string fname;
	    if( fstab->findDevice( dev, entry ) ||
		fstab->findDevice( alt_names, entry ) ||
		(cont->type()==LOOP && getLoopFile(fname) &&
		     fstab->findDevice( fname, entry )))
		{
		y2mil( "changed:" << entry );    
		FstabChange che( entry );
		string de = getFstabDentry();
		if( orig_mp!=mp )
		    {
		    changed = true;
		    che.mount = mp;
		    }
		if( fstab_opt!=orig_fstab_opt )
		    {
		    changed = true;
		    getFstabOpts( che.opts );
		    if( encryption!=ENC_NONE )
			che.dentry = de;
		    }
		if( mount_by!=orig_mount_by ||
		    (format && mount_by==MOUNTBY_UUID) ||
		    (orig_label!=label && mount_by==MOUNTBY_LABEL) )
		    {
		    changed = true;
		    che.dentry = de;
		    }
		if( fs != detected_fs )
		    {
		    changed = true;
		    che.fs = fs_names[fs];
		    if( fs==SWAP || fs==NFS || encryption!=ENC_NONE )
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
		    if( inCryptotab() )
			{
			getFreeLoop();
			che.loop_dev = fstab_loop_dev;
			}
		    che.dentry = de;
		    if( encryption!=ENC_NONE )
			che.freq = che.passno = 0;
		    else
			{
			che.freq = 1;
			che.passno = (mp=="/") ? 1 : 2;
			}
		    }
		if( changed )
		    {
		    if( !silent() && !fstab_added )
			{
			cont->getStorage()->showInfoCb(
			    fstab->updateText( true, inCryptotab(),
			                       che.mount ));
			}
		    y2mil( "update fstab: " << che );
		    ret = fstab->updateEntry( che );
		    }
		}
	    else
		{
		changed = true;
		FstabChange che;
		che.device = getFstabDevice();
		if( !udevId().empty() )
		    che.device = udevId().front();
		che.dentry = getFstabDentry();
		che.encr = encryption;
		if( dmcrypt() && isTmpCryptMp(mp) && crypt_pwd.empty() )
		    che.tmpcrypt = true;
		if( inCryptotab() )
		    {
		    getFreeLoop();
		    che.loop_dev = fstab_loop_dev;
		    }
		che.fs = fs_names[fs];
		getFstabOpts( che.opts );
		che.mount = mp;
		if( fs != NFS && fs != SWAP && fs != FSUNKNOWN && fs != NTFS &&
		    fs != VFAT && !is_loop && !dmcrypt() && !optNoauto() )
		    {
		    che.freq = 1;
		    che.passno = (mp=="/") ? 1 : 2;
		    }
		if( !silent() )
		    {
		    cont->getStorage()->showInfoCb(
			fstab->addText( true, inCryptotab(), che.mount ));
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
    if( ret==0 && !format && !cont->getStorage()->instsys() &&
        fstab_opt!=orig_fstab_opt && !orig_fstab_opt.empty() &&
        mp==orig_mp && mp!="swap" )
	{
	SystemCmd c;
	y2mil( "fstab_opt:" << fstab_opt << " fstab_opt_orig:" << orig_fstab_opt );
	y2mil( "remount:" << *this );
	int r = umount( mp );
	y2mil( "remount umount:" << r );
	if( r==0 )
	    {
	    ret = mount(cont->getStorage()->prependRoot(mp));
	    y2mil( "remount mount:" << ret );
	    }
	else
	    {
	    c.execute(MOUNTBIN " -oremount " + quote(mp));
	    y2mil( "remount remount:" << c.retcode() );
	    if( c.retcode()!=0 )
		ret = VOLUME_REMOUNT_FAILED;
	    }
	if( !cont->getStorage()->instsys() &&
	    haveQuota(fstab_opt)!=haveQuota(orig_fstab_opt) )
	    {
	    c.execute( "/etc/init.d/boot.quota restart" );
	    }
	}
    y2mil("changed:" << changed << " ret:" << ret);
    return( ret );
    }

void Volume::fstabUpdateDone()
    {
    y2mil("begin");
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
    FsType ret = FSNONE;
    while( ret!=FSUNKNOWN && val!=fs_names[ret] )
	{
	ret = FsType(ret-1);
	}
    return( ret );
    }

MountByType Volume::toMountByType( const string& val )
    {
    MountByType ret = MOUNTBY_LABEL;
    while( ret!=MOUNTBY_DEVICE && val!=mb_names[ret] )
	{
	ret = MountByType(ret-1);
	}
    return( ret );
    }

string Volume::sizeString() const
{
    return byteToHumanString(1024 * size_k, false, 2, false);
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
    if( !need && !mp.empty() && !isMounted() && !optNoauto() &&
        is_loop==loop_active )
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
	// text displayed during action
	txt += string(" ") + _("(progress bar might not move)");
        }
    else
        {
	if( needShrink() )
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrink %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	else
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
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
    info.usedByType = uby.type();
    info.usedByName = uby.name();
    info.usedByDevice = uby.device();
    info.fstab_options = fstab_opt;
    info.uuid = uuid;
    info.label = label;
    info.encryption = encryption;
    info.crypt_pwd = crypt_pwd;
    info.fs = fs;
    info.format = format;
    info.create = create;
    info.mkfs_options = mkfs_opt;
    info.tunefs_options = tunefs_opt;
    info.dtxt = dtxt;
    info.loop = loop_dev;
    info.is_mounted = is_mounted;
    info.ignore_fs = ignore_fs;
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
    info.fstab_options = boost::join( fste.opts, "," );
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
	file << " pwd:" << crypt_pwd;
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
	{
	if( v.num>0 )
	    s << " Nr:" << v.num;
	}
    else if( v.nm!=v.dev )
	s << " Name:" << v.nm;
    s << " SizeK:" << v.size_k;
    if( v.size_k != v.orig_size_k )
	s << " orig_SizeK:" << v.orig_size_k;
    if( v.mjr!=0 || v.mnr!=0 )
	s << " Node <" << v.mjr << ":" << v.mnr << ">";
    if( v.ronly )
	s << " readonly";
    if( v.del )
	s << " deleted";
    if( v.create )
	s << " created";
    if( v.format )
	s << " format";
    if( v.silnt )
	s << " silent";
    if( v.ignore_fstab )
	s << " ignoreFstab";
    if( v.ignore_fs )
	s << " ignoreFs";
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
	}
    if( v.label != v.orig_label && v.orig_label.length()>0 )
	s << " orig_label:" << v.orig_label;
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
    if( v.tunefs_opt.length()>0 )
	{
	s << " tunefsopt:" << v.tunefs_opt;
	}
    if( v.dtxt.length()>0 )
	{
	s << " dtxt:" << v.dtxt;
	}
    if( v.alt_names.begin() != v.alt_names.end() )
	{
	s << " alt_names:" << v.alt_names;
	}
    if( v.encryption != storage::ENC_NONE ||
        v.orig_encryption != storage::ENC_NONE )
	{
	s << " encr:" << v.enc_names[v.encryption];
	if( v.encryption != v.orig_encryption &&
	    v.orig_encryption!=storage::ENC_NONE )
	    s << " orig_encr:" << v.enc_names[v.orig_encryption];
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
	s << " pwd:" << v.crypt_pwd;
#endif
	}
    if( !v.dmcrypt_dev.empty() )
	s << " dmcrypt:" << v.dmcrypt_dev;
    if( v.dmcrypt_active )
	s << " active";
    if( v.is_loop )
	{
	s << " loop:" << v.loop_dev;
	if( v.loop_active )
	    s << " active";
	if( v.fstab_loop_dev != v.loop_dev )
	    {
	    s << " fstab_loop:" << v.fstab_loop_dev;
	    }
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
	classic(b);
	b << uby << "-->" << rhs.uby;
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
    if( tunefs_opt!=rhs.tunefs_opt )
	ret += " tunefsopt:" + tunefs_opt + "-->" + rhs.tunefs_opt;
    if( dtxt!=rhs.dtxt )
	ret += " dtxt:" + dtxt + "-->" + rhs.dtxt;
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
	    silnt==rhs.silnt && format==rhs.format &&
	    fstab_added==rhs.fstab_added &&
	    fs==rhs.fs && mount_by==rhs.mount_by &&
	    uuid==rhs.uuid && label==rhs.label && mp==rhs.mp &&
	    fstab_opt==rhs.fstab_opt && mkfs_opt==rhs.mkfs_opt &&
	    tunefs_opt==rhs.tunefs_opt && dtxt==rhs.dtxt &&
	    is_loop==rhs.is_loop && loop_active==rhs.loop_active &&
	    is_mounted==rhs.is_mounted && encryption==rhs.encryption &&
	    loop_dev==rhs.loop_dev && fstab_loop_dev==rhs.fstab_loop_dev &&
	    uby==rhs.uby );
    }

Volume& Volume::operator= ( const Volume& rhs )
    {
    y2deb("operator= from " << rhs.dev);
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
    silnt = rhs.silnt;
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
    tunefs_opt = rhs.tunefs_opt;
    dtxt = rhs.dtxt;
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
    y2deb("constructed vol by copy constructor from " << rhs.dev);
    *this = rhs;
    }

bool Volume::isTmpCryptMp( const string& mp )
    {
    string *end = tmp_mount + lengthof(tmp_mount);
    y2mil( "lengthof(tmp_mount):" << lengthof(tmp_mount) );
    y2mil( "find mp:" << mp << " is:" << (find( tmp_mount, end, mp )!=end) );
    return( find( tmp_mount, end, mp )!=end );
    }

string Volume::fs_names[] = { "unknown", "reiserfs", "ext2", "ext3", "ext4", "vfat",
                              "xfs", "jfs", "hfs", "ntfs", "swap", "hfsplus", "nfs",
			      "none" };

string Volume::mb_names[] = { "device", "uuid", "label", "id", "path" };

string Volume::enc_names[] = { "none", "twofish256", "twofish",
                               "twofishSL92", "luks", "unknown" };

string Volume::tmp_mount[] = { "swap", "/tmp", "/var/tmp" };

const string Volume::empty_string;
const list<string> Volume::empty_slist;


}
