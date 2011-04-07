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


#include <string>
#include <ostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <boost/algorithm/string.hpp>

#include "storage/Region.h"
#include "storage/Partition.h"
#include "storage/SystemInfo.h"
#include "storage/ProcParts.h"
#include "storage/Disk.h"
#include "storage/Storage.h"
#include "storage/AppUtil.h"
#include "storage/SystemCmd.h"
#include "storage/StorageDefines.h"
#include "storage/Parted.h"


namespace storage
{
    using namespace std;

    const unsigned int fuzz_cyl = 2;


    Disk::Disk(Storage* s, const string& name, const string& device,
	       unsigned long long SizeK, SystemInfo& systeminfo)
	: Container(s, name, device, staticType(), systeminfo),
	  init_disk(false), transport(TUNKNOWN), dmp_slave(false), no_addpart(false),
	  gpt_enlarge(false), del_ptable(false)
    {
    logfile_name = boost::replace_all_copy(nm, "/", "_");
    getMajorMinor();

    Lsscsi::Entry entry;
    if (systeminfo.getLsscsi().getEntry(device, entry))
	transport = entry.transport;

    size_k = SizeK;
    y2deb("constructed Disk name:" << name);
    }


    Disk::Disk(Storage* s, const string& name, const string& device, unsigned num,
	       unsigned long long SizeK, SystemInfo& systeminfo)
	: Container(s, name, device, staticType(), systeminfo),
	  init_disk(false), transport(TUNKNOWN), dmp_slave(false), no_addpart(false),
	  gpt_enlarge(false), del_ptable(false)
    {
    y2mil("constructed Disk name:" << name << " nr " << num << " sizeK:" << SizeK);
    logfile_name = name + decString(num);
    ronly = true;
    size_k = SizeK;
    addPartition( num, size_k, systeminfo );
    }


    Disk::Disk(Storage* s, const xmlNode* node)
	: Container(s, staticType(), node), label(), udev_path(),
	  udev_id(), max_primary(0), ext_possible(false), max_logical(0),
	  init_disk(false), transport(TUNKNOWN), dmp_slave(false), no_addpart(false),
	  gpt_enlarge(false), range(4), del_ptable(false)
    {
	logfile_name = nm;

	getChildValue(node, "range", range);

	getChildValue(node, "geometry", geometry);

	getChildValue(node, "label", label);
	getChildValue(node, "max_primary", max_primary);
	getChildValue(node, "ext_possible", ext_possible);
	getChildValue(node, "max_logical", max_logical);

	const list<const xmlNode*> l = getChildNodes(node, "partition");
	for (list<const xmlNode*>::const_iterator it = l.begin(); it != l.end(); ++it)
	    addToList(new Partition(*this, *it));

	y2deb("constructed Disk " << dev);
    }


    Disk::Disk(const Disk& c)
	: Container(c), geometry(c.geometry), new_geometry(c.new_geometry),
	  label(c.label), udev_path(c.udev_path), udev_id(c.udev_id),
	  detected_label(c.detected_label), logfile_name(c.logfile_name),
	  max_primary(c.max_primary),
	  ext_possible(c.ext_possible), max_logical(c.max_logical),
	  init_disk(c.init_disk), transport(c.transport),
	  dmp_slave(c.dmp_slave), no_addpart(c.no_addpart), 
	  gpt_enlarge(c.gpt_enlarge), range(c.range),
	  del_ptable(c.del_ptable)
    {
	y2deb("copy-constructed Disk " << dev);

	ConstPartPair p = c.partPair();
	for (ConstPartIter i = p.begin(); i != p.end(); ++i)
	{
	    Partition* p = new Partition(*this, *i);
	    vols.push_back(p);
	}
    }


    Disk::~Disk()
    {
	y2deb("destructed Disk " << dev);
    }


    void
    Disk::saveData(xmlNode* node) const
    {
	Container::saveData(node);

	setChildValue(node, "range", range);

	setChildValue(node, "geometry", geometry);

	setChildValue(node, "label", label);
	setChildValue(node, "max_primary", max_primary);
	if (ext_possible)
	{
	    setChildValue(node, "ext_possible", ext_possible);
	    setChildValue(node, "max_logical", max_logical);
	}

	if (!udev_path.empty())
	    setChildValue(node, "udev_path", udev_path);
	if (!udev_id.empty())
	    setChildValue(node, "udev_id", udev_id);

	if (transport != TUNKNOWN)
	    setChildValue(node, "transport", toString(transport));

	ConstPartPair vp = partPair();
	for (ConstPartIter v = vp.begin(); v != vp.end(); ++v)
	    v->saveData(xmlNewChild(node, "partition"));
    }


    string
    Disk::sysfsPath() const
    {
	return SYSFSDIR "/" + boost::replace_all_copy(procName(), "/", "!");
    }


void
Disk::triggerUdevUpdate() const
{
    ConstPartPair pp = partPair();
    for( ConstPartIter p=pp.begin(); p!=pp.end(); ++p )
    {
	if( !p->deleted() && !p->created() )
	    p->triggerUdevUpdate();
    }
}


void
Disk::setUdevData(const string& path, const list<string>& id)
{
    y2mil("disk:" << nm << " path:" << path << "id:" << id);
    udev_path = path;
    udev_id = id;
    udev_id.remove_if(string_starts_with("edd-"));
    partition(udev_id.begin(), udev_id.end(), string_starts_with("ata-"));
    y2mil("id:" << udev_id);

    alt_names.remove_if(string_contains("/by-path/"));
    if (!udev_path.empty())
	alt_names.push_back("/dev/disk/by-path/" + udev_path);

    alt_names.remove_if(string_contains("/by-id/"));
    for (list<string>::const_iterator i = udev_id.begin(); i != udev_id.end(); ++i)
	alt_names.push_back("/dev/disk/by-id/" + *i);

    PartPair pp = partPair();
    for( PartIter p=pp.begin(); p!=pp.end(); ++p )
	{
	p->addUdevData();
	}
}


    bool
    Disk::detect(SystemInfo& systeminfo)
    {
	return detectGeometry() && detectPartitions(systeminfo);
    }


bool Disk::detectGeometry()
    {
	return storage::detectGeometry(dev, geometry);
    }


    bool
    Disk::getSysfsInfo(const string& sysfsdir, SysfsInfo& sysfsinfo)
    {
	string devtype;
	if (read_sysfs_property(sysfsdir + "/device/devtype", devtype, false))
	{
	    sysfsinfo.vbd = devtype == "vbd";
	}
	else
	{
	    // not always available so no error
	    sysfsinfo.vbd = false;
	}

	if (!read_sysfs_property(sysfsdir + "/ext_range", sysfsinfo.range, false))
	    if (!read_sysfs_property(sysfsdir + "/range", sysfsinfo.range))
		return false;

	if (!read_sysfs_property(sysfsdir + "/size", sysfsinfo.size))
	    return false;

	y2mil("sysfsdir:" << sysfsdir << " devtype:" << devtype << " range:" << sysfsinfo.range <<
	      " size:" << sysfsinfo.size);

	return true;
    }


    bool
    Disk::getSysfsInfo()
    {
	bool ret = true;

	SysfsInfo sysfsinfo;
	if (getSysfsInfo(sysfsPath(), sysfsinfo))
	{
	    range = sysfsinfo.range;
	    if (range <= 1)
		ret = false;
	}
	else
	{
	    ret = false;
	}

	y2mil("dev:" << dev << " ret:" << ret << " range:" << range << " major:" << mjr <<
	      " minor:" << mnr);

	return ret;
    }


    bool
    Disk::detectPartitions(SystemInfo& systeminfo)
    {
	bool ret = true;

	const Parted& parted = systeminfo.getParted(dev);

	string dlabel = parted.getLabel();

	new_geometry = geometry = parted.getGeometry();

	gpt_enlarge = parted.getGptEnlarge();

	y2mil("dlabel:" << dlabel << " geometry:" << geometry << " gpt_enlarge:" << gpt_enlarge);

    if( dlabel!="loop" )
	{
	setLabelData( dlabel );
	checkPartedOutput(systeminfo);
	}
    else
	dlabel.erase();
    if( detected_label.empty() )
	detected_label = dlabel;
    if( dlabel.empty() )
	dlabel = defaultLabel();
    setLabelData( dlabel );

    if (label == "unsupported")
	{
	Text txt = sformat(
	// popup text %1$s is replaced by disk name e.g. /dev/hda
_("The partition table type on disk %1$s cannot be handled by\n"
"this tool.\n"
"\n"
"You can use the partitions on disk %1$s as they are.\n"
"You can format them and assign mount points to them, but you\n"
"cannot add, edit, resize, or remove partitions from that\n"
"disk with this tool."), dev.c_str() );
	y2war( "unsupported disk label on " << dev << " txt:" << txt.native );

	detected_label = label;
	ronly = true;
	}

    y2mil("ret:" << ret << " partitions:" << vols.size() << " detected label:" << detected_label <<
	  " label:" << label);
    return( ret );
    }


void
Disk::logData(const string& Dir) const
    {
    string fname(Dir + "/disk_" + logfile_name + ".info.tmp");

    XmlFile xml;
    xmlNode* node = xmlNewNode("disk");
    xml.setRootElement(node);
    saveData(node);
    xml.save(fname);

    getStorage()->handleLogFile( fname );
    }


