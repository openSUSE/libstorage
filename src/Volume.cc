#include <sstream>

#include <features.h>
#include <sys/stat.h>
#include <ycp/y2log.h>


#include "y2storage/Volume.h"
#include "y2storage/Disk.h"
#include "y2storage/Storage.h"
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
    size_k = SizeK;
    init();
    y2milestone( "constructed volume %s on disk %s", (num>0)?dev.c_str():"",
                 cont->name().c_str() );
    }

Volume::Volume( const Container& c, const string& Name, unsigned long long SizeK ) : cont(&c)
    {
    numeric = false;
    nm = Name;
    size_k = SizeK;
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
    del = create = format = is_loop = silent = false;
    mp_from_fstab = false;
    detected_fs = fs = FSUNKNOWN;
    mount_by = orig_mount_by = MOUNTBY_DEVICE;
    setNameDev();
    mjr = mnr = 0;
    getMajorMinor( dev, mjr, mnr );
    if( !numeric )
	num = 0;
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
    string dev( device );
    if( dev.find( "/dev/" )!=0 )
	dev = "/dev/"+ dev;
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
    bool found = fstabData.findDevice( device(), entry );
    if( !found )
	{
	found = fstabData.findDevice( alt_names, entry );
	}
    if( !found && (uuid.size()>0||label.size()>0) )
	{
	found = fstabData.findUuidLabel( uuid, label, entry );
	fstabData.setDevice( entry, device() );
	}
    if( !found && mp.size()>0 )
	{
	found = fstabData.findMount( mp, entry );
	}
    if( found )
	{
	std::ostringstream b;
	b << "line[" << device() << "]=";
	mp_from_fstab = entry.noauto;
	b << "noauto:" << mp_from_fstab;
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
	y2milestone( "%s mounted on %s", device().c_str(), mp.c_str() );
	}
    orig_mp = mp;
    }

