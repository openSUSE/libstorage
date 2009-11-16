/*
 * Copyright (c) [2004-2009] Novell, Inc.
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


Partition::Partition( const Disk& d, unsigned PNr, unsigned long long SizeK,
                      unsigned long Start, unsigned long CSize,
		      PartitionType Type, unsigned Id, bool Boot )
    : Volume( d, PNr, SizeK ), reg( Start, CSize )
    {
    bootflag = Boot;
    idt = orig_id = Id;
    typ = Type;
    orig_num = num;
    addUdevData();
    y2deb("constructed Partition " << dev << " on " << cont->device());
    }

Partition::Partition( const Disk& d, const string& Data ) :
        Volume( d, 0, 0 )
    {
    string ts, rs;
    istringstream i( Data );
    classic(i);
    i >> num >> dev >> size_k >> mjr >> mnr >> reg >>
	 hex >> idt >> dec >> ts >> rs;
    orig_size_k = size_k;
    orig_num = num;
    orig_id = idt;
    nm = dev;
    undevDevice(nm);	// strip "/dev/"
    if( ts == "extended" )
	typ = EXTENDED;
    else if( ts == "logical" )
	typ = LOGICAL;
    else
	typ = PRIMARY;
    if( rs == "boot" )
	bootflag = true;
    else
	bootflag = false;
    addUdevData();
    y2deb("constructed Partition " << dev << " on " << cont->device());
    }


    Partition::Partition(const Disk& c, const Partition& v)
	: Volume(c, v), reg(v.reg), bootflag(v.bootflag), typ(v.typ),
	  idt(v.idt), orig_id(v.orig_id), orig_num(v.orig_num)
    {
	y2deb("copy-constructed Partition from " << v.dev);
    }


    Partition::~Partition()
    {
	y2deb("destructed Partition " << dev);
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
	string tmp = undevDevice(dev);
	string ret = disk()->sysfsDir() + "/"  + boost::replace_all_copy(tmp, "/", "!");
	y2mil("ret:" << ret);
	return ret;
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
    y2mil( "dev:" << dev << " mby:" << mbyTypeString(mount_by) << 
           " orig:" << mbyTypeString(orig_mount_by) );
    mount_by = orig_mount_by = defaultMountBy();
    y2mil( "dev:" << dev << " mby:" << mbyTypeString(mount_by) << 
           " orig:" << mbyTypeString(orig_mount_by) );
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

	nm.clear();
	setNameDev();
	getMajorMinor( dev, mjr, mnr );
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

void Partition::changeRegion( unsigned long Start, unsigned long CSize,
			      unsigned long long SizeK )
    {
    reg = Region( Start, CSize );
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
    reg = Region( cylStart(), disk()->kbToCylinder(SizeK) );
    }

void Partition::forgetResize() 
    { 
    Volume::forgetResize();
    reg = Region( cylStart(), disk()->kbToCylinder(size_k) );
    }

bool Partition::operator== ( const Partition& rhs ) const
    {
    return( orig_num == rhs.orig_num &&
            num == rhs.num &&
            del == rhs.del );
    }

bool Partition::operator< ( const Partition& rhs ) const
    {
    if( orig_num!=rhs.orig_num )
	return( orig_num<rhs.orig_num );
    else
        return( !del );
    }

ostream& Partition::logData( ostream& file ) const
    {
    file << num << " " << dev << " " << size_k << " " <<  mjr << " "
         << mnr << " ";
    file << reg.start() << " " << reg.len() << " " << hex << idt << dec;
    if( typ == LOGICAL )
	file << " logical";
    else if( typ == EXTENDED )
	file << " extended";
    else
	file << " primary";
    if( bootflag )
	file << " boot";
    if( orig_num!=num )
	file << " OrigNr:" << orig_num;
    return( file );
    }

string Partition::setTypeText( bool doing ) const
    {
    string txt;
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
    y2mil("device:" << dev << " val:" << val << " fs:" << fs_names[new_fs]);
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

bool Partition::isWindows() const
    {
    return( idt==6 || idt==0xb || idt==ID_DOS || idt==0xe || idt==1 || idt==4 ||
	    idt==ID_NTFS || idt==0x17 );
    }

string Partition::removeText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( orig_num!=num )
	{
	d = disk()->getPartName( orig_num );
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

string Partition::createText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/hda1
	txt = sformat( _("Creating partition %1$s"), d.c_str() );
	}
    else
	{
	if( typ==EXTENDED )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create extended partition %1$s (%2$s)"),
	                   d.c_str(), sizeString().c_str() );
	    }
	else if( mp=="swap" )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create swap partition %1$s (%2$s)"),
	                   d.c_str(), sizeString().c_str() );
	    }
	else if( mp=="/" )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Create root partition %1$s (%2$s) with %3$s"),
	                   d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	    }
	else if (mp == getStorage()->bootMount())
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Create boot partition %1$s (%2$s) with %3$s"),
	                   d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	    }
	else if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create encrypted partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else if( idt != ID_SWAP && idt != ID_LINUX && idt<256 )
	{
	    if (encryption == ENC_NONE)
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by hexadecimal number (e.g. 8E)
		txt = sformat(_("Create partition %1$s (%2$s) with id=%3$X"), d.c_str(),
			      sizeString().c_str(), idt);
	    }
	    else
	    {   
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by hexadecimal number (e.g. 8E)
		txt = sformat(_("Create encrypted partition %1$s (%2$s) with id=%3$X"), d.c_str(),
			      sizeString().c_str(), idt);
	    }
	}
	else
	{
	    if (encryption == ENC_NONE)
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat(_("Create partition %1$s (%2$s)"), d.c_str(), sizeString().c_str());
	    }
	    else
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/hda1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat(_("Create encrypted partition %1$s (%2$s)"), d.c_str(),
			      sizeString().c_str());
	    }
	}
	}
    return txt;
    }

string Partition::formatText( bool doing ) const
    {
    string txt;
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

string Partition::resizeText( bool doing ) const
    {
    string txt;
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
	// text displayed during action
	txt += string(" ") + _("(progress bar might not move)");
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
	l.push_back(commitAction(INCREASE, cont->staticType(),
				 setTypeText(false), this, false));
	}
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
    tinfo.udevPath = udevPath();
    tinfo.udevId = boost::join(udevId(), " ");
    }

void
Partition::getInfo( PartitionInfo& tinfo ) const
    {
    ((Volume*)this)->getInfo( info.v );
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
    udevPath = rhs.udevPath;
    udevId = rhs.udevId;
    return( *this );
    }

std::ostream& operator<< (std::ostream& s, const Partition &p )
    {
    s << "Partition " << *(Volume*)&p
      << " Start:" << p.reg.start()
      << " CylNum:" << p.reg.len()
      << " Id:" << std::hex << p.idt << std::dec;
    if( p.typ!=storage::PRIMARY )
      s << " " << Partition::pt_names[p.typ];
    if( p.orig_num!=p.num )
      s << " OrigNr:" << p.orig_num;
    if( p.orig_id!=p.idt )
      s << " OrigId:" << p.orig_id;
    if( p.bootflag )
      s << " boot";
    return( s );
    }


void Partition::logDifference( const Partition& rhs ) const
    {
    string log = Volume::logDifference( rhs );
    if( reg.start()!=rhs.reg.start() )
	log += " Start:" + decString(reg.start()) + "-->" + 
	       decString(rhs.reg.start());
    if( reg.len()!=rhs.reg.len() )
	log += " CylNum:" + decString(reg.len()) + "-->" + 
	       decString(rhs.reg.len());
    if( typ!=rhs.typ )
	log += " Typ:" + pt_names[typ] + "-->" + pt_names[rhs.typ];
    if( idt!=rhs.idt )
	log += " Id:" + hexString(idt) + "-->" + hexString(rhs.idt);
    if( orig_id!=rhs.orig_id )
	log += " OrigId:" + hexString(orig_id) + "-->" + hexString(rhs.orig_id);
    if( orig_num!=rhs.orig_num )
	log += " OrigNr:" + decString(orig_num) + "-->" + decString(rhs.orig_num);
    if( bootflag!=rhs.bootflag )
	{
	if( rhs.bootflag )
	    log += " -->boot";
	else
	    log += " boot-->";
	}
    y2mil(log);
    }

bool Partition::equalContent( const Partition& rhs ) const
    {
    return( Volume::equalContent(rhs) &&
            reg==rhs.reg && bootflag==rhs.bootflag && typ==rhs.typ && 
            idt==rhs.idt );
    }


    const string Partition::pt_names[] = { "primary", "extended", "logical", "any" };

}