    void
    Disk::setLabelData(const string& disklabel)
    {
	y2mil("disklabel:" << disklabel);
	DlabelCapabilities caps;
	if (getDlabelCapabilities(disklabel, caps))
	{
	    max_primary = min(caps.maxPrimary, (unsigned)(range - 1));
	    ext_possible = caps.extendedPossible;
	    max_logical = min(caps.maxLogical, (unsigned)(range - 1));
	    label = disklabel;
	}
	else
	{
	    max_primary = 0;
	    ext_possible = false;
	    max_logical = 0;
	    label = "unsupported";
	}
	y2mil("label:" << label << " max_primary:" << max_logical << " ext_possible:" <<
	      ext_possible << " max_logical:" << max_logical);
    }


bool
Disk::getDlabelCapabilities(const string& dlabel, DlabelCapabilities& dlabelcapabilities)
{
    bool ret = false;
    int i = 0;
    while (!labels[i].name.empty() && labels[i].name != dlabel)
    {
	i++;
    }
    if (!labels[i].name.empty())
    {
	ret = true;
	dlabelcapabilities.maxPrimary = labels[i].primary;
	dlabelcapabilities.extendedPossible = labels[i].extended;
	dlabelcapabilities.maxLogical = labels[i].logical;
	dlabelcapabilities.maxSectors = labels[i].max_sectors;
    }
    y2mil("dlabel:" << dlabel << " ret:" << ret);
    return ret;
}


int
Disk::checkSystemError( const string& cmd_line, const SystemCmd& cmd ) const
    {
    string tmp = boost::join(cmd.stderr(), "\n");
    if (!tmp.empty())
        {
	y2err("cmd:" << cmd_line);
	y2err("err:" << tmp);
        }
    tmp = boost::join(cmd.stdout(), "\n");
    if (!tmp.empty())
        {
	y2mil("cmd:" << cmd_line);
	y2mil("out:" << tmp);
        }
    int ret = cmd.retcode();
    if( ret!=0 && tmp.find( "kernel failed to re-read" )!=string::npos )
	{
	y2mil( "resetting retcode cmd " << ret << " of:" << cmd_line );
	ret = 0;
	}
    if( ret != 0 )
        {
	if( dmp_slave && tmp.empty() )
	    {
	    y2mil( "resetting retcode " << ret << " of:" << cmd_line );
	    ret = 0;
	    }
	else
	    y2err("retcode:" << cmd.retcode());
        }
    return( ret );
    }

int
Disk::execCheckFailed( const string& cmd_line, bool stop_hald )
    {
    static SystemCmd cmd;
    return( execCheckFailed( cmd, cmd_line, stop_hald ) );
    }

int Disk::execCheckFailed( SystemCmd& cmd, const string& cmd_line, 
                           bool stop_hald )
    {
    if( stop_hald )
	getStorage()->handleHald(true);
    cmd.execute( cmd_line );
    int ret = checkSystemError( cmd_line, cmd );
    if( ret!=0 )
	setExtError( cmd );
    if( stop_hald )
	getStorage()->handleHald(false);
    return( ret );
    }


bool
    Disk::checkPartedOutput(SystemInfo& systeminfo)
    {
	const ProcParts& parts = systeminfo.getProcParts();
	const Parted& parted = systeminfo.getParted(dev);

	unsigned long range_exceed = 0;
	list<Partition *> pl;

	for (Parted::const_iterator it = parted.getEntries().begin();
	     it != parted.getEntries().end(); ++it)
	{
	    if (it->num < range)
	    {
		unsigned long long s = cylinderToKb(it->cylRegion.len());
		Partition* p = new Partition(*this, getPartName(it->num), getPartDevice(it->num),
					     it->num, systeminfo, s, it->cylRegion, it->type,
					     it->id, it->boot);
		if (parts.getSize(p->procName(), s))
		{
		    if( s>0 && p->type() != EXTENDED )
			p->setSize( s );
		}
		pl.push_back( p );
	    }
	    else
		range_exceed = max(range_exceed, (unsigned long) it->num);
	}

    y2mil("nm:" << nm);
    if (!dmp_slave && !checkPartedValid(systeminfo, pl, range_exceed))
	{
	Text txt = sformat(
	// popup text %1$s is replaced by disk name e.g. /dev/hda
_("The partitioning on disk %1$s is not readable by\n"
"the partitioning tool parted, which is used to change the\n"
"partition table.\n"
"\n"
"You can use the partitions on disk %1$s as they are.\n"
"You can format them and assign mount points to them, but you\n"
"cannot add, edit, resize, or remove partitions from that\n"
"disk with this tool."), dev.c_str() );

	getStorage()->addInfoPopupText( dev, txt );
	ronly = true;
	}
    if( range_exceed>0 )
	{
	Text txt = sformat(
	// popup text %1$s is replaced by disk name e.g. /dev/hda
	//            %2$lu and %3$lu are replaced by numbers.
_("Your disk %1$s contains %2$lu partitions. The maximum number\n"
"of partitions that the kernel driver of the disk can handle is %3$lu.\n"
"Partitions numbered above %3$lu cannot be accessed."),
                              dev.c_str(), range_exceed, range-1 );
	txt += Text("\n", "\n");
	txt += 
	// popup text
_("You have the following options:\n"
"  - Repartition your system so that only the maximal allowed number\n"
"    of partitions is used. To repartition, use your existing operating\n"
"    system.\n"
"  - Use LVM since it can provide an arbitrary and flexible\n"
"    number of block devices partitions. This needs a repartition\n"
"    as well.");
	getStorage()->addInfoPopupText( dev, txt );
	}
    for( list<Partition*>::iterator i=pl.begin(); i!=pl.end(); ++i )
	{
	addToList( *i );
	}
    return( true );
    }


    list<string>
    Disk::partitionsKernelKnowns(const ProcParts& parts) const
    {
	// does not work for device-mapper based disks

	string reg = "^" + dev + partNaming(dev) + "[0-9]+" "$";
	list<string> ps = parts.getMatchingEntries(regex_matches(reg));
	y2mil("dev:" << dev << " reg:\"" << reg << "\" ps:" << ps);
	return ps;
    }


bool
Disk::checkPartitionsValid(SystemInfo& systeminfo, const list<Partition*>& pl) const
{
    const Parted& parted = systeminfo.getParted(dev);

    // It's allowed that the kernel sees more partitions than parted. This is
    // the case with BSD slices.

    for (list<Partition*>::const_iterator i = pl.begin(); i != pl.end(); ++i)
    {
	const Partition& p = **i;

	if (p.type() != EXTENDED)
	{
	    Parted::Entry entry;
	    if (parted.getEntry(p.nr(), entry))
	    {
		Region sec_parted = entry.secRegion;
		Region sec_kernel = p.detectSysfsBlkRegion() / (sectorSize() / 512);

		if (sec_parted != sec_kernel)
		{
		    y2err("region mismatch dev:" << dev << " nr:" << p.nr() << " sec_parted:" <<
			  sec_parted << " sec_kernel:" << sec_kernel);
		    return false;
		}
	    }
	}
    }

    // But if parted sees no disk the kernel must also see no disks.

    if (pl.empty())
    {
	const ProcParts& parts = systeminfo.getProcParts();
	list<string> ps = partitionsKernelKnowns(parts);

	if (!ps.empty())
	{
	    y2err("parted sees no partitions but kernel does");
	    return false;
	}
    }

    return true;
}


bool
Disk::checkPartedValid(SystemInfo& systeminfo, list<Partition*>& pl,
			   unsigned long& range_exceed) const
{
    bool ret = checkPartitionsValid(systeminfo, pl);

    if( !ret || label=="unsupported" )
	{
	range_exceed = 0;
	for( list<Partition*>::iterator i=pl.begin(); i!=pl.end(); i++ )
	    {
	    delete *i;
	    }
	pl.clear();
	unsigned long cyl_start = 1;
	const ProcParts& parts = systeminfo.getProcParts();
	list<string> ps = partitionsKernelKnowns(parts);
	for( list<string>::const_iterator i=ps.begin(); i!=ps.end(); i++ )
	    {
	    unsigned long cyl;
	    unsigned long long s;
	    pair<string,unsigned> pr = getDiskPartition( *i );
	    if (parts.getSize(*i, s))
		{
		cyl = kbToCylinder(s);
		if( pr.second!=0 && pr.second < range )
		    {
		    unsigned id = Partition::ID_LINUX;
		    PartitionType type = PRIMARY;
		    if( ext_possible )
			{
			if (2 * s == procExtendedBlks())
			    {
			    type = EXTENDED;
			    id = Partition::ID_EXTENDED;
			    }
			if( pr.second>max_primary )
			    {
			    type = LOGICAL;
			    }
			}
		    Partition *p = new Partition(*this, getPartName(pr.second), getPartDevice(pr.second),
						 pr.second, systeminfo, s, Region(cyl_start, cyl), type,
						 id);
		    pl.push_back( p );
		    }
		else if( pr.second>0 )
		    range_exceed = max( range_exceed, (unsigned long)pr.second );
		cyl_start += cyl;
		}
	    }
	}

    y2mil("ret:" << ret);
    return ret;
}


string
Disk::defaultLabel() const
{
    string ret = "msdos";

    if (isDasd())
    {
	ret = "dasd";
    }
    else
    {
	const ArchInfo& archinfo = getStorage()->getArchInfo();
	unsigned long long num_sectors = kbToSector(size_k);
	y2mil("num_sectors:" << num_sectors);

	ret = "msdos";
	if (archinfo.is_efiboot)
	    ret = "gpt";
	else if (num_sectors > (1ULL << 32) - 1)
	    ret = "gpt";
	else if (archinfo.arch == "ia64")
	    ret = "gpt";
	else if (archinfo.arch == "sparc")
	    ret = "sun";
	else if (archinfo.arch == "ppc" && archinfo.is_ppc_mac)
	    ret = "mac";
	else if (archinfo.arch == "ppc" && archinfo.is_ppc_pegasos)
	    ret = "amiga";
    }

    y2mil("ret:" << ret);
    return ret;
}


