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


#include <sstream>

#include "storage/Partition.h"
#include "storage/AppUtil.h"
#include "storage/Disk.h"
#include "storage/Storage.h"


namespace storage
{
    using namespace std;


    Partition::Partition(const Disk& c, const string& name, const string& device, unsigned PNr,
			 unsigned long long SizeK, const Region& cylRegion, PartitionType Type,
			 unsigned Id, bool Boot)
	: Volume(c, name, device), reg(cylRegion), bootflag(Boot), typ(Type), idt(Id), orig_id(Id)
    {
	numeric = true;
	num = orig_num = PNr;
	size_k = orig_size_k = SizeK;

    addUdevData();
    y2deb("constructed Partition " << dev << " on " << cont->device());
    }


    Partition::Partition(const Disk& c, const string& name, const string& device, unsigned PNr,
			 SystemInfo& systeminfo, unsigned long long SizeK, const Region& cylRegion,
			 PartitionType Type, unsigned Id, bool Boot)
	: Volume(c, name, device, systeminfo), reg(cylRegion), bootflag(Boot), typ(Type), idt(Id),
	  orig_id(Id)
    {
	numeric = true;
	num = orig_num = PNr;
	size_k = orig_size_k = SizeK;

	getMajorMinor();

	addUdevData();
	y2deb("constructed Partition " << dev << " on " << cont->device());
    }


    Partition::Partition(const Disk& c, const xmlNode* node)
	: Volume(c, node), reg(0, 0), bootflag(false)
    {
	string tmp;

	assert(numeric);
	assert(num > 0);

	getChildValue(node, "region", reg);

	if (getChildValue(node, "partition_type", tmp))
	    typ = toValueWithFallback(tmp, PRIMARY);
	getChildValue(node, "partition_id", idt);

	getChildValue(node, "boot_flag", bootflag);

	orig_num = num;
	orig_id = idt;

	y2deb("constructed Partition " << dev);
    }


    Partition::Partition(const Disk& c, const Partition& v)
	: Volume(c, v), reg(v.reg), bootflag(v.bootflag), typ(v.typ),
	  idt(v.idt), orig_id(v.orig_id), orig_num(v.orig_num)
    {
	y2deb("copy-constructed Partition " << dev);
    }


    Partition::~Partition()
    {
	y2deb("destructed Partition " << dev);
    }


    void
    Partition::saveData(xmlNode* node) const
    {
	Volume::saveData(node);

	setChildValue(node, "region", reg);

	setChildValue(node, "partition_type", toString(typ));
	setChildValue(node, "partition_id", idt);

	if (bootflag)
	    setChildValue(node, "boot_flag", true);
    }


    list<string>
    Partition::udevId() const 
    { 
	list<string> ret;
	const list<string> tmp = disk()->udevId();
	for (list<string>::const_iterator i = tmp.begin(); i != tmp.end(); ++i)
	    ret.push_back(udevAppendPart(*i, num));
	return ret;
    }


    string
    Partition::udevPath() const 
    {
	const string tmp = disk()->udevPath();
	if (!tmp.empty())
	    return udevAppendPart(tmp, num);
	return "";
    }


    string
    Partition::sysfsPath() const
    {
	return disk()->sysfsPath() + "/" + boost::replace_all_copy(procName(), "/", "!");
    }


bool Partition::intersectArea( const Region& r, unsigned fuzz ) const
    {
    return( r.intersect( reg ).len()>fuzz );
    }

bool Partition::contains( const Region& r, unsigned fuzz ) const
    {
    return( (r.len() - reg.intersect( r ).len()) <= fuzz );
    }

void Partition::addUdevData()
    {
    addAltUdevPath( num );
    addAltUdevId( num );
    y2mil("dev:" << dev << " mby:" << toString(mount_by) << " orig:" << toString(orig_mount_by));
    mount_by = orig_mount_by = defaultMountBy();
    y2mil("dev:" << dev << " mby:" << toString(mount_by) << " orig:" << toString(orig_mount_by));
    }


void Partition::addAltUdevId( unsigned num )
    {
    alt_names.remove_if(string_contains("/by-id/"));

    const list<string> tmp = disk()->udevId();
    for (list<string>::const_iterator i = tmp.begin(); i != tmp.end(); ++i)
	alt_names.push_back("/dev/disk/by-id/" + udevAppendPart(*i, num));
    }


void Partition::addAltUdevPath( unsigned num )
    {
    alt_names.remove_if(string_contains("/by-path/"));

    const string tmp = disk()->udevPath();
    if (!tmp.empty() )
	alt_names.push_back("/dev/disk/by-path/" + udevAppendPart(tmp, num));
    }


void Partition::changeNumber( unsigned new_num )
    {
    if( new_num!=num )
	{
	string old = dev;
	if( orig_num==num )
	    {
	    orig_num = num;
	    }
	num = new_num;
	if( created() )
	    {
	    orig_num = num;
	    }

	addAltUdevId(num);
	addAltUdevPath(num);

	setNameDevice(disk()->getPartName(num), disk()->getPartDevice(num));

	getMajorMinor();
	getStorage()->changeDeviceName(old, dev);
	}
    }


int
Partition::changeId(unsigned new_id)
{
    int ret = 0;
    if (new_id == 0)
    {
	ret = DISK_INVALID_PARTITION_ID;
    }
    if (ret == 0 && new_id != idt)
    {
	if( orig_id==idt )
	{
	    orig_id = idt;
	}
	idt = new_id;
	if( created() )
	{
	    orig_id = idt;
	}
    }
    y2mil("ret:" << ret);
    return ret;
}


void Partition::changeIdDone()
    {
    orig_id = idt;
    }

void Partition::unChangeId()
    {
    idt = orig_id;
    }


