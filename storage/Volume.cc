/*
 * Copyright (c) [2004-2010] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#include <features.h>
#include <sys/stat.h>
#include <sstream>
#include <fstream>

#include "storage/Volume.h"
#include "storage/Disk.h"
#include "storage/Storage.h"
#include "storage/StorageTypes.h"
#include "storage/Container.h"
#include "storage/AppUtil.h"
#include "storage/Blkid.h"
#include "storage/SystemCmd.h"
#include "storage/ProcMounts.h"
#include "storage/ProcParts.h"
#include "storage/OutputProcessor.h"
#include "storage/EtcFstab.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    Volume::Volume(const Container& c, unsigned PNr, unsigned long long SizeK)
	: Device("", ""), cont(&c), numeric(true), num(PNr)
    {
	size_k = orig_size_k = SizeK;
	init();
	y2deb("constructed Volume " << ((num>0)?dev:"") << " on " << cont->device());
    }


    Volume::Volume(const Container& c, const string& name, const string& device)
	: Device(name, device), cont(&c), numeric(false), format(false),
	  fstab_added(false), fs(FSUNKNOWN), detected_fs(FSUNKNOWN),
	  mount_by(MOUNTBY_DEVICE), orig_mount_by(MOUNTBY_DEVICE),
	  is_loop(false), is_mounted(false), ignore_fstab(false),
	  ignore_fs(false), loop_active(false), dmcrypt_active(false),
	  ronly(false), encryption(ENC_NONE), orig_encryption(ENC_NONE),
	  num(0), orig_size_k(0)
    {
	y2deb("constructed Volume " << dev << " on " << cont->device());

	mount_by = orig_mount_by = defaultMountBy();

	assert(!nm.empty() && !dev.empty());
    }


    Volume::Volume(const Container& c, const string& name, const string& device,
		   SystemInfo& systeminfo)
	: Device(name, device, systeminfo), cont(&c), numeric(false),
	  format(false), fstab_added(false), fs(FSUNKNOWN),
	  detected_fs(FSUNKNOWN), mount_by(MOUNTBY_DEVICE),
	  orig_mount_by(MOUNTBY_DEVICE), is_loop(false), is_mounted(false),
	  ignore_fstab(false), ignore_fs(false), loop_active(false),
	  dmcrypt_active(false), ronly(false), encryption(ENC_NONE),
	  orig_encryption(ENC_NONE), num(0), orig_size_k(0)
    {
	y2deb("constructed Volume " << dev << " on " << cont->device());

	mount_by = orig_mount_by = defaultMountBy();

	assert(!nm.empty() && !dev.empty());
	assert(!getStorage()->testmode());
    }


    /* This is our constructor for Volume used during fake detection in
       testmode. This is recognisable by the fact that it has an parameter of
       type xmlNode. */
    Volume::Volume(const Container& c, const xmlNode* node)
	: Device(node), cont(&c), numeric(false), format(false),
	  fstab_added(false), fs(FSUNKNOWN), detected_fs(FSUNKNOWN),
	  mount_by(MOUNTBY_DEVICE), orig_mount_by(MOUNTBY_DEVICE),
	  is_loop(false), is_mounted(false), ignore_fstab(false),
	  ignore_fs(false), loop_active(false), dmcrypt_active(false),
	  ronly(false), encryption(ENC_NONE), orig_encryption(ENC_NONE),
	  num(0), orig_size_k(0)
    {
	string tmp;

	getChildValue(node, "numeric", numeric);
	getChildValue(node, "number", num);

	if (getChildValue(node, "fs_type", tmp))
	    fs = detected_fs = toValueWithFallback(tmp, FSUNKNOWN);
	if (getChildValue(node, "fs_uuid", uuid))
	    orig_uuid = uuid;
	if (getChildValue(node, "fs_label", label))
	    orig_label = label;
	if (getChildValue(node, "mount", mp))
	    orig_mp = mp;
	if (getChildValue(node, "mount_by", tmp))
	    mount_by = orig_mount_by = toValueWithFallback(tmp, MOUNTBY_DEVICE);
	if (getChildValue(node, "fstopt", fstab_opt))
	    orig_fstab_opt = fstab_opt;

	if (getChildValue(node, "encryption", tmp))
	    encryption = orig_encryption = toValueWithFallback(tmp, ENC_UNKNOWN);
	if (getChildValue(node, "password", crypt_pwd))
	    orig_crypt_pwd = crypt_pwd;

	orig_size_k = size_k;

	y2deb("constructed Volume " << dev);

	assert(getStorage()->testmode());
    }


    /* This is our copy-constructor for Volumes. Every class derived from
       Volume needs an equivalent one. It takes a Container as extra argument
       since the newly created Volume can belong to a different Container than
       the original Volume. */
    Volume::Volume(const Container&c, const Volume&v)
	: Device(v), cont(&c), numeric(v.numeric), format(v.format),
	  fstab_added(v.fstab_added), fs(v.fs), detected_fs(v.detected_fs),
	  mount_by(v.mount_by), orig_mount_by(v.orig_mount_by), uuid(v.uuid),
	  orig_uuid(v.orig_uuid),
	  label(v.label), orig_label(v.orig_label), mp(v.mp),
	  orig_mp(v.orig_mp), fstab_opt(v.fstab_opt),
	  orig_fstab_opt(v.orig_fstab_opt), mkfs_opt(v.mkfs_opt),
	  tunefs_opt(v.tunefs_opt), is_loop(v.is_loop),
	  is_mounted(v.is_mounted), ignore_fstab(v.ignore_fstab),
	  ignore_fs(v.ignore_fs), loop_active(v.loop_active),
	  dmcrypt_active(v.dmcrypt_active), ronly(v.ronly),
	  encryption(v.encryption), orig_encryption(v.orig_encryption),
	  loop_dev(v.loop_dev), dmcrypt_dev(v.dmcrypt_dev),
	  fstab_loop_dev(v.fstab_loop_dev), 
	  crypt_pwd(v.crypt_pwd), orig_crypt_pwd(v.orig_crypt_pwd),
	  num(v.num), orig_size_k(v.orig_size_k), dtxt(v.dtxt)
    {
	y2deb("copy-constructed Volume " << dev);
    }


    Volume::~Volume()
    {
	y2deb("destructed Volume " << dev);
    }


    void
    Volume::saveData(xmlNode* node) const
    {
	Device::saveData(node);

	setChildValue(node, "numeric", numeric);
	if (numeric)
	    setChildValue(node, "number", num);

	if (fs != FSUNKNOWN)
	{
	    setChildValue(node, "fs_type", toString(fs));
	    if (!uuid.empty())
		setChildValue(node, "fs_uuid", uuid);
	    if (!label.empty())
		setChildValue(node, "fs_label", label);
	    if (!mp.empty())
		setChildValue(node, "mount", mp);
	    if (mount_by != MOUNTBY_DEVICE)
		setChildValue(node, "mount_by", toString(mount_by));
	    if (!fstab_opt.empty())
		setChildValue(node, "fstopt", fstab_opt);
	}

	if (encryption != ENC_NONE)
	    setChildValue(node, "encryption", toString(encryption));
#ifdef DEBUG_CRYPT_PASSWORD
	if (encryption != ENC_NONE && !crypt_pwd.empty())
	    setChildValue(node, "password", crypt_pwd);
#endif
    }


    Storage*
    Volume::getStorage() const
    {
	return cont->getStorage(); 
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
    y2mil("dev:" << dev << " dm:" << dm << " enc_type:" << toString(typ) << " active:" << active);
    dmcrypt_dev = dm;
    encryption = orig_encryption = typ;
    dmcrypt_active = active;
    y2mil( "this:" << *this );
    }

const string& Volume::mountDevice() const
    {
    if( dmcrypt() && !dmcrypt_dev.empty() )
	return( dmcrypt_dev );
    else
	return( is_loop?loop_dev:dev );
    }