    const Disk::label_info Disk::labels[] = {
	{ "msdos", true, 4, 256, (1ULL << 32) - 1 },	// actually unlimited number of logical partitions
	{ "gpt", false, 128, 0, (1ULL << 48) - 1 },	// actually 64 bit but we cannot calculate with that,
							// 48 bit looks nice since it matches LBA48
	{ "bsd", false, 8, 0, (1ULL << 32) - 1 },
	{ "sun", false, 8, 0, (1ULL << 32) - 1 },
	{ "mac", false, 64, 0, (1ULL << 32) - 1 },
	{ "dasd", false, 3, 0, (1ULL << 32) - 1 },
	{ "aix", false, 0, 0, (1ULL << 32) - 1 },
	{ "amiga", false, 63, 0, (1ULL << 32) - 1 },
	{ "", false, 0, 0, 0 }
    };


Region Disk::usableCylRegion() const
{
    unsigned long long maxSectors = (1ULL << 32) - 1;
    DlabelCapabilities caps;
    if (getDlabelCapabilities(label, caps))
	maxSectors = caps.maxSectors;
    return Region(0, min(cylinders(), kbToCylinder(sectorToKb(maxSectors))));
}


    const string Disk::p_disks[] = { "cciss/", "ida/", "ataraid/", "etherd/", "rd/", "mmcblk[0-9]+",
				     "md[0-9]+" };


    bool
    Disk::needP(const string& disk)
    {
	for (unsigned i = 0; i < lengthof(p_disks); ++i)
	{
	    Regex rx("^(/dev/)?" + p_disks[i]);
	    if (rx.match(disk))
		return true;
	}
	return false;
    }


string
Disk::partNaming(const string& disk)
{
    // TODO: this is hackish
    if (boost::starts_with(disk, "/dev/mapper/"))
	return "_part";
    else if (needP(disk))
	return "p";
    else
	return "";
}


    string
    Disk::getPartName(unsigned nr) const
    {
	return nm + partNaming(dev) + decString(nr);
    }


    string
    Disk::getPartDevice(unsigned nr) const
    {
	return dev + partNaming(dev) + decString(nr);
    }


pair<string,unsigned> Disk::getDiskPartition( const string& dev )
    {
    static Regex prx( "[0123456789]p[0123456789]+$" );
    static Regex partrx( "[-_]part[0123456789]+$" );
    unsigned nr = 0;
    string disk = dev;
    bool need_p = prx.match( dev );
    bool part = partrx.match( dev );
    string::size_type p = dev.find_last_not_of( "0123456789" );
    if( p != string::npos && p<dev.size() && (!need_p||dev[p]=='p') &&
	(dev.find("/disk/by-")==string::npos||part) )
	{
	dev.substr(p+1) >> nr;
	unsigned pos = p+1;
	if( need_p )
	    pos--;
	else if( part )
	    pos -= 5;
	disk = dev.substr( 0, pos );
	}
    y2mil( "dev:" << dev << " disk:" << disk << " nr:" << nr );
    return make_pair(disk, nr);
    }


static bool notDeletedPri( const Partition& p )
{
    return !p.deleted() && p.type()==PRIMARY;
}

static bool notDeletedExt( const Partition& p )
{
    return !p.deleted() && p.type()==EXTENDED;
}

static bool notDeletedLog( const Partition& p )
{
    return !p.deleted() && p.type()==LOGICAL;
}


unsigned int Disk::numPrimary() const
{
    return partPair(notDeletedPri).length();
}

bool Disk::hasExtended() const
{
    return ext_possible && !partPair(notDeletedExt).empty();
}

unsigned int Disk::numLogical() const
{
    return partPair(notDeletedLog).length();
}


unsigned
Disk::availablePartNumber(PartitionType type) const
{
    y2mil("begin name:" << name() << " type:" << toString(type));
    unsigned ret = 0;
    ConstPartPair p = partPair(Partition::notDeleted);
    if( !ext_possible && type==LOGICAL )
	{
	ret = 0;
	}
    else if( p.empty() )
	{
	if( type==LOGICAL )
	    ret = max_primary + 1;
	else
	    ret = label!="mac" ? 1 : 2;
	}
    else if( type==LOGICAL )
	{
	unsigned mx = max_primary;
	ConstPartIter i=p.begin();
	while( i!=p.end() )
	    {
	    y2mil( "i:" << *i );
	    if( i->nr()>mx )
		mx = i->nr();
	    ++i;
	    }
	ret = mx+1;
	if( !ext_possible || !hasExtended() || ret>max_logical )
	    ret = 0;
	}
    else
	{
	ConstPartIter i=p.begin();
	unsigned start = label!="mac" ? 1 : 2;
	while( i!=p.end() && i->nr()<=start && i->nr()<=max_primary )
	    {
	    if( i->nr()==start )
		start++;
	    if( label=="sun" && start==3 )
		start++;
	    ++i;
	    }
	if( start<=max_primary )
	    ret = start;
	if( type==EXTENDED && (!ext_possible || hasExtended()))
	    ret = 0;
	}

    if( ret >= range )
	ret = 0;

    y2mil("ret:" << ret);
    return ret;
}


static bool notDeletedNotLog( const Partition& p )
    {
    return( !p.deleted() && p.type()!=LOGICAL );
    }

static bool existingNotLog( const Partition& p )
    {
    return( !p.deleted() && !p.created() && p.type()!=LOGICAL );
    }

static bool existingLog( const Partition& p )
    {
    return( !p.deleted() && !p.created() && p.type()==LOGICAL );
    }

static bool notCreatedPrimary( const Partition& p )
    {
    return( !p.created() && p.type()==PRIMARY );
    }


void
Disk::getUnusedSpace(list<Region>& free, bool all, bool logical) const
{
    y2mil("all:" << all << " logical:" << logical);

    free.clear();

    if (all || !logical)
    {
	ConstPartPair p = partPair(notDeletedNotLog);

	unsigned long start = 0;
	unsigned long end = cylinders();

	list<Region> tmp;
	for (ConstPartIter i = p.begin(); i != p.end(); ++i)
	    tmp.push_back(i->cylRegion());
	tmp.sort();

	for (list<Region>::const_iterator i = tmp.begin(); i != tmp.end(); ++i)
	{
	    if (i->start() > start)
		free.push_back(Region(start, i->start() - start));
	    start = i->end() + 1;
	}
	if (end > start)
	    free.push_back(Region(start, end - start));
    }

    if (all || logical)
    {
	ConstPartPair ext = partPair(notDeletedExt);
	if (!ext.empty())
	{
	    ConstPartPair p = partPair(notDeletedLog);

	    unsigned long start = ext.begin()->cylStart();
	    unsigned long end = ext.begin()->cylEnd();

	    list<Region> tmp;
	    for (ConstPartIter i = p.begin(); i != p.end(); ++i)
		tmp.push_back(i->cylRegion());
	    tmp.sort();

	    for (list<Region>::const_iterator i = tmp.begin(); i != tmp.end(); ++i)
	    {
		if (i->start() > start)
		    free.push_back(Region(start, i->start() - start));
		start = i->end() + 1;
	    }
	    if (end > start)
		free.push_back(Region(start, end - start));
	}
    }

    y2deb("free:" << free);

    Region usable_region = usableCylRegion();

    list<Region>::iterator i = free.begin();
    while (i != free.end())
    {
	if (usable_region.doIntersect(*i))
	{
	    *i = usable_region.intersect(*i);
	    ++i;
	}
	else
	{
	    i = free.erase(i);
	}
    }

    y2deb("free:" << free);
}


static bool regions_sort_size( const Region& rhs, const Region& lhs )
    {
    return( rhs.len()>lhs.len() );
    }

int Disk::createPartition( unsigned long cylLen, string& device,
			   bool checkRelaxed )
    {
    y2mil("len:" << cylLen << " relaxed:" << checkRelaxed);
    getStorage()->logCo( this );
    int ret = 0;
    list<Region> free;
    getUnusedSpace( free );
    y2mil("free:");
    if( !free.empty() )
	{
	free.sort( regions_sort_size );
	list<Region>::const_iterator i = free.begin();
	while( i!=free.end() && i->len()>=cylLen )
	    ++i;
	--i;
	if( i->len()>=cylLen )
	    {
	    ConstPartPair ext = partPair(notDeletedExt);
	    PartitionType t = PRIMARY;
	    bool usable = false;
	    do
		{
		t = PRIMARY;
		if( !ext.empty() && ext.begin()->contains( *i ) )
		    t = LOGICAL;
		usable = availablePartNumber(t)>0;
		if( !usable && i!=free.begin() )
		    --i;
		}
	    while( i!=free.begin() && !usable );
	    usable = availablePartNumber(t)>0;
	    if( usable )
		ret = createPartition( t, i->start(), cylLen, device,
				       checkRelaxed );
	    else
		ret = DISK_PARTITION_NO_FREE_NUMBER;
	    }
	else
	    ret = DISK_CREATE_PARTITION_NO_SPACE;
	}
    else
	ret = DISK_CREATE_PARTITION_NO_SPACE;
    getStorage()->logCo( this );
    y2mil("ret:" << ret);
    return( ret );
    }

int Disk::createPartition( PartitionType type, string& device )
    {
    y2mil("type " << toString(type));
    int ret = 0;
    list<Region> free;
    getUnusedSpace( free, type==PTYPE_ANY, type==LOGICAL );
    if( !free.empty() )
	{
	free.sort( regions_sort_size );
	list<Region>::const_iterator i = free.begin();
	ConstPartPair ext = partPair(notDeletedExt);
	PartitionType t = type;
	bool usable = false;
	do
	    {
	    t = PRIMARY;
	    if( !ext.empty() && ext.begin()->contains( *i ) )
		t = LOGICAL;
	    usable = t==type || type==PTYPE_ANY || (t==PRIMARY&&type==EXTENDED);
	    usable = usable && availablePartNumber(t)>0;
	    if( !usable && i!=free.end() )
		++i;
	    }
	while( i!=free.end() && !usable );
	usable = availablePartNumber(t)>0;
	if( usable )
	    ret = createPartition( type==PTYPE_ANY?t:type, i->start(),
	                           i->len(), device, true );
	else
	    ret = DISK_PARTITION_NO_FREE_NUMBER;
	}
    else
	ret = DISK_CREATE_PARTITION_NO_SPACE;
    y2mil("ret:" << ret);
    return( ret );
    }


int
Disk::nextFreePartition(PartitionType type, unsigned& nr, string& device) const
{
    int ret = 0;
    device = "";
    nr = availablePartNumber( type );
    if (nr == 0)
	{
	ret = DISK_PARTITION_NO_FREE_NUMBER;
	}
    else
	{
	device = getPartDevice(nr);
	}
    y2mil("ret:" << ret << " nr:" << nr << " device:" << device);
    return ret;
}


int Disk::createPartition( PartitionType type, unsigned long start,
                           unsigned long len, string& device,
			   bool checkRelaxed )
    {
    y2mil("begin type " << toString(type) << " at " << start << " len " << len << " relaxed:" <<
	  checkRelaxed);
    getStorage()->logCo( this );
    int ret = createChecks(type, Region(start, len), checkRelaxed);
    unsigned number = 0;
    if( ret==0 )
	{
	number = availablePartNumber( type );
	if( number==0 )
	    {
	    ret = DISK_PARTITION_NO_FREE_NUMBER;
	    }
	}
    if( ret==0 )
	{
	if( label=="sun" && start==0 )
	    start=1;
	Partition * p = new Partition(*this, getPartName(number), getPartDevice(number), number,
				      cylinderToKb(len), Region(start, len), type);
	ConstPartPair pp = partPair();
	ConstPartIter i = pp.begin();
	while( i!=pp.end() && !(i->deleted() && i->cylStart()==start) )
	    ++i;
	if( i!=pp.end() )
	    {
	    y2mil( "deleted at same start:" << *i );
	    p->getFsInfo( &(*i) );
	    }

	// see bnc #591075
	if (getStorage()->instsys())
	{
	    for (i = pp.begin(); i != pp.end(); ++i)
	    {
		if (i->deleted() && i->nr() == number && !i->getCryptPwd().empty())
		{
		    y2mil("harvesting old password");
		    p->setCryptPwd(i->getCryptPwd());
		}
	    }
	}

	p->setCreated();
	device = p->device();
	addToList( p );
	}
    getStorage()->logCo( this );
    y2mil("ret:" << ret << " device:" << (ret==0?device:""));
    return( ret );
    }