void Volume::getLoopData( SystemCmd& loopData )
    {
    bool found = loopData.select( " (" + device() + ")" )>0;
    if( !found )
	{
	list<string>::const_iterator an = alt_names.begin();
	while( !found && an!=alt_names.end() )
	    {
	    found = loopData.select( " (" + *an + ") " )>0;
	    ++an;
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
	    is_loop = true;
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
		    if( encr == "twofish256" )
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
	    b.str("");
	    if( m.find( "TYPE" )!=m.end() )
		{
		if( m["TYPE"] == "reiserfs" )
		    {
		    fs = REISERFS;
		    }
		else if( m["TYPE"] == "swap" )
		    {
		    fs = SWAP;
		    }
		else if( m["TYPE"] == "ext2" )
		    {
		    fs = (m["SEC_TYPE"]=="ext3")?EXT3:EXT2;
		    }
		else if( m["TYPE"] == "vfat" )
		    {
		    fs = VFAT;
		    }
		else if( m["TYPE"] == "ntfs" )
		    {
		    fs = NTFS;
		    }
		else if( m["TYPE"] == "jfs" )
		    {
		    fs = JFS;
		    }
		else if( m["TYPE"] == "xfs" )
		    {
		    fs = XFS;
		    }
		detected_fs = fs;
		b << "fs:" << fs_names[fs];
		}
	    if( m.find("UUID") != m.end() )
		{
		uuid = m["UUID"];
		b << " uuid:" << uuid;
		}
	    if( m.find("LABEL") != m.end() )
		{
		label = orig_label = m["LABEL"];
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
	fs = new_fs;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::changeMount( const string& m )
    {
    int ret = 0;
    y2milestone( "device:%s mount:%s", dev.c_str(), m.c_str() );
    mp = m;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::changeFstabOptions( const string& options )
    {
    int ret = 0;
    y2milestone( "device:%s options:%s", dev.c_str(), options.c_str() );
    if( mp.size()>0 )
	fstab_opt = options;
    else
	ret = VOLUME_FSTAB_EMPTY_MOUNT;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::doFormat()
    {
    int ret = 0;
    bool needMount = false;
    y2milestone( "device:%s", dev.c_str() );
    if( !silent )
	{
	cont->getStorage()->showInfoCb( formatText(true) );
	}
    if( isMounted() )
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
		list<string>::const_iterator i = l.begin();
		while( i!=l.end() && i->find( "-F" )!=0 )
		    ++i;
		if( i!=l.end() )
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
	    }
	delete p;
	}
    if( ret==0 )
	{
	ret = doFstabUpdate();
	}
    if( ret==0 )
	{
	format = false;
	detected_fs = fs;
	if( fs != SWAP )
	    {
	    SystemCmd Blkid( "/sbin/blkid -c /dev/null " + mountDevice() );
	    FsType old=fs;
	    getFsData( Blkid );
	    if( fs != old )
		ret = VOLUME_FORMAT_FS_UNDETECTED;
	    }
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
    string cmdline = "umount " + mountDevice();
    int ret = cmd.execute( cmdline );
    if( ret != 0 && mp.size()>0 )
	{
	cmdline = "umount " + mp;
	ret = cmd.execute( cmdline );
	}
    if( ret != 0 )
	ret = VOLUME_UMOUNT_FAILED;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string Volume::mountText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	// %2$s is replaced by mount point e.g. /home
        txt = sformat( _("Mounting %1$s to %2$s"), dev.c_str(), mp.c_str() );
        }
    else
        {
	string d = dev.substr( 5 );
	if( orig_mp.size()>0 && mp.size()>0 )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. hda1
	    // %2$s is replaced by mount point e.g. /home
	    txt = sformat( _("Change mount point of %1$s to %2$s"), d.c_str() );
	    }
	else if( mp.size()>0 )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. hda1
	    // %2$s is replaced by mount point e.g. /home
	    txt = sformat( _("Set mount point of %1$s to %2$s"), d.c_str() );
	    }
	else if( orig_mp.size()>0 )
	    {
	    if( encryption!=ENC_NONE || optNoauto() )
		{
		// displayed text before action, %1$s is replaced by device name e.g. hda1
		txt = sformat( _("Remove %1$s from /etc/fstab"), d.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. hda1
		txt = sformat( _("Remove %1$s from /etc/cryptotab"), d.c_str() );
		}
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
    if( orig_mp.size()>0 )
	{
	ret = umount( orig_mp );
	}
    if( access( lmount.c_str(), R_OK )!=0 )
	{
	createPath( lmount );
	}
    if( ret==0 && mp.size()>0 )
	{
	ret = mount();
	}
    if( ret==0 )
	{
	ret = doFstabUpdate();
	orig_mp = mp;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::doLosetup()
    {
    int ret = 0;
    y2milestone( "device:%s mp:%s",  dev.c_str(), mp.c_str() );
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Volume::doSetLabel()
    {
    int ret = 0;
    y2milestone( "device:%s mp:%s label:%s",  dev.c_str(), mp.c_str(), 
                 label.c_str() );
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

string Volume::getMountbyString( MountByType mby, const string& dev, 
				 const string& uuid, const string& label )
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
    else if( created() )
	{
	l.push_back( new commitAction( INCREASE, cont->type(),
				       createText(false), false ));
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
	ret = fstab->removeEntry( entry );
	}
    else if( mp.size()>0 )
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
		che.dentry = getMountbyString( mount_by, dev, uuid, label );
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
		che.loop_dev = (fstab_loop_dev.size()>0)?fstab_loop_dev:
		                                           loop_dev;
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
	    che.dentry = getMountbyString( mount_by, dev, uuid, label );
	    che.encr = encryption;
	    che.loop_dev = (fstab_loop_dev.size()>0)?fstab_loop_dev:
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
    if( changed && ret==0 )
	{
	ret = fstab->flush();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
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

string Volume::sizeString() const
    {
    return( kbyteToHumanString( size_k ));
    }

string Volume::bootMount() const
    {
    if( Storage::arch()=="ia64" )
	return( "/boot/efi" );
    else
	return( "/boot" );
    }

bool Volume::optNoauto() const
    {
    list<string> l = splitString( fstab_opt );
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




string Volume::fs_names[] = { "unknown", "reiserfs", "ext2", "ext3", "vfat",
                              "xfs", "jfs", "ntfs", "swap" };

string Volume::mb_names[] = { "device", "uuid", "label" };

string Volume::enc_names[] = { "none", "twofish256", "twofish", 
                               "twofishSL92", "unknown" };