MountByType
Volume::defaultMountBy() const
{
    MountByType mby = getStorage()->getDefaultMountBy();

    if (!allowedMountBy(mby))
	mby = MOUNTBY_DEVICE;

    y2mil("dev:" << dev << " ctype:" << toString(cType()) << " ret:" << toString(mby));
    return mby;
}


bool 
Volume::allowedMountBy(MountByType mby) const
{
    bool ret = true;

    switch (mby)
    {
	case MOUNTBY_PATH:
	    if (cType() != DISK)
		ret = false;
	    if (udevPath().empty())
		ret = false;
	    break;

	case MOUNTBY_ID:
	    if (cType() != DISK && cType() != DMRAID && cType() != DMMULTIPATH && cType() != MD && cType() != MDPART)
		ret = false;
	    if (udevId().empty() && cType() != MD)
		ret = false;
	    break;

	case MOUNTBY_UUID:
	case MOUNTBY_LABEL:
	    if (cType() == NFSC)
		ret = false;
	    if (encryption != ENC_NONE)
		ret = false;
	    break;

	case MOUNTBY_DEVICE:
	    break;
    }

    y2mil("dev:" << dev << " ctype:" << toString(cType()) << " mby:" << toString(mby) <<
	  " ret:" << ret);
    return ret;
}


void Volume::init()
    {
    format = is_loop = loop_active = false;
    is_mounted = ronly = fstab_added = ignore_fstab = ignore_fs = false;
    dmcrypt_active = false;
    detected_fs = fs = FSUNKNOWN;
    encryption = orig_encryption = ENC_NONE;
    if( numeric||!nm.empty() )
	{
	setNameDev();
	if (!getStorage()->testmode() && cType()!=NFSC)
	    getMajorMinor();
	}
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


void Volume::getFsInfo( const Volume* source )
    {
    setFs( source->getFs() );
    setFormat( source->getFormat(), source->getFs() );
    initUuid( source->getUuid() );
    initLabel( source->getLabel() );
    }

void Volume::getFstabData( EtcFstab& fstabData )
    {
    FstabEntry entry;
    bool found = false;
    if( cType()==LOOP )
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
    if (!found && !mp.empty())
	{
	setIgnoreFstab(true);
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
	    b << " mountby:" << toString(mount_by);
	    }
	fstab_opt = orig_fstab_opt = boost::join( entry.opts, "," );
	b << " fstopt:" << fstab_opt;
	if( !is_loop && entry.loop )
	    {
	    is_loop = true;
	    orig_encryption = encryption = entry.encr;
	    loop_dev = fstab_loop_dev = entry.loop_dev;
	    b << " loop_dev:" << loop_dev << " encr:" << toString(encryption);
	    }
	y2mil(b.str());
	}
    }

void Volume::getMountData( const ProcMounts& mounts, bool swap_only )
    {
    if( fs != TMPFS )
	{
	y2mil( "this:" << *this );
	y2mil( "swap_only:" << swap_only << " mountDevice:" << mountDevice() );
	mp = mounts.getMount(mountDevice());
	if( mp.empty() )
	    {
	    mp = mounts.getMount(alt_names);
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
    }

void Volume::getLoopData( SystemCmd& loopData )
    {
    bool found = false;
    if( cType()==LOOP )
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
	    b << " encr:" << toString(encryption);
	    y2mil(b.str());
	    }
	}
    }


bool 
Volume::findBlkid( const Blkid& blkid, Blkid::Entry& entry )
    {
    bool found = blkid.getEntry(mountDevice(), entry);
    if (!found && !is_loop)
	{
	list<string>::const_iterator an = alt_names.begin();
	while (!found && an != alt_names.end())
	    {
	    found = blkid.getEntry(*an, entry);
	    ++an;
	    }
	}
    return( found );
    }


    void
    Volume::getFsData(const Blkid& blkid, bool setUsedByLvm )
    {
	Blkid::Entry entry;

	if( findBlkid( blkid, entry ))
	{
	    y2mil("device:" << device() << " mountDevice:" << mountDevice() << " entry:" << entry);

	    if (entry.is_fs)
	    {
		detected_fs = fs = entry.fs_type;

		if (!entry.fs_uuid.empty())
		{
		    uuid = entry.fs_uuid;
		    alt_names.remove_if(string_contains("/by-uuid/"));
		    alt_names.push_back("/dev/disk/by-uuid/" + uuid);
		}

		if (!entry.fs_label.empty())
		{
		    // ignore label for hfs since we cannot set it (bnc #447782)
		    if (entry.fs_type != HFS)
			label = orig_label = entry.fs_label;
		    alt_names.remove_if(string_contains("/by-label/"));
		    if (!label.empty())
			alt_names.push_back("/dev/disk/by-label/" + udevEncode(label));
		}
	    }
	    if (setUsedByLvm && entry.is_lvm)
	    {
		setUsedBy(UB_LVM,"");

	    }
	}
    }