    int
    Disk::createChecks(PartitionType& type, const Region& cylRegion, bool checkRelaxed) const
    {
    y2mil("type:" << toString(type) << " cylRegion:" << cylRegion << " relaxed:" <<
	  checkRelaxed);
    unsigned fuzz = checkRelaxed ? fuzz_cyl : 0;
    int ret = 0;
    ConstPartPair ext = partPair(notDeletedExt);
    if( type==PTYPE_ANY )
	{
	if( ext.empty() || !ext.begin()->contains( Region(cylRegion.start(), 1) ))
	    type = PRIMARY;
	else
	    type = LOGICAL;
	}

    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    if( ret==0 && (cylRegion.end() > cylinders()+fuzz) )
	{
	y2mil("too large for disk cylinders " << cylinders());
	ret = DISK_PARTITION_EXCEEDS_DISK;
	}
    if( ret==0 && cylRegion.empty() )
	{
	ret = DISK_PARTITION_ZERO_SIZE;
	}
    if( ret==0 && type==LOGICAL && ext.empty() )
	{
	ret = DISK_CREATE_PARTITION_LOGICAL_NO_EXT;
	}
    if( ret==0 )
	{
	ConstPartPair p = (type != LOGICAL) ? partPair(Partition::notDeleted) : partPair(notDeletedLog);
	ConstPartIter i = p.begin();
	while( i!=p.end() && !i->intersectArea( cylRegion, fuzz ))
	    {
	    ++i;
	    }
	if( i!=p.end() )
	    {
	    y2war("overlaps r:" << cylRegion << " p:" << i->cylRegion() <<
		  " inter:" << i->cylRegion().intersect(cylRegion) );
	    ret = DISK_PARTITION_OVERLAPS_EXISTING;
	    }
	}
    if( ret==0 && type==LOGICAL && !ext.begin()->contains( cylRegion, fuzz ))
	{
	y2war("outside ext r:" << cylRegion << " ext:" << ext.begin()->cylRegion() <<
	      "inter:" << ext.begin()->cylRegion().intersect(cylRegion) );
	ret = DISK_PARTITION_LOGICAL_OUTSIDE_EXT;
	}
    if( ret==0 && type==EXTENDED )
	{
	if( !ext_possible || !ext.empty())
	    {
	    ret = ext_possible ? DISK_CREATE_PARTITION_EXT_ONLY_ONCE
	                       : DISK_CREATE_PARTITION_EXT_IMPOSSIBLE;
	    }
	}
    y2mil("ret:" << ret);
    return ret;
    }