    void
    Partition::changeRegion(const Region& cylRegion, unsigned long long SizeK)
    {
	reg = cylRegion;
	size_k = orig_size_k = SizeK;
    }


bool Partition::canUseDevice() const
    {
    bool ret = Volume::canUseDevice();
    if( ret )
	ret = type()!=EXTENDED;
    return( ret );
    }

void Partition::setResizedSize( unsigned long long SizeK ) 
    {
    Volume::setResizedSize(SizeK);
    reg.setLen(disk()->kbToCylinder(SizeK));
    }

void Partition::forgetResize() 
    { 
    Volume::forgetResize();
    reg.setLen(disk()->kbToCylinder(size_k));
    }

bool Partition::operator== ( const Partition& rhs ) const
    {
    return( orig_num == rhs.orig_num &&
            num == rhs.num &&
            del == rhs.del &&
	    cont == rhs.getContainer() );
    }

bool Partition::operator< ( const Partition& rhs ) const
    {
    if( cont != rhs.getContainer() )
	return( *cont < *rhs.getContainer() );
    else if( orig_num!=rhs.orig_num )
	return( orig_num<rhs.orig_num );
    else
        return( !del );
    }


Text Partition::setTypeText( bool doing ) const
    {
    Text txt;
    string d = dev;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by partition name (e.g. /dev/hda1),
        // %2$s is replaced by hexadecimal number (e.g. 8E)
        txt = sformat( _("Setting type of partition %1$s to %2$X"),
                      d.c_str(), id() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by partition name (e.g. /dev/hda1),
        // %2$s is replaced by hexadecimal number (e.g. 8E)
        txt = sformat( _("Set type of partition %1$s to %2$X"),
                      d.c_str(), id() );
        }
    return( txt );
    }

const Disk* Partition::disk() const
    {
    return(dynamic_cast<const Disk* const>(cont));
    }

int Partition::setFormat( bool val, storage::FsType new_fs )
    {
    int ret = 0;
    y2mil("device:" << dev << " val:" << val << " fs:" << toString(new_fs));
    if( typ==EXTENDED )
	{
	if( val )
	    ret = VOLUME_FORMAT_EXTENDED_UNSUPPORTED;
	}
    else
	ret = Volume::setFormat( val, new_fs );
    y2mil("ret:" << ret);
    return( ret );
    }

int Partition::changeMount( const string& val )
    {
    int ret = 0;
    y2mil("device:" << dev << " val:" << val);
    if( typ==EXTENDED )
	ret = VOLUME_MOUNT_EXTENDED_UNSUPPORTED;
    else
	ret = Volume::changeMount( val );
    y2mil("ret:" << ret);
    return( ret );
    }


    bool
    Partition::isWindows() const
    {
	return idt==ID_DOS16 || idt==0x0b || idt==ID_DOS32 || idt==0x0e || idt==ID_DOS12 || idt==0x04 ||
	    idt==ID_NTFS || idt==0x17;
    }


int
Partition::zeroIfNeeded() const
{
    int ret = 0;

    bool zero_new = getContainer()->getStorage()->getZeroNewPartitions();
    bool used_as_pv = isUsedBy(UB_LVM);

    y2mil("zero_new:" << zero_new << " used_as_pv:" << used_as_pv);

    if (zero_new || used_as_pv)
    {
       ret = getContainer()->getStorage()->zeroDevice(device(), sizeK());
    }

    return ret;
}


Text Partition::removeText( bool doing ) const
    {
    Text txt;
    string d = dev;
    if( orig_num!=num )
	{
	d = disk()->getPartDevice( orig_num );
	}
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	txt = sformat( _("Deleting partition %1$s"), d.c_str() );
	}
    else
	{
	if( isWindows() && sizeK()>700*1024 )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Delete Windows partition %1$s (%2$s)"), d.c_str(),
	                      sizeString().c_str() );
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Delete partition %1$s (%2$s)"), d.c_str(),
	                      sizeString().c_str() );
	    }
	}
    return( txt );
    }