int Volume::setFormat( bool val, storage::FsType new_fs )
    {
    int ret = 0;
    y2mil("device:" << dev << " val:" << val << " fs:" << toString(new_fs));
    format = val;
    if( !format )
	{
	fs = detected_fs;
	mkfs_opt = "";
	tunefs_opt = "";
	if( label.empty() && !orig_label.empty() )
	    label = orig_label;
	if( uuid.empty() && !orig_uuid.empty() )
	    uuid = orig_uuid;
	}
    else
	{
	FsCapabilities caps;
	if (isUsedBy())
	    {
	    ret = VOLUME_ALREADY_IN_USE;
	    }
	else if (getStorage()->getFsCapabilities(new_fs, caps) && caps.minimalFsSizeK > size_k)
	    {
	    ret = VOLUME_FORMAT_FS_TOO_SMALL;
	    }
	else if( new_fs == NFS || new_fs == NFS4 || new_fs == TMPFS )
	    {
	    ret = VOLUME_FORMAT_IMPOSSIBLE;
	    }
	else
	    {
	    fs = new_fs;
	    FsCapabilities caps;
	    if (!getStorage()->getFsCapabilities(fs, caps) || !caps.supportsLabel)
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
    else if (isUsedBy())
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else
	{
	mp = m;
	if( m.empty() )
	    {
	    orig_fstab_opt = fstab_opt = "";
	    orig_mount_by = mount_by = defaultMountBy();
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
    y2mil("device:" << dev << " mby:" << toString(mby));
    y2mil( "vorher:" << *this );
    if (isUsedBy())
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
	    else if( !getStorage()->getFsCapabilities( fs, caps ) ||
	             (mby==MOUNTBY_LABEL && !caps.supportsLabel) ||
	             (mby==MOUNTBY_UUID && !caps.supportsUuid))
		{
		ret = VOLUME_MOUNTBY_UNSUPPORTED_BY_FS;
		}
	    }
	else if (!allowedMountBy(mby))
	    {
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


    void
    Volume::updateFstabOptions()
    {
	list<string> l = getFstabOpts();
	fstab_opt = boost::join(l, ",");
    }


int Volume::changeFstabOptions( const string& options )
    {
    int ret = 0;
    y2mil("device:" << dev << " options:" << options << " encr:" << toString(encryption));
    if (isUsedBy())
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

Text Volume::formatText( bool doing ) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat(_("Formatting device %1$s (%2$s) with %3$s"), dev.c_str(),
		      sizeString().c_str(), fsTypeString().c_str());
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
		txt = sformat(_("Format device %1$s (%2$s) for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str());
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat(_("Format encrypted device %1$s (%2$s) for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str());
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat(_("Format device %1$s (%2$s) with %3$s"),
			  dev.c_str(), sizeString().c_str(), fsTypeString().c_str());
	    }
	}
    return( txt );
    }

static string handle_O_Features( const string& opts )
    {
    string ret = opts;
    const string ofb = "-O ";
    const string of = "-O";
    string::size_type pos;
    if( (pos=ret.find( ofb ))!=string::npos && 
        ret.find( ofb, pos+ofb.size())!=string::npos )
	{
	list<string> ls = splitString( ret, " " );
	list<string>::iterator li = find( ls.begin(), ls.end(), of );
	if( li != ls.end() )
	    li++;
	list<string>::iterator ofi = li;
	while( li != ls.end() )
	    {
	    if( *li == of )
		{
		li = ls.erase( li );
		if( li != ls.end() )
		    {
		    *ofi += ',';
		    *ofi += *li;
		    li = ls.erase( li );
		    }
		}
	    else
		++li;
	    }
	ret = boost::join( ls, " " );
	}
    if( ret!=opts )
	{
	y2mil( "org:" << opts );
	y2mil( "ret:" << ret );
	}
    return( ret );
    }

int Volume::doFormat()
    {
    static int fcount=1000;
    int ret = 0;
    bool needMount = false;
    y2mil("device:" << dev);
    if( !silent )
	{
	getStorage()->showInfoCb( formatText(true) );
	}
    if (isUsedBy())
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else if( isMounted() )
	{
	ret = umount( orig_mp );
	needMount = ret==0;
	}
    if( ret==0 && !getStorage()->testmode() )
	{
	ret = checkDevice();
	}
    if( ret==0 )
	{
	getStorage()->removeDmTableTo( *this );
	}
    if( ret==0 && encryption!=ENC_NONE )
	{
	ret = Storage::zeroDevice(mountDevice(), size_k, true);
	}
    if( ret==0 && mountDevice()!=dev && !getStorage()->testmode() )
	{
	ret = checkDevice(mountDevice());
	}
    if( ret==0 && mountDevice().find( "/dev/md" )!=0 &&
        mountDevice().find( "/dev/loop" )!=0 )
	{
	SystemCmd(MDADMBIN " --zero-superblock " + quote(mountDevice()));
	}
    if( ret==0 )
	{
	string cmd;
	string params;
	ProgressBar* progressbar = NULL;
	CallbackProgressBar cb = getStorage()->getCallbackProgressBarTheOne();

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
	    case BTRFS:
		cmd = "/sbin/mkfs.btrfs";
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
		params = "-f";
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
		cmd += handle_O_Features(mkfs_opt) + " ";
		}
	    if( !params.empty() )
		{
		cmd += params + " ";
		}
	    if( fs==BTRFS && cType()==BTRFSC && getEncryption()==ENC_NONE )
		{
		const Btrfs* l = static_cast<const Btrfs*>(this);
		list<string> li = l->getDevices();
		for( list<string>::const_iterator i=li.begin(); i!=li.end(); ++i )
		    {
		    cmd += ' ';
		    cmd += quote(*i );
		    }
		}
	    else
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
	if (!getStorage()->testmode())
	    {
	    FsType old=fs;
	    updateFsData();
	    if( fs != old )
		ret = VOLUME_FORMAT_FS_UNDETECTED;
	    else if( fs==BTRFS )
		getStorage()->setUsedByBtrfs( dev, uuid );
	    }
	else
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


    void
    Volume::updateFsData( bool setUsedByLvm )
    {
	Blkid blkid(mountDevice());
	getFsData(blkid,setUsedByLvm);
	if( getFs()==FSUNKNOWN && getEncryption()==ENC_NONE &&
	    orig_encryption==ENC_NONE )
	    {
	    Blkid::Entry e;
	    if( findBlkid( blkid, e ) && e.is_luks)
		initEncryption(ENC_LUKS);
	    }
    }

void Volume::setUsedByUuid( UsedByType ubt, const string& uuid )
    {
    eraseUuid();
    eraseLabel();
    setMount( "" );
    formattingDone();
    setUsedBy( ubt, uuid );
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
    int ret = -1;
    if( fs!=TMPFS )
	{
	ret = cmd.execute( cmdline );
	if( ret != 0 && mountDevice()!=dev )
	    {
	    cmdline = ((detected_fs != SWAP)?UMOUNTBIN " ":SWAPOFFBIN " ") + quote(dev);
	    ret = cmd.execute( cmdline );
	    }
	}
    list<string> mps;
    if( ret!=0 && !mp.empty() && mp!="swap" )
	{
	mps.push_back(mp);
	cmdline = UMOUNTBIN " " + quote(mp);
	ret = cmd.execute( cmdline );
	}
    if( ret!=0 && !orig_mp.empty() && orig_mp!="swap" )
	{
	mps.push_back(orig_mp);
	cmdline = UMOUNTBIN " " + quote(orig_mp);
	ret = cmd.execute( cmdline );
	}
    if( ret != 0 )
	{
	ret = VOLUME_UMOUNT_FAILED;
	ProcMounts mounts;
	list<string> devs;
	devs.push_back( mountDevice() );
	devs.push_back( dev );
	if( mounts.getMount(devs).empty() )
	    {
	    ret = VOLUME_UMOUNT_NOT_MOUNTED;
	    if( !mps.empty() )
		{
		map<string, string> mp = mounts.allMounts();
		for( list<string>::const_iterator i=mps.begin(); i!=mps.end(); ++i )
		    {
		    if( !mp[*i].empty() )
			ret = VOLUME_UMOUNT_FAILED;
		    }
		}
	    }
	if( ret==VOLUME_UMOUNT_NOT_MOUNTED )
	    is_mounted = false;
	}
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
    y2mil("force:" << force << " active:" << dmcrypt_active << " table:" << dmcrypt_dev);
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

Text Volume::mountText( bool doing ) const
    {
    Text txt;
    if( doing )
        {
	if( !mp.empty() )
	    {
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by mount point e.g. /home
	    txt = sformat(_("Mounting %1$s to %2$s"), dev.c_str(), mp.c_str());
	    }
	else
	    {
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    txt = sformat(_("Unmounting %1$s"), dev.c_str());
	    }
        }
    else
        {
	if( !orig_mp.empty() && !mp.empty() && 
	    (!getStorage()->instsys()||mp!=orig_mp||mp!="swap") )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by mount point e.g. /home
	    txt = sformat(_("Change mount point of %1$s to %2$s"), dev.c_str(),
			  mp.c_str());
	    }
	else if( !mp.empty() )
	    {
	    if( mp != "swap" )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by mount point e.g. /home
		txt = sformat(_("Set mount point of %1$s to %2$s"), dev.c_str(),
			      mp.c_str());
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by "swap"
		txt = sformat(_("Use %1$s as %2$s"), dev.c_str(), mp.c_str());
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
	    txt = sformat(_("Remove %1$s from %2$s"), dev.c_str(), fn.c_str());
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
    y2mil("device:" << dev << " mp:" << mp << " orig_mp:" << orig_mp);
    if( !silent )
	{
	getStorage()->showInfoCb( mountText(true) );
	}
    if( ret==0 && !orig_mp.empty() && isMounted() )
	{
	ret = umount(getStorage()->prependRoot(orig_mp));
	}
    string lmount = getStorage()->prependRoot(mp);
    if( ret==0 && lmount!="swap" && access( lmount.c_str(), R_OK )!=0 )
	{
	createPath( lmount );
	}
    if (ret == 0 && !mp.empty() && isUsedBy())
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    if( ret==0 && !mp.empty() && !getStorage()->testmode() )
	{
	bool do_chmod = isTmpCryptMp(mp) && mp!="swap";
	mode_t mode, omode;

	if( fs!=NFS && fs!=NFS4 && fs!=TMPFS )
	    {
	    getStorage()->removeDmTableTo(*this);
	    ret = checkDevice(mountDevice());
	    }
	if( ret==0 && do_chmod )
	    {
	    if( !getStatMode( lmount, mode ) )
		do_chmod=false;
	    else
		y2mil( "Mode of " << lmount << " is " << oct << mode << dec);
	    }
	if( ret==0 )
	    {
	    ret = mount( lmount );
	    }
	if( ret==0 && do_chmod && getStatMode( lmount, omode ) && mode!=omode )
	    {
	    bool ok = setStatMode( lmount, mode );
	    y2mil( "setting mode for " << lmount << " from:" << oct << omode <<
	           " to:" << mode << dec << " ok:" << ok );
	    }
	}
    if( ret==0 )
	{
	ret = doFstabUpdate();
	orig_mp = mp;
	}
    if( ret==0 && mp=="/" && !getStorage()->root().empty() )
	{
	getStorage()->rootMounted();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Volume::canResize( unsigned long long newSizeK ) const
    {
    int ret=0;
    y2mil("val:" << newSizeK);
    if (isUsedBy() && !getStorage()->isUsedBySingleBtrfs(*this))
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    else
	{
	FsCapabilities caps;
	if( !format && fs!=FSNONE && !ignore_fs &&
	    (!getStorage()->getFsCapabilities( fs, caps ) ||
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
		    mpoint = getStorage()->tmpDir() + "/tmp-xfs-mp";
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


int
Volume::setEncryption(bool val, EncryptType typ )
    {
    int ret = 0;
    y2mil("val:" << val << " typ:" << toString(typ));
    if (isUsedBy())
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
	    orig_crypt_pwd.erase();
	    }
	else
	    {
	    if( !isTmpCryptMp(mp) )
		{
		if( !dmcrypt_active && crypt_pwd.empty() )
		    ret = VOLUME_CRYPT_NO_PWD;
		if( ret==0 && !dmcrypt_active && 
		    !pwdLengthOk(typ,crypt_pwd,format) )
		    {
		    ret = VOLUME_CRYPT_PWD_TOO_SHORT;
		    clearCryptPwd();
		    }
		}
	    if( ret == 0 && cType()==NFSC )
		ret = VOLUME_CRYPT_NFS_IMPOSSIBLE;
	    if (ret == 0 && (create || format || loop_active || mp.empty()))
		{
		encryption = typ;
		is_loop = cType() == LOOP;
		dmcrypt_dev = getDmcryptName();
		}
	    if (ret == 0 && !create && !format && !loop_active && !mp.empty() )
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

Text Volume::losetupText( bool doing ) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat(_("Setting up encrypted loop device on %1$s"), dev.c_str());
        }
    else
        {
	// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat(_("Set up encrypted loop device on %1$s"), dev.c_str());
        }
    return( txt );
    }

Text Volume::crsetupText( bool doing ) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat(_("Setting up encrypted dm device on %1$s"), dev.c_str());
        }
    else
        {
	// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat(_("Set up encrypted dm device on %1$s"), dev.c_str());
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

bool Volume::loopInUse(const Storage* sto, const string& loopdev)
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
    Storage::ConstVolPair p = getStorage()->volPair( hasLoopDevice );
    for( Storage::ConstVolIterator i=p.begin(); i!=p.end(); ++i )
	{
	y2mil( "lvol:" << *i );
	unsigned num;
	if( loopStringNum( i->loopDevice(), num ))
	    lnum.push_back( num );
	}
    y2mil( "lnum:" << lnum );
    unsigned num = getStorage()->instsys() ? loop_instsys_offset : 0;
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
	if (getStorage()->instsys())
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
	ret = getFreeLoop(loopData, list<unsigned>());
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
    y2mil( "enctype:" << toString(e) << " dmdev:" << dmdev << " mount:" << mount <<
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

bool Volume::pwdLengthOk( storage::EncryptType typ, const string& val,
                          bool fmt ) const
    {
    bool ret = true;
    if( fmt )
	{
	ret = val.size()>=8;
	}
    else
	{
	if( typ==ENC_TWOFISH_OLD )
	    ret = val.size()>=5;
	else if( typ==ENC_TWOFISH || typ==ENC_TWOFISH256_OLD ) 
	    ret = val.size()>=8;
	else 
	    ret = val.size()>=1;
	}
    return( ret );
    }

int
Volume::setCryptPwd( const string& val )
    {
#ifdef DEBUG_CRYPT_PASSWORD
    y2mil("pwd:" << val << " orig_pwd:" << orig_crypt_pwd );
#endif
    int ret = 0;

    if( !pwdLengthOk(encryption,val,format) && !isTmpCryptMp(mp) )
	ret = VOLUME_CRYPT_PWD_TOO_SHORT;
    else
	{
	orig_crypt_pwd = crypt_pwd;
	crypt_pwd = val;
	if( encryption==ENC_UNKNOWN )
	    detectEncryption();
	}
#ifdef DEBUG_CRYPT_PASSWORD
    y2mil("pwd:" << crypt_pwd << " orig_pwd:" << orig_crypt_pwd );
#endif
    y2mil("ret:" << ret);
    return( ret );
    }

bool
Volume::needCryptPwd() const
    {
    bool ret = crypt_pwd.empty();
    if( ret && is_loop )
	ret = ret && !loop_active;
    if( ret && dmcrypt() )
	ret = ret && !dmcrypt_active;
    y2mil("ret:" << ret);
    return( ret );
    }

bool Volume::needLosetup( bool urgent ) const
    {
    bool ret = (is_loop!=loop_active) &&
		(encryption==ENC_NONE || !crypt_pwd.empty() ||
		(dmcrypt() && cType() == LOOP));
    if( !urgent && loop_dev.empty() )
	ret = false;
    if( is_loop && encryption!=ENC_NONE &&
        !crypt_pwd.empty() && crypt_pwd!=orig_crypt_pwd )
	ret = true;
    return( ret );
    }

bool Volume::needCryptsetup() const
    {
    bool ret = (dmcrypt()!=dmcrypt_active) &&
	       (encryption==ENC_NONE || encryption!=orig_encryption || 
	        !crypt_pwd.empty() || isTmpCryptMp(mp) ||
		(encryption==ENC_LUKS&&!isTmpCryptMp(mp)&&isUsedBy(UB_LVM)) );
    if( dmcrypt() && encryption!=ENC_NONE &&
        ((!crypt_pwd.empty() && crypt_pwd!=orig_crypt_pwd) ||
	 (crypt_pwd.empty() && isTmpCryptMp(mp) && format)) )
	ret = true;
    return( ret );
    }

bool Volume::needCrsetup( bool urgent ) const
    {
    return( needLosetup(urgent)||needCryptsetup() );
    }

bool Volume::needFstabUpdate() const
    {
    bool ret = !ignore_fstab && !(mp.empty() && orig_mp.empty()) &&
	       ((getStorage()->instsys()&&mp=="swap"&&mp==orig_mp) ||
	        fstab_opt!=orig_fstab_opt || mount_by!=orig_mount_by ||
		encryption!=orig_encryption);
    return( ret );
    }

EncryptType Volume::detectEncryption()
    {
    EncryptType ret = ENC_UNKNOWN;
    EncryptType save_enc = encryption;
    EncryptType save_orig = orig_encryption;

    if (getStorage()->testmode())
	{
	ret = encryption = orig_encryption = ENC_TWOFISH;
	y2mil("ret:" << toString(ret));
	return( ret );
	}

    unsigned pos=0;
    static EncryptType try_order[] = { ENC_LUKS, ENC_TWOFISH_OLD,
                                       ENC_TWOFISH256_OLD, ENC_TWOFISH };
    string fname = getStorage()->tmpDir() + "/pwdf";
    string mpname = getStorage()->tmpDir() + "/tmp-enc-mp";
    SystemCmd c;
    y2mil("device:" << dev);

    mkdir( mpname.c_str(), 0700 );
    if( is_loop )
	getFreeLoop();
    detected_fs = fs = FSUNKNOWN;
    bool luks_ok = false;
    do
	{
	ofstream pwdfile( fname.c_str() );
	classic(pwdfile);
	pwdfile << crypt_pwd;
	pwdfile.close();
	encryption = orig_encryption = try_order[pos];
	is_loop = cType() == LOOP;
	dmcrypt_dev = getDmcryptName();
	crUnsetup( true );
	if( is_loop )
	    {
	    string lfile;
	    if( getLoopFile( lfile ))
		c.execute(LOSETUPBIN " " + quote(loop_dev) + " " +
			  quote(getStorage()->root() + lfile));
	    }
	string cmd = getCryptsetupCmd( try_order[pos], dmcrypt_dev, "", fname, false );
	c.execute(MODPROBEBIN " dm-crypt");
	c.execute( cmd );
        string use_dev = dmcrypt_dev;
	luks_ok = try_order[pos]==ENC_LUKS && c.retcode()==0;
	if( c.retcode()==0 )
	    {
	    Storage::waitForDevice(use_dev);
	    updateFsData();
	    unsigned long mj = 0;
	    unsigned long mn;
	    if( storage::getMajorMinor( use_dev, mj, mn ) && mj>0 )
		getStorage()->removeDmTableTo( mj, mn );
	    if( detected_fs!=FSUNKNOWN && !luks_ok )
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
		    case XFS:
			cmd = "xfs_check " + quote(use_dev);
			break;
		    case JFS:
			cmd = "fsck.jfs -n " + quote(use_dev);
			break;
		    default:
			cmd = "fsck -n -t " + toString(detected_fs) + " " + quote(use_dev);
			break;
		    }
		bool excTime, excLines;
		c.executeRestricted( cmd, 15, 500, excTime, excLines );
		bool ok = c.retcode()==0 || (excTime && !excLines);
		y2mil("ok:" << ok << " retcode:" << c.retcode() << " excTime:" << excTime <<
		      " excLines:" << excLines);
		if( ok )
		    {
		    c.execute(MODPROBEBIN " " + toString(detected_fs));
		    c.execute(MOUNTBIN " -oro -t " + toString(detected_fs) + " " +
			      quote(use_dev) + " " + quote(mpname));
		    ok = c.retcode()==0;
		    c.execute(UMOUNTBIN " " + quote(mpname));
		    }
		if( !ok )
		    {
		    detected_fs = fs = FSUNKNOWN;
		    eraseLabel();
		    eraseUuid();
		    }
		}
	    }
	if( fs==FSUNKNOWN && !luks_ok )
	    pos++;
	y2mil("pos:" << pos << " luks_ok:" << luks_ok << " fs:" << toString(fs));
	}
    while( !luks_ok && detected_fs==FSUNKNOWN && pos<lengthof(try_order) );
    crUnsetup( true );
    if( detected_fs!=FSUNKNOWN || luks_ok )
	{
	is_loop = cType() == LOOP;
	ret = encryption = orig_encryption = try_order[pos];
	orig_crypt_pwd = crypt_pwd;
	}
    else
	{
	is_loop = false;
	dmcrypt_dev.erase();
	loop_dev.erase();
	crypt_pwd.erase();
	orig_crypt_pwd.erase();
	ret = ENC_UNKNOWN;
	encryption = save_enc;
	orig_encryption = save_orig;
	}
    unlink( fname.c_str() );
    rmdir( mpname.c_str() );
#ifdef DEBUG_CRYPT_PASSWORD
    y2mil("pwd:" << crypt_pwd << " orig_pwd:" << orig_crypt_pwd );
#endif
    y2mil("ret:" << toString(ret));
    return( ret );
    }

int Volume::doLosetup()
    {
    int ret = 0;
    y2mil("device:" << dev << " mp:" << mp << " is_loop:" << is_loop << " loop_active:" << loop_active);
    if( !silent && is_loop && !dmcrypt() )
	{
	getStorage()->showInfoCb( losetupText(true) );
	}
    if( is_mounted )
	{
	umount( orig_mp );
	}
    if( is_loop )
	{
	getStorage()->removeDmTableTo( *this );
	if( ret==0 && loop_dev.empty() )
	    {
	    ret = getFreeLoop();
	    }
	if( ret==0 )
	    {
	    string fname;
	    if( !dmcrypt() )
		{
		fname = getStorage()->tmpDir() + "/pwdf";
		ofstream pwdfile( fname.c_str() );
		classic(pwdfile);
		pwdfile << crypt_pwd << endl;
		pwdfile.close();
		}
	    SystemCmd c( getLosetupCmd( encryption, fname ));
	    if( c.retcode()!=0 )
		ret = VOLUME_LOSETUP_FAILED;
	    else
		orig_crypt_pwd = crypt_pwd;
	    if( !fname.empty() )
		{
		unlink( fname.c_str() );
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
	if (!loop_dev.empty())
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
    if( cType() != LOOP )
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
    y2mil("device:" << dev << " prefix:" << prefix << " new:" << newn);
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
    y2mil("device:" << dev << " mp:" << mp << " dmcrypt:" << dmcrypt() << 
          " active:" << dmcrypt_active << " format:" << format );
    if( !silent && dmcrypt() )
	{
	getStorage()->showInfoCb( crsetupText(true) );
	}
    if( is_mounted )
	{
	umount( orig_mp );
	}
    if( dmcrypt() )
	{
	getStorage()->removeDmTableTo( *this );
	if( ret==0 && dmcrypt_dev.empty() )
	    {
	    dmcrypt_dev = getDmcryptName();
	    }
	if( ret==0 )
	    {
	    ret = cryptUnsetup();
	    }
	if( ret==0 && encryption==ENC_LUKS && !isTmpCryptMp(mp) &&
	    crypt_pwd.empty() )
	    {
	    ret = VOLUME_CRYPT_NO_PWD;
	    }
	if( ret==0 )
	    {
	    string fname = getStorage()->tmpDir() + "/pwdf";
	    ofstream pwdfile( fname.c_str() );
	    classic(pwdfile);
	    pwdfile << crypt_pwd;
	    pwdfile.close();
	    SystemCmd cmd;
	    if( format || (isTmpCryptMp(mp)&&crypt_pwd.empty()) ||
	        (encryption!=ENC_NONE&&orig_crypt_pwd!=crypt_pwd) )
		{
		string cmdline = getCryptsetupCmd( encryption, dmcrypt_dev, mp, fname, true,
						   crypt_pwd.empty() );
		if( !cmdline.empty() )
		    {
		    cmd.execute( cmdline );
		    if( cmd.retcode()!=0 )
			ret = VOLUME_CRYPTFORMAT_FAILED;
		    if( ret==0 && mp=="swap" && crypt_pwd.empty() && !format )
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
	    if( ret==0 )
		orig_crypt_pwd = crypt_pwd;
	    unlink( fname.c_str() );
	    Storage::waitForDevice(dmcrypt_dev);
	    }
	if( ret==0 )
	    {
	    dmcrypt_active = true;
	    unsigned long dummy, minor;
	    if (cType() == LOOP)
		{
		getMajorMinor();
		minor = mnr;
		replaceAltName( "/dev/dm-", Dm::dmDeviceName(mnr) );
		}
	    else
		{
		storage::getMajorMinor( dmcrypt_dev, dummy, minor );
		replaceAltName("/dev/dm-", Dm::dmDeviceName(minor));
		}

	    ProcParts parts;
	    unsigned long long sz;
	    if (parts.getSize( Dm::dmDeviceName(minor), sz))
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
    bool force_fstab_rewrite = false;
    bool losetup_done = false;
    bool did_cryptsetup = false;
    if( needLosetup(true) )
	{
	ret = doLosetup();
	losetup_done = ret==0;
	}
    if( ret==0 && needCryptsetup() )
	{
	force_fstab_rewrite = 
	    encryption != ENC_NONE &&
	    ((!crypt_pwd.empty() && crypt_pwd!=orig_crypt_pwd) ||
	     (crypt_pwd.empty() && isTmpCryptMp(mp) && format));
	ret = doCryptsetup();
	if( ret!=0 && losetup_done )
	    loUnsetup();
	did_cryptsetup = true;
	}
    if (ret == 0)
    {
	updateFsData(did_cryptsetup); 
    }
    if (ret == 0 && encryption != ENC_NONE)
    {
	doFstabUpdate(force_fstab_rewrite);
    }
    y2mil("ret:" << ret);
    return ret;
    }

Text Volume::labelText( bool doing ) const
    {
    Text txt;
    if( doing )
    {
	if( label.empty() )
	{
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    txt = sformat(_("Clearing label on %1$s"), dev.c_str());
	}
	else
	{
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by a name e.g. ROOT
	    txt = sformat(_("Setting label on %1$s to %2$s"), dev.c_str(), label.c_str());
	}
    }
    else
    {
	if( label.empty() )
	{
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    txt = sformat(_("Clear label on %1$s"), dev.c_str());
	}
	else
	{  
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by a name e.g. ROOT
	    txt = sformat(_("Set label on %1$s to %2$s"), dev.c_str(), label.c_str());
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
    if( !silent )
	{
	getStorage()->showInfoCb( labelText(true) );
	}
    if (!getStorage()->getFsCapabilities(fs, caps) || !caps.supportsLabel)
	{
	ret = VOLUME_LABEL_TOO_LONG;
	}
    if (ret == 0 && isUsedBy())
	{
	ret = VOLUME_ALREADY_IN_USE;
	}
    if( ret==0 && is_mounted && !caps.labelWhileMounted )
	{
	ret = umount(getStorage()->prependRoot(orig_mp));
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
	ret = mount(getStorage()->prependRoot(orig_mp));
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
    if (getStorage()->getFsCapabilities(fs, caps) && caps.supportsLabel)
	{
	if( caps.labelLength < val.size() )
	    ret = VOLUME_LABEL_TOO_LONG;
	else if (isUsedBy())
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
	string fsn = toString(fs);
	switch( fs )
	    {
	    case NTFS:
		fsn = "ntfs-3g";
		break;
	    case FSUNKNOWN:
		fsn = "auto";
		break;
	    default:
		cmd.execute(MODPROBEBIN " " + fsn);
		break;
	    }
	if( fs == VFAT )
	    {
	    cmd.execute(MODPROBEBIN " nls_cp437");
	    cmd.execute(MODPROBEBIN " nls_iso8859-1");
	    }
	cmdline = MOUNTBIN " ";
	if( ro )
	    cmdline += "-r ";
	cmdline += "-t " + fsn + " ";

	const char * ign_opt[] = { "defaults", "" };
	const char * ign_beg[] = { "loop", "encryption=", "phash=",
	                           "itercountk=", "" };
	if (getStorage()->instsys())
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
	if( !l.empty() )
	    cmdline += "-o " + boost::join(l, ",") + " ";

	cmdline += quote(mountDevice()) + " " + quote(lmount);
	}
    else
	{
	cmdline = SWAPONBIN " --fixpgsz " + quote(mountDevice());
	if (getStorage()->instsys())
	    {
		ProcMounts mounts;
		string m = mounts.getMount(mountDevice());
	    if( m.empty() )
		{
		    m = mounts.getMount(alt_names);
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

int Volume::unaccessVol()
    {
    int ret = 0;
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
    return( ret );
    }

int Volume::prepareRemove()
    {
    y2mil("device:" << dev);
    int ret = unaccessVol();
    if( loop_active || dmcrypt_active )
	{
	crUnsetup();
	}
    getStorage()->eraseCachedFreeInfo(dev);
    getStorage()->removeDmTableTo(*this);
    y2mil("ret:" << ret);
    return( ret );
    }


    string
    Volume::getMountByString() const
    {
    string ret = dev;

	switch (mount_by)
	{
	    case MOUNTBY_UUID:
		if (!uuid.empty())
		    ret = "UUID=" + uuid;
		else
		    y2err("no uuid defined");
		break;

	    case MOUNTBY_LABEL:
		if (!label.empty())
		    ret = "LABEL=" + label;
		else
		    y2err("no label defined");
		break;

	    case MOUNTBY_ID:
		if (!udevId().empty())
		    ret = "/dev/disk/by-id/" + udevId().front();
		else
		    y2err("no udev-id defined");
		break;

	    case MOUNTBY_PATH:
		if (!udevPath().empty())
		    ret = "/dev/disk/by-path/" + udevPath();
		else
		    y2err("no udev-path defined");
		break;

	    case MOUNTBY_DEVICE:
		break;
	}

	return ret;
    }


void
Volume::getCommitActions(list<commitAction>& l) const
    {
    Volume const * p=this;
    if( getStorage()->isUsedBySingleBtrfs(*this))
	{
	Volume const * n=NULL;
	string id = "UUID="+getUsedBy().front().device();
	y2mil( "usedBy:" << id );
	if( getStorage()->findVolume( id, n ))
	    {
	    p = n;
	    y2mil( "found:" << *p );
	    }
	}
    if( deleted() )
	{
	l.push_back(commitAction(DECREASE, cType(), p->removeText(false), this, true));
	}
    else if( needShrink() && !format )
	{
	l.push_back(commitAction(DECREASE, cType(), p->resizeText(false), this, true));
	}
    else if( created() )
	{
	l.push_back(commitAction(INCREASE, cType(), p->createText(false), this, false));
	}
    else if( needExtend() && !format )
	{
	l.push_back(commitAction(INCREASE, cType(), p->resizeText(false), this, true));
	}
    else if( format )
	{
	l.push_back(commitAction(FORMAT, cType(), p->formatText(false), this, true));
	}
    else if( needCrsetup(false) )
	{
	l.push_back(commitAction(mp.empty()?INCREASE:FORMAT, cType(), 
	                         p->crsetupText(false), this, mp.empty()));
	}
    else if (mp != orig_mp || (getStorage()->instsys() && mp=="swap"))
	{
	l.push_back(commitAction(MOUNT, cType(), p->mountText(false), this, false));
	}
    else if( label != orig_label )
	{
	l.push_back(commitAction(MOUNT, cType(), p->labelText(false), this, false));
	}
    else if( needFstabUpdate() )
	{
	l.push_back(commitAction(MOUNT, cType(), p->fstabUpdateText(), this, false));
	}
    }


Text Volume::fstabUpdateText() const
    {
    Text txt;
    const EtcFstab* fstab = getStorage()->getFstab();
    if( !orig_mp.empty() && mp.empty() )
	txt = fstab->removeText( false, inCryptotab(), orig_mp );
    else
	txt = fstab->updateText( false, inCryptotab(), orig_mp );
    return( txt );
    }

string Volume::getFstabDevice() const
    {
    string ret = dev;
    if( cType() == TMPFSC )
	ret = "tmpfs";
    else if (cType() == LOOP)
	{
	const Loop* l = static_cast<const Loop*>(this);
	if( l && dmcrypt() )
	    ret = l->loopFile();
	}
    y2mil( "ret:" << ret );
    return( ret );
    }

string Volume::getFstabDentry() const
    {
    string ret;
    if (cType() != LOOP)
	{
	if( dmcrypt() )
	    ret = inCryptotab()?dev:dmcrypt_dev;
	else if( cType() == TMPFSC )
	    ret = "tmpfs";
	else
	    ret = getMountByString();
	}
    else
	{
	const Loop* l = static_cast<const Loop*>(this);
	if( dmcrypt() )
	    ret = dmcrypt_dev;
	else if( l )
	    ret = l->loopFile();
	}
    return( ret );
    }


list<string>
Volume::getFstabOpts() const
    {
    list<string> l;
	
    if( fstab_opt.empty() )
	l.push_back( (is_loop&&!dmcrypt())?"noatime":"defaults" );
    else
	l = splitString( fstab_opt, "," );
    list<string>::iterator loop = find( l.begin(), l.end(), "loop" );
    if( loop == l.end() )
	loop = find_if( l.begin(), l.end(), string_starts_with( "loop=" ) );
    list<string>::iterator enc =
	find_if( l.begin(), l.end(), string_starts_with( "encryption=" ) );
    string lstr;
    if( (encryption!=ENC_NONE && !dmcrypt()) || cType() == LOOP )
	{
	lstr = "loop";
	if(  !fstab_loop_dev.empty() && !dmcrypt() )
	    lstr += "="+fstab_loop_dev;
	}
    string estr;
    if( encryption!=ENC_NONE && !dmcrypt() )
	estr = "encryption=" + toString(encryption);
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

    return l;
    }


bool Volume::getLoopFile( string& fname ) const
    {
    const Loop* l = NULL;
    if (cType() == LOOP)
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
	ret = (*i=="usrquota") || (*i=="grpquota") ||
	      i->find("usrjquota=")==0 || i->find("grpjquota=")==0;
	++i;
	}
    y2mil( "fstopt:" << fstopt << " ret:" << ret );
    return( ret );
    }

bool Volume::noFreqPassno() const
    {
    return( fs==SWAP || fs==NFS || fs==NFS4 || fs==VFAT || fs==NTFS || 
            fs==FSUNKNOWN || fs==TMPFS || is_loop || optNoauto() );
    }

unsigned Volume::fstabFreq() const
    {
    unsigned ret = 1;
    if( noFreqPassno() || encryption!=ENC_NONE )
	ret = 0;
    return ret;
    }

unsigned Volume::fstabPassno() const
    {
    unsigned ret = 2;
    if( noFreqPassno() || (encryption!=ENC_NONE) )
	ret = 0;
    else if( mp=="/" )
	ret = 1;
    return( ret );
    }

bool Volume::pvEncryption() const
    {
    return( encryption==ENC_LUKS && isUsedBy(UB_LVM) );
    }

int Volume::doFstabUpdate( bool force_rewrite )
    {
    int ret = 0;
    bool changed = false;
    y2mil( "force_rewrite:" << force_rewrite );
    y2mil(*this);
    if( !ignore_fstab )
	{
	EtcFstab* fstab = getStorage()->getFstab();
	FstabEntry entry;
	if( (!orig_mp.empty() || orig_encryption != ENC_NONE) &&
	    (deleted() || (mp.empty() && !pvEncryption())) &&
	    (fstab->findDevice( dev, entry ) ||
	     fstab->findDevice( alt_names, entry ) ||
	     ((cType()==LOOP||cType()==TMPFSC) && fstab->findMount( orig_mp, entry )) ||
	     ((cType()==LOOP||cType()==TMPFSC) && fstab->findMount( mp, entry ))) )
	    {
	    changed = true;
	    if( !silent )
		{
		getStorage()->showInfoCb(fstab->removeText(true, entry.cryptotab, entry.mount));
		}
	    y2mil("before removeEntry");
	    ret = fstab->removeEntry( entry );
	    }
	else if ((!mp.empty() || pvEncryption()) && !deleted())
	    {
	    string fname;
	    if( fstab->findDevice( dev, entry ) ||
		fstab->findDevice( alt_names, entry ) ||
		(cType() == LOOP && getLoopFile(fname) &&
		     fstab->findDevice( fname, entry )) ||
		(cType() == TMPFSC && !mp.empty() &&
		     fstab->findMount( mp, entry )))
		{
		y2mil( "changed:" << entry );    
		changed = force_rewrite;
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
		    che.opts = getFstabOpts();
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
		if( fs != detected_fs || che.fs!=toString(fs) )
		    {
		    changed = true;
		    che.fs = toString(fs);
		    che.freq = fstabFreq();
		    che.passno = fstabPassno();
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
		    che.freq = fstabFreq();
		    che.passno = fstabPassno();
		    }
		che.tmpcrypt = dmcrypt() && isTmpCryptMp(mp) && 
		               crypt_pwd.empty();
		if( changed )
		    {
		    if( !silent && !fstab_added )
			{
			getStorage()->showInfoCb(
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
		    che.device = "/dev/disk/by-id/" + udevId().front();
		che.dentry = getFstabDentry();
		che.encr = encryption;
		if( dmcrypt() && isTmpCryptMp(mp) && crypt_pwd.empty() )
		    che.tmpcrypt = true;
		if( inCryptotab() )
		    {
		    getFreeLoop();
		    che.loop_dev = fstab_loop_dev;
		    }
		che.fs = toString(fs);
		che.opts = getFstabOpts();
		che.mount = mp;
		che.freq = fstabFreq();
		che.passno = fstabPassno();
		if( !silent )
		    {
		    getStorage()->showInfoCb(
			fstab->addText( true, inCryptotab(), che.mount ));
		    }
		ret = fstab->addEntry( che );
		fstab_added = true;
		}
	    }
	if( changed && ret==0 && getStorage()->isRootMounted() )
	    {
	    ret = fstab->flush();
	    }
	}
    if( ret==0 && !format && !getStorage()->instsys() &&
        fstab_opt!=orig_fstab_opt && !orig_fstab_opt.empty() &&
        mp==orig_mp && mp!="swap" )
	{
	SystemCmd c;
	y2mil( "fstab_opt:" << fstab_opt << " fstab_opt_orig:" << orig_fstab_opt );
	y2mil( "remount:" << *this );
	int r = 0;
	if( isMounted() )
	    {
	    r = umount( mp );
	    y2mil( "remount umount:" << r );
	    }
	if( r==0 || r==VOLUME_UMOUNT_NOT_MOUNTED )
	    {
	    ret = mount(getStorage()->prependRoot(mp));
	    y2mil( "remount mount:" << ret );
	    }
	else
	    {
	    c.execute(MOUNTBIN " -oremount " + quote(mp));
	    y2mil( "remount remount:" << c.retcode() );
	    if( c.retcode()!=0 )
		ret = VOLUME_REMOUNT_FAILED;
	    }
	if( !getStorage()->instsys() &&
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


bool Volume::canUseDevice() const
    {
    bool ret = !isUsedBy() && getMount().empty();
    return ret;
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


Text
Volume::createText(bool doing) const
{
    Text txt;
    if (doing)
    {
	// displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	txt = sformat(_("Creating volume %1$s"), dev.c_str());
    }
    else
    {
	if (mp == "swap")
	{
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat(_("Create swap volume %1$s (%2$s)"), dev.c_str(),
			  sizeString().c_str());
	}
	else if (mp == "/")
	{
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat(_("Create root volume %1$s (%2$s) with %3$s"),
			  dev.c_str(), sizeString().c_str(), fsTypeString().c_str());
	}
	else if (mp == getStorage()->bootMount())
	{
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat(_("Create boot volume %1$s (%2$s) with %3$s"),
			  dev.c_str(), sizeString().c_str(), fsTypeString().c_str());
	}
	else if (!mp.empty())
	{
	    if (encryption == ENC_NONE)
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat(_("Create volume %1$s (%2$s) for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str());
	    }
	    else
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat(_("Create encrypted volume %1$s (%2$s) for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str());
	    }
	}
	else
	{
	    if (encryption == ENC_NONE)
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat(_("Create volume %1$s (%2$s)"), dev.c_str(), sizeString().c_str());
	    }
	    else
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat(_("Create encrypted volume %1$s (%2$s)"), dev.c_str(),
			      sizeString().c_str());
	    }
	}
    }
    return txt;
}


Text Volume::resizeText( bool doing ) const
    {
    Text txt;
    if( doing )
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat(_("Shrinking %1$s to %2$s"), dev.c_str(), sizeString().c_str());
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat(_("Extending %1$s to %2$s"), dev.c_str(), sizeString().c_str());
	txt += Text(" ", " ");
	// text displayed during action
	txt += _("(progress bar might not move)");
        }
    else
        {
	if( needShrink() )
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat(_("Shrink %1$s to %2$s"), dev.c_str(), sizeString().c_str());
	else
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat(_("Extend %1$s to %2$s"), dev.c_str(), sizeString().c_str());
        }
    return txt;
    }

Text Volume::removeText( bool doing ) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Removing volume %1$s"), dev.c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
        txt = sformat( _("Remove volume %1$s"), dev.c_str() );
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
    if( dmcrypt() )
	info.crypt_device = dmcrypt_dev;
    info.mount = mp;
    info.mount_by = mount_by;

    info.udevPath = udevPath();
    info.udevId = boost::join(udevId(), " ");

    info.usedBy = list<UsedByInfo>(uby.begin(), uby.end());

    if (uby.empty())
    {
	info.usedByType = UB_NONE;
	info.usedByDevice = "";
    }
    else
    {
	info.usedByType = uby.front().type();
	info.usedByDevice = uby.front().device();
    }

    info.ignore_fstab = ignore_fstab;
    info.fstab_options = fstab_opt;
    info.uuid = uuid;
    info.label = label;
    info.encryption = encryption;
    info.crypt_pwd = crypt_pwd;
    info.fs = fs;
    info.detected_fs = detected_fs;
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
	info.origSizeK = orig_size_k;
    else
	info.origSizeK = 0;
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


std::ostream& operator<< (std::ostream& s, const Volume &v )
    {
    s << dynamic_cast<const Device&>(v);
    s << " Nr:" << v.num;
    if( v.size_k != v.orig_size_k )
	s << " orig_SizeK:" << v.orig_size_k;
    if( v.ronly )
	s << " readonly";
    if( v.format )
	s << " format";
    if( v.ignore_fstab )
	s << " ignoreFstab";
    if( v.ignore_fs )
	s << " ignoreFs";
    if (!v.uby.empty())
	s << " usedby:" << v.uby;
    if( v.fs != storage::FSUNKNOWN )
	{
	s << " fs:" << toString(v.fs);
	if( v.fs != v.detected_fs && v.detected_fs!=storage::FSUNKNOWN )
	    s << " det_fs:" << toString(v.detected_fs);
	}
    if (!v.mp.empty())
	{
	s << " mount:" << v.mp;
	if( !v.is_mounted )
	    s << " not_mounted";
	}
    if( v.mp != v.orig_mp && !v.orig_mp.empty() )
	s << " orig_mount:" << v.orig_mp;
    if( v.mount_by != storage::MOUNTBY_DEVICE )
	{
	s << " mount_by:" << toString(v.mount_by);
	if( v.mount_by != v.orig_mount_by )
	    s << " orig_mount_by:" << toString(v.orig_mount_by);
	}
    if (!v.uuid.empty())
	{
	s << " uuid:" << v.uuid;
	}
    if (!v.label.empty())
	{
	s << " label:" << v.label;
	}
    if( v.label != v.orig_label && !v.orig_label.empty() )
	s << " orig_label:" << v.orig_label;
    if( !v.fstab_opt.empty() )
	{
	s << " fstopt:" << v.fstab_opt;
	if( v.fstab_opt != v.orig_fstab_opt && !v.orig_fstab_opt.empty() )
	    s << " orig_fstopt:" << v.orig_fstab_opt;
	}
    if (!v.mkfs_opt.empty())
	{
	s << " mkfsopt:" << v.mkfs_opt;
	} 
    if (!v.tunefs_opt.empty())
	{
	s << " tunefsopt:" << v.tunefs_opt;
	}
    if (!v.dtxt.empty())
	{
	s << " dtxt:" << v.dtxt;
	}
    if (!v.alt_names.empty())
	s << " alt_names:" << v.alt_names;
    if( v.encryption != storage::ENC_NONE ||
        v.orig_encryption != storage::ENC_NONE )
	{
	s << " encr:" << toString(v.encryption);
	if( v.encryption != v.orig_encryption &&
	    v.orig_encryption!=storage::ENC_NONE )
	    s << " orig_encr:" << toString(v.orig_encryption);
#ifdef DEBUG_CRYPT_PASSWORD
	s << " pwd:" << v.crypt_pwd;
	if( v.orig_crypt_pwd.empty() && v.crypt_pwd!=v.orig_crypt_pwd )
	    s << " orig_pwd:" << v.orig_crypt_pwd;
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


    void
    Volume::logDifference(std::ostream& log, const Volume& rhs) const
    {
	Device::logDifference(log, rhs);

	logDiff(log, "num", num, rhs.num);

	logDiff(log, "size_k", size_k, rhs.size_k);
	logDiff(log, "orig_size_k", orig_size_k, rhs.orig_size_k);

	logDiff(log, "readonly", ronly, rhs.ronly);
	logDiff(log, "format", format, rhs.format);

	logDiff(log, "usedby", uby, rhs.uby);

	logDiffEnum(log, "fs", fs, rhs.fs);
	logDiffEnum(log, "det_fs", detected_fs, rhs.detected_fs);

	logDiff(log, "mount", mp, rhs.mp);
	logDiff(log, "orig_mount", orig_mp, rhs.orig_mp);

	logDiff(log, "is_mounted", is_mounted, rhs.is_mounted);

	logDiffEnum(log, "mount_by", mount_by, rhs.mount_by);
	logDiffEnum(log, "orig_mount_by", orig_mount_by, rhs.orig_mount_by);

	logDiff(log, "uuid", uuid, rhs.uuid);

	logDiff(log, "label", label, rhs.label);
	logDiff(log, "orig_label", orig_label, rhs.orig_label);

	logDiff(log, "fstopt", fstab_opt, rhs.fstab_opt);
	logDiff(log, "orig_fstopt", orig_fstab_opt, rhs.orig_fstab_opt);
	logDiff(log, "mkfsopt", mkfs_opt, rhs.mkfs_opt);
	logDiff(log, "tunefsopt", tunefs_opt, rhs.tunefs_opt);

	logDiff(log, "dtxt", dtxt, rhs.dtxt);

	logDiff(log, "is_loop", is_loop, rhs.is_loop);
	logDiff(log, "loop_active", loop_active, rhs.loop_active);
	logDiff(log, "loop_dev", loop_dev, rhs.loop_dev);
	logDiff(log, "fstab_loop_dev", fstab_loop_dev, rhs.fstab_loop_dev);

	logDiffEnum(log, "encr", encryption, rhs.encryption);
	logDiffEnum(log, "orig_encr", orig_encryption, rhs.orig_encryption);
#ifdef DEBUG_CRYPT_PASSWORD
	logDiff(log, "pwd", crypt_pwd, rhs.crypt_pwd);
#endif
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
	    tunefs_opt==rhs.tunefs_opt && dtxt==rhs.dtxt &&
	    is_loop==rhs.is_loop && loop_active==rhs.loop_active &&
	    is_mounted==rhs.is_mounted && encryption==rhs.encryption &&
	    loop_dev==rhs.loop_dev && fstab_loop_dev==rhs.fstab_loop_dev &&
	    uby==rhs.uby );
    }


    bool
    Volume::isTmpCryptMp(const string& mp)
    {
	const string* end = tmp_mount + lengthof(tmp_mount);
	bool ret = find(tmp_mount, end, mp) != end;
	y2mil("find mp:" << mp << " ret:" << ret);
	return ret;
    }


    const string Volume::tmp_mount[] = { "swap", "/tmp", "/var/tmp" };

}