    int
    Disk::changePartitionArea(unsigned nr, const Region& cylRegion, bool checkRelaxed)
    {
    y2mil("begin nr:" << nr << " cylRegion:" << cylRegion << " relaxed:" << checkRelaxed);
    int ret = 0;
    unsigned fuzz = checkRelaxed ? fuzz_cyl : 0;
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    PartPair p = partPair(Partition::notDeleted);
    PartIter part = p.begin();
    while( ret==0 && part!=p.end() && part->nr()!=nr)
	{
	++part;
	}
    if( ret==0 && part==p.end() )
	{
	ret = DISK_PARTITION_NOT_FOUND;
	}
    if( ret==0 && cylRegion.end() > cylinders()+fuzz )
	{
	y2mil("too large for disk cylinders " << cylinders());
	ret = DISK_PARTITION_EXCEEDS_DISK;
	}
    if( ret==0 && cylRegion.empty() )
	{
	ret = DISK_PARTITION_ZERO_SIZE;
	}
    if( ret==0 && part->type()==LOGICAL )
	{
	ConstPartPair ext = partPair(notDeletedExt);
	p = partPair( notDeletedLog );
	ConstPartIter i = p.begin();
	while( i!=p.end() && (i==part||!i->intersectArea( cylRegion, fuzz )) )
	    {
	    ++i;
	    }
	if( i!=p.end() )
	    {
	    y2war("overlaps r:" << cylRegion << " p:" << i->cylRegion() <<
		  " inter:" << i->cylRegion().intersect(cylRegion) );
	    ret = DISK_PARTITION_OVERLAPS_EXISTING;
	    }
	if( ret==0 && !ext.begin()->contains( cylRegion, fuzz ))
	    {
	    y2war("outside ext r:" <<  cylRegion << " ext:" << ext.begin()->cylRegion() <<
		  "inter:" << ext.begin()->cylRegion().intersect(cylRegion) );
	    ret = DISK_PARTITION_LOGICAL_OUTSIDE_EXT;
	    }
	}
    if( ret==0 && part->type()!=LOGICAL )
	{
	ConstPartIter i = p.begin();
	while( i!=p.end() &&
	       (i==part || i->nr()>max_primary || !i->intersectArea( cylRegion, fuzz )))
	    {
	    ++i;
	    }
	if( i!=p.end() )
	    {
	    y2war("overlaps r:" << cylRegion << " p:" << i->cylRegion() <<
		  " inter:" << i->cylRegion().intersect(cylRegion) );
	    ret = DISK_PARTITION_OVERLAPS_EXISTING;
	    }
	}
    if( ret==0 )
	{
	part->changeRegion(cylRegion, cylinderToKb(cylRegion.len()));
	}
    y2mil("ret:" << ret);
    return( ret );
    }

static bool volume_ptr_sort_nr( Partition*& rhs, Partition*& lhs )
    {
    return( rhs->nr()<lhs->nr() );
    }

int Disk::removePartition( unsigned nr )
    {
    y2mil("begin nr " << nr);
    getStorage()->logCo( this );
    int ret = 0;
    PartPair p = partPair(Partition::notDeleted);
    PartIter i = p.begin();
    while( i!=p.end() && i->nr()!=nr)
	{
	++i;
	}
    if( i==p.end() )
	{
	ret = DISK_PARTITION_NOT_FOUND;
	}
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    else if (i->isUsedBy())
	{
	ret = DISK_REMOVE_USED_BY;
	}
    if( ret==0 )
	{
	PartitionType t = i->type();
	if( i->created() )
	    {
	    if( !removeFromList( &(*i) ))
		ret = DISK_REMOVE_PARTITION_CREATE_NOT_FOUND;
	    p = partPair(Partition::notDeleted);
	    }
	else
	    i->setDeleted();
	if( ret==0 && nr>max_primary )
	    {
	    changeNumbers( p.begin(), p.end(), nr, -1 );
	    }
	else if( ret==0 && nr<=max_primary )
	    {
	    list<Partition*> l;
	    i = p.begin();
	    while( i!=p.end() )
		{
		if( i->created() && i->nr()<=max_primary && i->nr()>nr )
		    {
		    l.push_back( &(*i) );
		    }
		++i;
		}
	    if( !l.empty() )
		{
		l.sort( volume_ptr_sort_nr );
		unsigned old = nr;
		list<Partition*>::iterator vi = l.begin();
		while( vi!=l.end() )
		    {
		    unsigned save = (*vi)->nr();
		    (*vi)->changeNumber( old );
		    old = save;
		    ++vi;
		    }
		}
	    }
	if( t==EXTENDED )
	    {
	    list<Volume*> l;
	    i = p.begin();
	    while( i!=p.end() )
		{
		if( i->nr()>max_primary )
		    {
		    if( i->created() )
			l.push_back( &(*i) );
		    else
			i->setDeleted();
		    }
		++i;
		}
	    list<Volume*>::iterator vi = l.begin();
	    while( ret==0 && vi!=l.end() )
		{
		if( !removeFromList( *vi ))
		    ret = DISK_PARTITION_NOT_FOUND;
		++vi;
		}
	    }
	}
    getStorage()->logCo( this );
    y2mil("ret:" << ret);
    return( ret );
    }

void Disk::changeNumbers( const PartIter& b, const PartIter& e,
                          unsigned start, int incr )
    {
    y2mil("start:" << start << " incr:" << incr);
    PartIter i(b);
    while( i!=e )
	{
	if( i->nr()>start )
	    {
	    i->changeNumber( i->nr()+incr );
	    }
	++i;
	}
    }

int Disk::destroyPartitionTable( const string& new_label )
    {
    y2mil("begin");
    int ret = 0;
    setLabelData( new_label );
    if( max_primary==0 )
	{
	setLabelData( label );
	ret = DISK_DESTROY_TABLE_INVALID_LABEL;
	}
    else
	{
	label = new_label;
	VIter j = vols.begin();
	while( j!=vols.end() )
	    {
	    if( (*j)->created() )
		{
		delete( *j );
		j = vols.erase( j );
		}
	    else
		++j;
	    }
	bool save = getStorage()->getRecursiveRemoval();
	getStorage()->setRecursiveRemoval(true);
	if (isUsedBy())
	    {
	    getStorage()->removeUsing( device(), getUsedBy() );
	    }
	ronly = false;
	RVIter i = vols.rbegin();
	while( i!=vols.rend() && !dmp_slave )
	    {
	    if( !(*i)->deleted() )
		getStorage()->removeVolume( (*i)->device() );
	    ++i;
	    }
	getStorage()->setRecursiveRemoval(save);
	del_ptable = true;
	}
    y2mil("ret:" << ret);
    return( ret );
    }


int
Disk::changePartitionId(unsigned nr, unsigned id)
{
    y2mil("begin nr:" << nr << " id:" << hex << id);
    int ret = 0;
    PartPair p = partPair(Partition::notDeleted);
    PartIter i = p.begin();
    while( i!=p.end() && i->nr()!=nr)
	{
	++i;
	}
    if( i==p.end() )
	{
	ret = DISK_PARTITION_NOT_FOUND;
	}
    if( readonly() )
	{
	y2war( "trying to chang partition id on readonly disk - ignoring" );
	i->changeIdDone();
	ret = 0;
	}
    else
	{
	ret = i->changeId( id );
	}
    y2mil("ret:" << ret);
    return ret;
    }


int Disk::forgetChangePartitionId( unsigned nr )
    {
    y2mil("begin nr:" << nr);
    int ret = 0;
    PartPair p = partPair(Partition::notDeleted);
    PartIter i = p.begin();
    while( i!=p.end() && i->nr()!=nr)
	{
	++i;
	}
    if( i==p.end() )
	{
	ret = DISK_PARTITION_NOT_FOUND;
	}
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	i->unChangeId();
	}
    y2mil("ret:" << ret);
    return( ret );
    }


void
Disk::getToCommit(CommitStage stage, list<const Container*>& col, list<const Volume*>& vol) const
    {
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    Container::getToCommit( stage, col, vol );
    if( stage==INCREASE )
	{
	ConstPartPair p = partPair( Partition::toChangeId );
	for( ConstPartIter i=p.begin(); i!=p.end(); ++i )
	    if( find( vol.begin(), vol.end(), &(*i) )==vol.end() )
		vol.push_back( &(*i) );
	}
    if( del_ptable && find( col.begin(), col.end(), this )==col.end() )
	col.push_back( this );
    if( col.size()!=oco || vol.size()!=ovo )
	y2mil("stage:" << stage << " col:" << col.size() << " vol:" << vol.size());
    }

int Disk::commitChanges( CommitStage stage, Volume* vol )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = Container::commitChanges( stage, vol );
    if( ret==0 && stage==INCREASE )
	{
	Partition * p = dynamic_cast<Partition *>(vol);
	if( p!=NULL )
	    {
	    if( Partition::toChangeId( *p ) )
		ret = doSetType( p );
	    }
	else
	    ret = DISK_SET_TYPE_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Disk::commitChanges( CommitStage stage )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = 0;
    if( stage==DECREASE && del_ptable )
	{
	ret = doCreateLabel();
	}
    else
	ret = DISK_COMMIT_NOTHING_TODO;
    y2mil("ret:" << ret);
    return( ret );
    }


void
Disk::getCommitActions(list<commitAction>& l) const
    {
    Container::getCommitActions( l );
    if( del_ptable )
	{
	l.remove_if(stage_is(DECREASE));
	l.push_front(commitAction(DECREASE, staticType(), setDiskLabelText(false), this, true));
	}
    }


    Text
    Disk::setDiskLabelText(bool doing) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by disk name (e.g. /dev/hda),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat(_("Setting disk label of %1$s to %2$s"), dev.c_str(),
		      boost::to_upper_copy(label).c_str());
        }
    else
        {
        // displayed text before action, %1$s is replaced by disk name (e.g. /dev/hda),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat(_("Set disk label of %1$s to %2$s"), dev.c_str(),
		      boost::to_upper_copy(label).c_str());
        }
    return txt;
    }


int Disk::doCreateLabel()
    {
    y2mil("label:" << label);
    int ret = 0;
    if( !silent )
	{
	getStorage()->showInfoCb( setDiskLabelText(true) );
	}
    if( !dmp_slave )
	getStorage()->removeDmMapsTo( device() );
    removePresentPartitions();
    std::ostringstream cmd_line;
    classic(cmd_line);
    cmd_line << MDADMBIN " --zero-superblock --force " << quote(device());
    execCheckFailed( cmd_line.str() );
    cmd_line.str("");
    cmd_line << PARTEDCMD << quote(device()) << " mklabel " << label;
    if( execCheckFailed( cmd_line.str() ) )
	{
	ret = DISK_SET_LABEL_PARTED_FAILED;
	}
    else
	{
	del_ptable = false;
	removeFromMemory();
	}
    if( ret==0 )
	{
	if( !dmp_slave )
	    Storage::waitForDevice();
	redetectGeometry();
	}
    gpt_enlarge = false;
    y2mil("ret:" << ret);
    return( ret );
    }