Text
Partition::createText(bool doing) const
{
    Text txt;
    if (doing)
    {
	txt = Volume::createText(doing);
    }
    else
    {
	if (typ == EXTENDED)
	{
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat(_("Create extended partition %1$s (%2$s)"), dev.c_str(),
			  sizeString().c_str());
	}
	else
	{
	    txt = Volume::createText(doing);
	}
    }
    return txt;
}


Text Partition::formatText( bool doing ) const
    {
    Text txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting partition %1$s (%2$s) with %3$s "),
		       d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( !mp.empty() )
	    {
	    if( mp=="swap" )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Format partition %1$s (%2$s) for swap"),
			       d.c_str(), sizeString().c_str() );
		}
	    else if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format partition %1$s (%2$s) with %3$s"),
			   d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

Text Partition::resizeText( bool doing ) const
    {
    Text txt;
    string d = dev;
    if( doing )
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrinking partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extending partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	txt += Text(" ", " ");
	// text displayed during action
	txt += _("(progress bar might not move)");
	}
    else
        {
	if( isWindows() )
	    {
	    if( needShrink() )
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Shrink Windows partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	    else
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Extend Windows partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
    	    }
        else
            {
	    if( needShrink() )
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Shrink partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	    else
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Extend partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	    }
	}
    return( txt );
    }


void
Partition::getCommitActions(list<commitAction>& l) const
    {
    unsigned s = l.size();
    bool change_id = idt!=orig_id;
    Volume::getCommitActions( l );
    if( s<l.size() && change_id )
	change_id = false;
    if( change_id )
	{
	l.push_back(commitAction(INCREASE, cont->type(),
				 setTypeText(false), this, false));
	}
    }


    Region
    Partition::detectSysfsBlkRegion(bool log_error) const
    {
	string start_p = sysfsPath() + "/start";
	string size_p = sysfsPath() + "/size";

	unsigned long long start = 0;
	unsigned long long len = 0;
	read_sysfs_property(start_p, start, log_error);
	read_sysfs_property(size_p, len, log_error);

	return Region(start, len);
    }


void
Partition::getInfo( PartitionAddInfo& tinfo ) const
    {
    tinfo.partitionType = type ();
    tinfo.cylStart = cylStart ();
    tinfo.cylSize = cylSize ();
    tinfo.nr = num;
    tinfo.id = idt;
    tinfo.boot = bootflag;
    }

void
Partition::getInfo( PartitionInfo& tinfo ) const
    {
    Volume::getInfo(info.v);
    PartitionAddInfo tmp;
    getInfo( tmp );
    info = tmp;
    tinfo = info;
    }


PartitionInfo& PartitionInfo::operator=( const PartitionAddInfo& rhs )
    {
    nr = rhs.nr;
    cylStart = rhs.cylStart;
    cylSize = rhs.cylSize;
    partitionType = rhs.partitionType;
    id = rhs.id;
    boot = rhs.boot;
    return( *this );
    }

std::ostream& operator<< (std::ostream& s, const Partition &p )
    {
    s << "Partition " << dynamic_cast<const Volume&>(p)
      << " cylRegion:" << p.reg
      << " Id:" << std::hex << p.idt << std::dec;
    if( p.typ!=storage::PRIMARY )
	s << " " << toString(p.typ);
    if( p.orig_num!=p.num )
      s << " OrigNr:" << p.orig_num;
    if( p.orig_id!=p.idt )
      s << " OrigId:" << p.orig_id;
    if( p.bootflag )
      s << " boot";
    return( s );
    }


    void
    Partition::logDifference(std::ostream& log, const Partition& rhs) const
    {
	Volume::logDifference(log, rhs);

	logDiff(log, "reg", reg, rhs.reg);
	logDiffEnum(log, "type", typ, rhs.typ);
	logDiffHex(log, "id", idt, rhs.idt);
	logDiffHex(log, "orig_id", orig_id, rhs.orig_id);
	logDiff(log, "orig_num", orig_num, rhs.orig_num);
	logDiff(log, "boot", bootflag, rhs.bootflag);
    }


bool Partition::equalContent( const Partition& rhs ) const
    {
    return( Volume::equalContent(rhs) &&
            reg==rhs.reg && bootflag==rhs.bootflag && typ==rhs.typ && 
            idt==rhs.idt );
    }

}