void Disk::removePresentPartitions()
    {
    VolPair p = volPair();
    if( !p.empty() )
	{
	bool save=silent;
	setSilent( true );
	list<VolIterator> l;
	for( VolIterator i=p.begin(); i!=p.end(); ++i )
	    {
	    y2mil( "rem:" << *i );
	    if( !i->created() )
		l.push_front( i );
	    }
	for( list<VolIterator>::const_iterator i=l.begin(); i!=l.end(); ++i )
	    {
	    doRemove( &(**i) );
	    }
	setSilent( save );
	}
    }

void Disk::removeFromMemory()
    {
    VIter i = vols.begin();
    while( i!=vols.end() )
	{
	y2mil( "rem:" << *i );
	if( !(*i)->created() )
	    {
	    delete *i;
	    i = vols.erase( i );
	    }
	else
	    ++i;
	}
    }


    void
    Disk::redetectGeometry()
    {
	Parted parted(device());
	Geometry tmp_geometry = parted.getGeometry();
	if (tmp_geometry != geometry)
	{
	    new_geometry = tmp_geometry;
	    y2mil("new parted geometry " << new_geometry);
	}
    }


int Disk::doSetType( Volume* v )
    {
    y2mil("doSetType container " << name() << " name " << v->name());
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->setTypeText(true) );
	    }
	if( p->id()!=Partition::ID_LINUX && p->id()!=Partition::ID_SWAP )
	    p->eraseLabel();

	std::ostringstream options;
	classic(options);

	if( ret==0 )
	{
	    options << " set " << p->nr() << " lvm " << (p->id()==Partition::ID_LVM ? "on" : "off");

	    if (label != "sun")
		options << " set " << p->nr() << " raid " << (p->id()==Partition::ID_RAID?"on":"off");

	    if (label == "dvh" || label == "mac")
		options << " set " << p->nr() << " swap " << (p->id()==Partition::ID_SWAP?"on":"off");

	    options << " set " << p->nr() << " boot " << ((p->boot()||p->id()==Partition::ID_GPT_BOOT)?"on":"off");

	    if (p->id() <= 255 && label == "msdos")
		options << " set " << p->nr() << " type " << p->id();
	}

	if (!options.str().empty())
	{
	    string cmd_line = PARTEDCMD + quote(device()) + options.str();
	    if( execCheckFailed( cmd_line ) && !dmp_slave )
	    {
		ret = DISK_SET_TYPE_PARTED_FAILED;
	    }
	}

	if( ret==0 )
	    {
	    if( !dmp_slave )
		Storage::waitForDevice(p->device());
	    p->changeIdDone();
	    }
	}
    else
	{
	ret = DISK_SET_TYPE_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }


    bool
    Disk::callDelpart(unsigned nr) const
    {
	SystemCmd c(DELPARTBIN " " + quote(device()) + ' ' + decString(nr));
	return c.retcode() == 0;
    }


    bool
    Disk::callAddpart(unsigned nr, const Region& blkRegion) const
    {
	SystemCmd c(ADDPARTBIN " " + quote(device()) + ' ' + decString(nr) + ' ' +
		    decString(blkRegion.start()) + ' ' + decString(blkRegion.len()));
	return c.retcode() == 0;
    }


unsigned long long
Disk::procExtendedBlks() const
{
    return std::max(2U, sectorSize() / 512);
}


bool
Disk::getPartedValues( Partition *p ) const
    {
    y2mil( "nr:" << p->nr() );
    bool ret = false;
    if (getStorage()->testmode())
	{
	ret = true;
	p->setSize( p->sizeK() );
	}
    else
	{
	Parted parted(device());
	Parted::Entry entry;
	if (parted.getEntry(p->nr(), entry))
	{
	    Region partedBlkRegion = sectorSize() / 512 * entry.secRegion;
	    y2mil("partedBlkRegion:" << partedBlkRegion);

	    if (p->type() == EXTENDED)
		partedBlkRegion.setLen(procExtendedBlks());

	    Region sysfsBlkRegion = p->detectSysfsBlkRegion(false);
	    y2mil("sysfsBlkRegion:" << sysfsBlkRegion);

	    if (!no_addpart && partedBlkRegion != sysfsBlkRegion)
	    {
		callDelpart(p->nr());
		callAddpart(p->nr(), partedBlkRegion);

		sysfsBlkRegion = p->detectSysfsBlkRegion();

		if (partedBlkRegion != sysfsBlkRegion)
		    y2err("addpart failed sysfsBlkRegion:" << sysfsBlkRegion);
	    }

	    const Region& partedCylRegion = entry.cylRegion;
	    y2mil("partedCylRegion:" << partedCylRegion);
	    p->changeRegion(partedCylRegion, cylinderToKb(partedCylRegion.len()));

	    ret = true;

	    if( !dmp_slave && p->type() != EXTENDED )
		{
		ProcParts parts;
		unsigned long long s = 0;
		if (!parts.getSize(p->procName(), s) || s == 0)
		    {
		    y2err("device " << p->device() << " not found in /proc/partitions");
		    ret = false;
		    }
		else
		    p->setSize( s );
		}
	    }
	}
    return( ret );
    }

bool
Disk::getPartedSectors( const Partition *p, unsigned long long& start,
                        unsigned long long& end ) const
    {
    bool ret = false;
    if (getStorage()->testmode())
	{
	ret = true;
	start = p->cylStart() * new_geometry.heads * new_geometry.sectors;
	end = (p->cylEnd() + 1) * new_geometry.heads * new_geometry.sectors - 1;
	}
    else
	{
	Parted parted(device());
	Parted::Entry entry;
	if (parted.getEntry(p->nr(), entry))
	{
	    start = entry.secRegion.start();
	    end = entry.secRegion.end();
	    ret = true;
	}
	}
    return( ret );
    }


void Disk::enlargeGpt()
    {
    y2mil( "gpt_enlarge:" << gpt_enlarge );
    if( gpt_enlarge )
	{
	string cmd_line( "yes Fix | " PARTEDBIN );
	cmd_line += " ---pretend-input-tty ";
	cmd_line += quote(device());
	cmd_line += " print ";
	SystemCmd cmd( cmd_line );
	gpt_enlarge = false;
	}
    }

static bool logicalCreated( const Partition& p )
    { return( p.type()==LOGICAL && p.created() ); }


int Disk::doCreate( Volume* v )
    {
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	bool call_blockdev = false;
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->createText(true) );
	    }
	y2mil("doCreate container " << name() << " name " << p->name());
	y2mil("doCreate nr:" << p->nr() << " start " << p->cylStart() << " len " << p->cylSize());
	y2mil("doCreate detected_label:" << detected_label << " label:" << label);
	if( detected_label != label )
	    {
	    ret = doCreateLabel();
	    if( ret==0 )
		detected_label = label;
	    }
	if( ret==0 && gpt_enlarge )
	    {
	    enlargeGpt();
	    }
	std::ostringstream cmd_line;
	classic(cmd_line);
	if( ret==0 )
	    {
	    cmd_line << PARTEDCMD;
	    cmd_line << "--align=" << toString(getStorage()->getPartitionAlignment()) << " ";
	    cmd_line << quote(device()) << " unit cyl mkpart ";
	    if( label != "sun" )
		{
		switch( p->type() )
		    {
		    case LOGICAL:
			cmd_line << "logical ";
			break;
		    case PRIMARY:
			cmd_line << "primary ";
			break;
		    case EXTENDED:
			cmd_line << "extended ";
			break;
		    default:
			ret = DISK_CREATE_PARTITION_INVALID_TYPE;
			break;
		    }
		}
	    }
	if( ret==0 && p->type()!=EXTENDED )
	    {
	    if( p->id()==Partition::ID_SWAP )
		{
		cmd_line << "linux-swap ";
		}
	    else if( p->id()==Partition::ID_GPT_BOOT ||
		     p->id()==Partition::ID_DOS16 ||
	             p->id()==Partition::ID_DOS32 )
	        {
		cmd_line << "fat32 ";
		}
	    else if( p->id()==Partition::ID_APPLE_HFS )
		{
		cmd_line << "hfs ";
		}
	    else
		{
		cmd_line << "ext2 ";
		}
	    }
	getStorage()->handleHald(true);
	if( ret==0 )
	    {
	    unsigned long start = p->cylStart();
	    unsigned long end = p->cylStart()+p->cylSize();
	    ConstPartPair pp = (p->type()!=LOGICAL) ? partPair( existingNotLog )
						    : partPair( existingLog );
	    unsigned long maxc = cylinders();
	    if( p->type()==LOGICAL )
		{
		ConstPartPair ext = partPair(notDeletedExt);
		if( !ext.empty() )
		    maxc = ext.begin()->cylEnd();
		}
	    y2mil("max " << maxc << " end:" << end);
	    y2mil("pp " << *p);
	    for (ConstPartIter i = pp.begin(); i != pp.end(); ++i)
		{
		y2mil( "i " << *i );
		if( i->cylStart()<maxc && i->cylStart()<end &&
		    i->cylEnd()-fuzz_cyl>p->cylStart() )
		    {
		    maxc=i->cylStart();
		    y2mil( "new maxc " << maxc );
		    }
		}
	    y2mil("max " << maxc);
	    if( new_geometry != geometry )
		{
		y2mil("parted geometry changed old geometry:" << geometry);
		y2mil("parted geometry changed new geometry:" << new_geometry);
		y2mil("old start:" << start << " end:" << end);
		start = start * new_geometry.cylinders / geometry.cylinders;
		end = end * new_geometry.cylinders / geometry.cylinders;
		y2mil("new start:" << start << " end:" << end);
		}
	    if( end>maxc && maxc<=cylinders() )
		{
		y2mil("corrected end from " << end << " to max " << maxc);
		end = maxc;
		}
	    if( start==0 && (label == "mac" || label == "amiga") )
		start = 1;
	    string save = cmd_line.str();
	    y2mil( "end:" << end << " cylinders:" << cylinders() );
	    string tmp = save + decString(start) + ' ' + decString(end);
	    if( execCheckFailed( tmp, false ) )
		{
		tmp = save + decString(start+1) + ' ' + decString(end+1);
		if( execCheckFailed( tmp, false ))
		    {
		    tmp = save + decString(start) + ' ' + decString(end-1);
		    if( execCheckFailed( tmp, false ) )
			{
			ret = DISK_CREATE_PARTITION_PARTED_FAILED;
			}
		    }
		}
	    }
	if( ret==0 )
	    {
	    p->setCreated( false );
	    if( !getPartedValues( p ))
		ret = DISK_PARTITION_NOT_FOUND;
	    }
	getStorage()->handleHald(false);
	if( ret==0 && !dmp_slave )
	    {
	    if( p->type()!=EXTENDED )
		Storage::waitForDevice(p->device());
	    else
		Storage::waitForDevice();
	    if( p->type()==LOGICAL && getStorage()->instsys() )
		{
		// kludge to make the extended partition visible in
		// /proc/partitions otherwise grub refuses to install if root
		// filesystem is a logical partition
		ConstPartPair lc = partPair(logicalCreated);
		call_blockdev = lc.length()<=1;
		y2mil("logicalCreated:" << lc.length() << " call_blockdev:" << call_blockdev);
		}
	    }
	if( ret==0 && p->type()!=EXTENDED )
	    {
	    ret = p->zeroIfNeeded();
	    if( !dmp_slave && !p->getFormat() )
		{
		bool lsave = false;
		string lbl;
		if( p->needLabel() )
		    {
		    lsave = true;
		    lbl = p->getLabel();
		    }
		p->updateFsData();
		if( lsave )
		    p->setLabel(lbl);
		}
	    }
	if( ret==0 && !dmp_slave && p->id()!=Partition::ID_LINUX )
	    {
	    ret = doSetType( p );
	    }
	if( !dmp_slave && call_blockdev )
	    {
	    SystemCmd c(BLOCKDEVBIN " --rereadpt " + quote(device()));
	    if( p->type()!=EXTENDED )
		Storage::waitForDevice(p->device());
	    }
	}
    else
	{
	ret = DISK_CREATE_PARTITION_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Disk::doRemove( Volume* v )
    {
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->removeText(true) );
	    }
	y2mil("doRemove container " << name() << " name " << p->name());
	if( !dmp_slave )
	    {
	    getStorage()->removeDmMapsTo( getPartDevice(p->OrigNr()) );
	    ret = v->prepareRemove();
	    }
	if( ret==0 && !p->created() )
	    {
	    std::ostringstream cmd_line;
	    classic(cmd_line);
	    cmd_line << PARTEDCMD << quote(device()) << " rm " << p->OrigNr();
	    getStorage()->handleHald(true);
	    if( execCheckFailed( cmd_line.str(), false ) )
		{
		ret = DISK_REMOVE_PARTITION_PARTED_FAILED;
		}
	    ProcParts parts;
	    if( parts.findDevice(getPartName(p->OrigNr())) )
		callDelpart( p->OrigNr() );
	    getStorage()->handleHald(false);
	    }
	if( ret==0 )
	    {
	    if( !removeFromList( p ) )
		ret = DISK_REMOVE_PARTITION_LIST_ERASE;
	    }
	if( ret==0 )
	    {
	    ConstPartPair p = partPair( notCreatedPrimary );
	    if( p.empty() )
		redetectGeometry();
	    }
	if( ret==0 && !dmp_slave )
	    Storage::waitForDevice();
	}
    else
	{
	ret = DISK_REMOVE_PARTITION_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }


int
Disk::freeCylindersAroundPartition(const Partition* p, unsigned long& freeCylsBefore,
				   unsigned long& freeCylsAfter) const
    {
    int ret = 0;

    unsigned long startBefore = 0;
    const unsigned long endBefore = p->cylStart() - 1;
    const unsigned long startAfter = p->cylEnd() + 1;
    unsigned long endAfter = cylinders();
    Region usable_region = usableCylRegion();
    if (p->type() == LOGICAL && hasExtended())
	{
	ConstPartPair pp = partPair(notDeletedExt);
	startBefore = pp.begin()->cylStart() - 1;
	endAfter = pp.begin()->cylEnd() + 1;
	usable_region = pp.begin()->cylRegion();
	}

    y2mil("startBefore:" << startBefore << " endBefore:" << endBefore);
    y2mil("startAfter:" << startAfter << " endAfter:" << endAfter);

    ConstPartPair pp = partPair(Partition::notDeleted);
    ConstPartIter previous = pp.end();
    ConstPartIter next = pp.end();

    y2mil( "p:" << *p );

    for (ConstPartIter i = pp.begin(); i != pp.end(); ++i)
	{
	y2mil( "i:" << *i );

	if( (i->type()==p->type() || (i->type()==EXTENDED&&p->type()==PRIMARY)) &&
	    i->cylEnd()<=p->cylEnd() && i->nr()!=p->nr() &&
	    (previous==pp.end() || previous->cylEnd()<i->cylEnd()) )
	    {
	    previous = i;
	    }

	if( (i->type()==p->type() || (i->type()==EXTENDED&&p->type()==PRIMARY)) &&
	    i->cylStart()>=p->cylStart() && i->nr()!=p->nr() &&
	    (next==pp.end() || next->cylStart()>i->cylStart()) )
	    {
	    next = i;
	    }
	}

    if( previous != pp.end() )
	{
	y2mil( "previous:" << *previous );
	if( previous->cylEnd()<endBefore )
	    {
	    startBefore = previous->cylEnd();
	    y2mil( "startBefore:" << startBefore );
	    }
	else
	    startBefore = cylinders();
	}

    if( next != pp.end() )
	{
	y2mil( "next:" << *next );
	if( next->cylStart()>startAfter )
	    {
	    endAfter = next->cylStart();
	    y2mil( "endAfter:" << endAfter );
	    }
	else
	    endAfter = 0;
	}

    freeCylsBefore = freeCylsAfter = 0;
    if (endBefore > startBefore)
	freeCylsBefore = endBefore - startBefore;
    if (endAfter > startAfter)
	freeCylsAfter = endAfter - startAfter;

    y2mil( "bef:" << freeCylsBefore << " aft:" << freeCylsAfter );
    Region around(p->cylStart() - freeCylsBefore, p->cylSize() + freeCylsAfter + freeCylsBefore );
    y2mil( "around:" << around );
    if (usable_region.doIntersect(around))
    {
	around = around.intersect(usable_region);

	freeCylsBefore = freeCylsAfter = 0;
	if (around.start() < p->cylStart())
	    freeCylsBefore = p->cylStart() - around.start();
	if (around.end() > p->cylEnd())
	    freeCylsAfter = around.end() - p->cylEnd();
    }
    else
    {
	freeCylsBefore = freeCylsAfter = 0;
    }

    y2mil("ret:" << ret << " freeCylsBefore:" << freeCylsBefore << " freeCylsAfter:" << freeCylsAfter);
    return ret;
    }


int
Disk::resizePartition(Partition* p, unsigned long newCyl)
{
    y2mil("newCyl:" << newCyl << " p:" << *p);
    int ret = 0;
    if( readonly() )
    {
	ret = DISK_CHANGE_READONLY;
    }
    else
    {
	unsigned long long newSize = cylinderToKb(newCyl);
	if( newCyl!=p->cylSize() )
	    ret = p->canResize( newSize );
	if( ret==0 && newCyl<p->cylSize() )
	{
	    if( p->created() )
		p->changeRegion(Region(p->cylStart(), newCyl), newSize);
	    else
		p->setResizedSize( newSize );
	}
	y2mil("newCyl:" << newCyl << " p->cylSize():" << p->cylSize());
	if( ret==0 && newCyl>p->cylSize() )
	{
	    unsigned long free_cyls_before = 0;
	    unsigned long free_cyls_after = 0;
	    ret = freeCylindersAroundPartition(p, free_cyls_before, free_cyls_after);
	    if (ret == 0)
	    {
		unsigned long increase = newCyl - p->cylSize();
		if (free_cyls_after < increase)
		{
		    ret = DISK_RESIZE_NO_SPACE;
		}
		else
		{
		    if( p->created() )
			p->changeRegion(Region(p->cylStart(), newCyl), newSize);
		    else
			p->setResizedSize( newSize );
		}
	    }
	}
    }
    y2mil("ret:" << ret);
    return ret;
}


int Disk::resizeVolume( Volume* v, unsigned long long newSize )
    {
    int ret = 0;
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    else
	{
	Partition * p = dynamic_cast<Partition *>(v);
	unsigned new_cyl_cnt = kbToCylinder(newSize);
	newSize = cylinderToKb(new_cyl_cnt);
	if( p!=NULL )
	    {
	    ret = resizePartition( p, new_cyl_cnt );
	    }
	else
	    {
	    ret = DISK_CHECK_RESIZE_INVALID_VOLUME;
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Disk::removeVolume( Volume* v )
    {
    return( removePartition( v->nr() ));
    }

bool Disk::isLogical( unsigned nr ) const
    {
    bool ret = ext_possible && nr>max_primary;
    y2mil("nr:" << nr << " ret:" << ret);
    return( ret );
    }

int Disk::doResize( Volume* v )
    {
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	bool remount = false;
	bool needExtend = !p->needShrink();
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->resizeText(true) );
	    }
	if( !dmp_slave && p->isMounted() )
	    {
	    ret = p->umount();
	    if( ret==0 )
		remount = true;
	    }
	if( ret==0 && !dmp_slave && !needExtend && 
	    p->getFs()!=HFS && p->getFs()!=HFSPLUS && p->getFs()!=VFAT && 
	    p->getFs()!=FSNONE )
	    ret = p->resizeFs();
	if( ret==0 )
	    {
	    y2mil("doResize container " << name() << " name " << p->name());
	    std::ostringstream cmd_line;
	    classic(cmd_line);
	    unsigned long long start_sect, end_sect;
	    getPartedSectors( p, start_sect, end_sect );
	    end_sect = start_sect + kbToSector(p->sizeK()) - 1;
	    y2mil("end_sect " << end_sect);
	    const Partition * after = getPartitionAfter( p );
	    unsigned long max_end = kbToSector(sizeK()) - 1;
	    if( after!=NULL )
		{
		unsigned long long start_after, end_after;
		getPartedSectors( after, start_after, end_after );
		max_end = start_after-1;
		if( p->type() == LOGICAL )
		    max_end--;
		}
	    else if( p->type()==LOGICAL )
		{
		ConstPartPair ext = partPair(notDeletedExt);
		if( !ext.empty() )
		    {
		    unsigned long long start_ext, end_ext;
		    getPartedSectors( &(*ext.begin()), start_ext, end_ext );
		    max_end = end_ext;
		    }
		}
	    y2mil( "max_end:" << max_end << " end_sect:" << end_sect );
	    if( max_end<end_sect ||
		max_end-end_sect < geometry.cylinderSize() / sectorSize() * 2 )
		{
		end_sect = max_end;
		y2mil( "new end_sect:" << end_sect );
		}
	    cmd_line << "YAST_IS_RUNNING=1 " << PARTEDCMD << quote(device())
	             << " unit s resize " << p->nr() << " "
	             << start_sect << " " << end_sect;
	    getStorage()->handleHald(true);
	    if( execCheckFailed( cmd_line.str(), false ) )
		{
		ret = DISK_RESIZE_PARTITION_PARTED_FAILED;
		}
	    if( !getPartedValues( p ))
		{
		if( ret==0 )
		    ret = DISK_PARTITION_NOT_FOUND;
		}
	    getStorage()->handleHald(false);
	    if( ret==0 && !dmp_slave )
		Storage::waitForDevice(p->device());
	    y2mil("after resize size:" << p->sizeK() << " resize:" << (p->needShrink()||p->needExtend()));
	    }
	if( needExtend && !dmp_slave && 
	    p->getFs()!=HFS && p->getFs()!=HFSPLUS && p->getFs()!=VFAT && 
	    p->getFs()!=FSNONE )
	    ret = p->resizeFs();
	if( ret==0 && remount )
	    ret = p->mount();
	}
    else
	{
	ret = DISK_RESIZE_PARTITION_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

const Partition* Disk::getPartitionAfter(const Partition* p) const
    {
    const Partition* ret = NULL;
    y2mil( "p:" << *p );
    ConstPartPair pp = partPair((p->type() == LOGICAL) ? Partition::notDeleted : notDeletedLog);
    for (ConstPartIter pi = pp.begin(); pi != pp.end(); ++pi)
	{
	if( !pi->created() &&
	    pi->cylStart()>p->cylStart() &&
	    (ret==NULL || ret->cylStart()>pi->cylStart()) )
	    ret = &(*pi);
	}
    if( ret==NULL )
	y2mil( "ret:NULL" );
    else
	y2mil( "ret:" << *ret );
    return ret;
    }

void Disk::addPartition( unsigned num, unsigned long long sz,
		         SystemInfo& systeminfo )
    {
    unsigned long cyl_inc = std::max(kbToSector(size_k) / geometry.heads / geometry.sectors, 1ULL);
    Partition *p = new Partition(*this, getPartName(num), getPartDevice(num), num, systeminfo, sz,
				 Region(geometry.cylinders, cyl_inc), PRIMARY);
    geometry.cylinders += cyl_inc;
    new_geometry.cylinders = geometry.cylinders;
    if( systeminfo.getProcParts().getSize(p->procName(), sz) && sz>0 )
	{
	p->setSize( sz );
	}
    addToList( p );
    }

unsigned Disk::numPartitions() const
    {
    return partPair(Partition::notDeleted).length();
    }

void Disk::getInfo( DiskInfo& tinfo ) const
    {
    info.sizeK = sizeK();
    info.cyl = cylinders();
    info.heads = heads();
    info.sectors = sectors();
    info.sectorSize = sectorSize();
    info.cylSize = geometry.cylinderSize();
    info.disklabel = labelName();
    info.maxPrimary = maxPrimary();
    info.extendedPossible = extendedPossible();
    info.maxLogical = maxLogical();
    info.initDisk = init_disk;
    info.iscsi = transport == ISCSI;
    info.transport = transport;
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Disk& d )
    {
    s << dynamic_cast<const Container&>(d);
    s << " geometry:" << d.geometry
      << " Range:" << d.range
      << " SizeK:" << d.size_k
      << " Label:" << d.label;
    if( d.detected_label!=d.label )
	s << " DetectedLabel:" << d.detected_label;
    if( !d.udev_path.empty() )
	s << " UdevPath:" << d.udev_path;
    if( !d.udev_id.empty() )
	s << " UdevId:" << d.udev_id;
    s << " MaxPrimary:" << d.max_primary;
    if( d.ext_possible )
	s << " ExtPossible MaxLogical:" << d.max_logical;
    if( d.init_disk )
	s << " InitDisk";
    s << " transport:" << toString(d.transport);
    if( d.dmp_slave )
	s << " DmpSlave";
    if( d.no_addpart )
	s << " NoAddpart";
    if( d.gpt_enlarge )
	s << " GptEnlarge";
    if (d.del_ptable)
	s << " delPT";
    return( s );
    }


    void
    Disk::logDifference(std::ostream& log, const Disk& rhs) const
    {
	Container::logDifference(log, rhs);

	logDiff(log, "geometry", geometry, rhs.geometry);
	logDiff(log, "size_k", size_k, rhs.size_k);

	logDiff(log, "label", label, rhs.label);
	logDiff(log, "range", range, rhs.range);
	logDiff(log, "max_primary", max_primary, rhs.max_primary);
	logDiff(log, "ext_possible", ext_possible, rhs.ext_possible);
	logDiff(log, "max_logical", max_logical, rhs.max_logical);

	logDiff(log, "init_disk", init_disk, rhs.init_disk);

	logDiffEnum(log, "transport", transport, rhs.transport);

	logDiff(log, "del_ptable", del_ptable, rhs.del_ptable);
    }


    void
    Disk::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
	const Disk& rhs = dynamic_cast<const Disk&>(rhs_c);

	logDifference(log, rhs);
	log << endl;

	ConstPartPair pp = partPair();
	ConstPartPair pc = rhs.partPair();
	logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


bool Disk::equalContent( const Container& rhs ) const
    {
    const Disk * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const Disk*>(&rhs);
    if( ret && p )
	ret = geometry == p->geometry &&
	      mjr==p->mjr && mnr==p->mnr && range==p->range &&
	      size_k==p->size_k && max_primary==p->max_primary &&
	      ext_possible==p->ext_possible && max_logical==p->max_logical &&
	      init_disk==p->init_disk && label==p->label && 
	      transport == p->transport &&
	      dmp_slave==p->dmp_slave && no_addpart==p->no_addpart &&
	      gpt_enlarge==p->gpt_enlarge && del_ptable == p->del_ptable;
    if( ret && p )
	{
	ConstPartPair pp = partPair();
	ConstPartPair pc = p->partPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
	}
    return( ret );
    }

}
