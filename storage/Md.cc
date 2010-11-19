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
#include <boost/algorithm/string.hpp>

#include "storage/Md.h"
#include "storage/StorageTypes.h"
#include "storage/Storage.h"
#include "storage/AppUtil.h"
#include "storage/SystemCmd.h"
#include "storage/Regex.h"
#include "storage/Container.h"
#include "storage/EtcMdadm.h"
#include "storage/StorageDefines.h"
#include "storage/SystemInfo.h"


namespace storage
{
    using namespace std;


    Md::Md(const MdCo& c, const string& name, const string& device, MdType Type,
	   const list<string>& devices, const list<string>& spares)
	: Volume(c, name, device), md_type(Type), md_parity(PAR_DEFAULT), chunk_k(0),
	  sb_ver("01.00.00"), destrSb(false), devs(devices), spare(spares), has_container(false)
    {
	y2deb("constructed Md " << dev << " on " << cont->device());

	assert(c.type() == MD);

	numeric = true;
	mdStringNum(name, num);

	getStorage()->addUsedBy(devs, UB_MD, dev);
	getStorage()->addUsedBy(spares, UB_MD, dev);

	computeSize();
    }


    Md::Md(const MdCo& c, const string& name, const string& device, SystemInfo& systeminfo)
	: Volume(c, name, device, systeminfo), md_type(RAID_UNK), md_parity(PAR_DEFAULT),
	  chunk_k(0), sb_ver("01.00.00"), destrSb(false), has_container(false)
    {
	y2deb("constructed Md " << device << " on " << cont->device());

	assert(c.type() == MD);

	numeric = true;
	mdStringNum(name, num);

	getMajorMinor();
	getStorage()->fetchDanglingUsedBy(dev, uby);

	ProcMdstat::Entry entry;
	if (!systeminfo.getProcMdstat().getEntry(nm, entry))
	    y2err("not found in mdstat nm:" << nm);

	md_type = entry.md_type;
	md_parity = entry.md_parity;

	setSize(entry.size_k);
	chunk_k = entry.chunk_k;

	devs = entry.devices;
	spare = entry.spares;

	if (entry.readonly)
	    setReadonly();

	if (entry.has_container)
	{
	    has_container = true;
	    parent_container = entry.container_name;

	    MdadmDetails details;
	    if (getMdadmDetails("/dev/" + entry.container_name, details))
	    {
		parent_uuid = details.uuid;
		parent_md_name = details.devname;
		parent_metadata = details.metadata;
	    }

	    parent_member = entry.container_member;

	    sb_ver = parent_metadata;
	}
	else
	{
	    sb_ver = entry.super;
	}

	setUdevData(systeminfo);

	MdadmDetails details;
	if (getMdadmDetails(dev, details))
	{
	    md_uuid = details.uuid;
	    md_name = details.devname;
	}

	getStorage()->addUsedBy(devs, UB_MD, dev);
	getStorage()->addUsedBy(spare, UB_MD, dev);
    }


    Md::Md(const MdCo& c, const Md& v)
	: Volume(c, v), md_type(v.md_type), md_parity(v.md_parity),
	  chunk_k(v.chunk_k), md_uuid(v.md_uuid), md_name(v.md_name),
	  sb_ver(v.sb_ver), destrSb(v.destrSb), devs(v.devs), spare(v.spare),
	  udev_id(udev_id),
	  has_container(v.has_container), parent_container(v.parent_container),
	  parent_uuid(v.parent_uuid), parent_md_name(v.parent_md_name),
	  parent_metadata(v.parent_metadata), parent_member(v.parent_member)
    {
	y2deb("copy-constructed Md from " << v.dev);
    }


    Md::~Md()
    {
	y2deb("destructed Md " << dev);
    }


    void
    Md::updateData(SystemInfo& systeminfo)
    {
	ProcMdstat::Entry entry;
	if (systeminfo.getProcMdstat().getEntry(nm, entry))
	{
	    if (md_type != entry.md_type)
		y2war("inconsistent md_type my:" << toString(md_type) << " kernel:" << toString(entry.md_type));
	    if (md_parity != PAR_DEFAULT && md_parity != entry.md_parity)
		y2war("inconsistent md_parity my:" << toString(md_parity) << " kernel:" << toString(entry.md_parity));
	    if (chunk_k > 0 && chunk_k != entry.chunk_k)
		y2war("inconsistent chunk my:" << chunk_k << " kernel:" << entry.chunk_k);

	    md_type = entry.md_type;
	    md_parity = entry.md_parity;

	    setSize(entry.size_k);
	    chunk_k = entry.chunk_k;
	}
	else
	{
	    y2err("not found in mdstat nm:" << nm);
	}

	MdadmDetails details;
	if (getMdadmDetails("/dev/" + nm, details))
	{
	    setMdUuid(details.uuid);
	}
    }


    void
    Md::setUdevData(SystemInfo& systeminfo)
    {
	const UdevMap& by_id = systeminfo.getUdevMap("/dev/disk/by-id");
	UdevMap::const_iterator it = by_id.find(nm);
	if (it != by_id.end())
	{
	    udev_id = it->second;
	    partition(udev_id.begin(), udev_id.end(), string_starts_with("md-uuid-"));
	}
	else
	{
	    udev_id.clear();
	}

	y2mil("dev:" << dev << " udev_id:" << udev_id);

	alt_names.remove_if(string_starts_with("/dev/disk/by-id/"));
	for (list<string>::const_iterator i = udev_id.begin(); i != udev_id.end(); ++i)
	    alt_names.push_back("/dev/disk/by-id/" + *i);
    }


    list<string>
    Md::getDevs(bool all, bool spares) const
    {
	list<string> ret;
	if (!all)
	{
	    ret = spares ? spare : devs;
	}
	else
	{
	    ret = devs;
	    ret.insert(ret.end(), spare.begin(), spare.end());
	}
	return ret;
    }


int
Md::addDevice(const string& new_dev, bool to_spare)
    {
    int ret = 0;
    if (find(devs.begin(), devs.end(), new_dev) != devs.end() ||
        find(spare.begin(), spare.end(), new_dev) != spare.end())
	{
	ret = MD_ADD_DUPLICATE;
	}
    if( ret==0 )
	{
	if (!to_spare)
	    devs.push_back(new_dev);
	else
	    spare.push_back(new_dev);
	getStorage()->addUsedBy(new_dev, UB_MD, dev);
	computeSize();
	}
    y2mil("new_dev:" << new_dev << " to_spare:" << to_spare << " ret:" << ret);
    return ret;
    }


int
Md::removeDevice( const string& dev )
    {
    int ret = 0;
    list<string>::iterator i;
    if( (i=find( devs.begin(), devs.end(), dev ))!=devs.end() )
	{
	devs.erase(i);
	getStorage()->clearUsedBy(dev);
	computeSize();
	}
    else if( (i=find( spare.begin(), spare.end(), dev ))!=spare.end() )
        {
	spare.erase(i);
	getStorage()->clearUsedBy(dev);
	computeSize();
	}
    else
	ret = MD_REMOVE_NONEXISTENT;
    y2mil("dev:" << dev << " ret:" << ret);
    return( ret );
    }

int
Md::checkDevices()
    {
    unsigned nmin = 2;
    switch( md_type )
	{
	case RAID5:
	    nmin = 3;
	    break;
	case RAID6:
	    nmin = 4;
	    break;
	default:
	    break;
	}
    int ret = devs.size()<nmin ? MD_TOO_FEW_DEVICES : 0;

    if (ret == 0 && md_type == RAID0 && !spare.empty())
	ret = MD_TOO_MANY_SPARES;

    y2mil("type:" << toString(md_type) << " min:" << nmin << " size:" << devs.size() <<
	  " ret:" << ret);
    return( ret );
    }


int
Md::getState(MdStateInfo& info) const
{
    string value;
    if (read_sysfs_property(sysfsPath() + "/md/array_state", value))
	if (toValue(value, info.state))
	    return STORAGE_NO_ERROR;

    return MD_GET_STATE_FAILED;
}


void
Md::computeSize()
{
    unsigned long long size_k = 0;
    getStorage()->computeMdSize(md_type, devs, spare, size_k);
    setSize(size_k);
}


void Md::changeDeviceName( const string& old, const string& nw )
    {
    list<string>::iterator i = find( devs.begin(), devs.end(), old );
    if( i!=devs.end() )
	*i = nw;
    i = find( spare.begin(), spare.end(), old );
    if( i!=spare.end() )
	*i = nw;
    }


string
Md::createCmd() const
{
    string cmd = LSBIN " -l --full-time " + quote(devs) + " " + quote(spare) + "; "
	MODPROBEBIN " " + toString(md_type) + "; " MDADMBIN " --create " + quote(device()) +
	" --run --level=" + toString(md_type) + " -e 1.0";
    if (md_type == RAID1 || md_type == RAID5 || md_type == RAID6 || md_type == RAID10)
	cmd += " -b internal";
    if (chunk_k > 0)
	cmd += " --chunk=" + decString(chunk_k);
    if (md_parity != PAR_DEFAULT)
	cmd += " --parity=" + toString(md_parity);
    cmd += " --raid-devices=" + decString(devs.size());
    if (!spare.empty())
	cmd += " --spare-devices=" + decString(spare.size());
    cmd += " " + quote(devs) + " " + quote(spare);
    y2mil("ret:" << cmd);
    return cmd;
}


Text Md::removeText( bool doing ) const
{
    Text txt;
    if( doing )
    {
	// displayed text during action, %1$s is replaced by device name e.g. /dev/md0
	txt = sformat(_("Deleting software RAID %1$s"), dev.c_str());
    }
    else
    {
	// displayed text before action, %1$s is replaced by device name e.g. md0
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat(_("Delete software RAID %1$s (%2$s)"), dev.c_str(),
		      sizeString().c_str());
    }
    return txt;
}


Text Md::createText( bool doing ) const
{
    Text txt;
    if( doing )
    {
	// displayed text during action, %1$s is replaced by device name e.g. /dev/md0
	// %2$s is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
	txt = sformat(_("Creating software RAID %1$s from %2$s"), dev.c_str(), 
		      boost::join(devs, " ").c_str());
    }
    else
    {
	if( !mp.empty() )
	{
	    if( encryption==ENC_NONE )
	    {
		// displayed text before action, %1$s is replaced by device name e.g. md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		// %5$s is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
		txt = sformat(_("Create software RAID %1$s (%2$s) from %5$s for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str(), boost::join(devs, " ").c_str());
	    }
	    else
	    {
		// displayed text before action, %1$s is replaced by device name e.g. md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		// %5$s is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
		txt = sformat(_("Create encrypted software RAID %1$s (%2$s) from %5$s for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str(), boost::join(devs, " ").c_str());
	    }
	}
	else
	{
	    // displayed text before action, %1$s is replaced by device name e.g. md0
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
	    txt = sformat(_("Create software RAID %1$s (%2$s) from %3$s"), dev.c_str(),
			  sizeString().c_str(), boost::join(devs, " ").c_str());
	}
    }
    return txt;
}


Text Md::formatText( bool doing ) const
{
    Text txt;
    if( doing )
    {
	// displayed text during action, %1$s is replaced by device name e.g. /dev/md0
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat(_("Formatting software RAID %1$s (%2$s) with %3$s "),
		      dev.c_str(), sizeString().c_str(), fsTypeString().c_str());
    }
    else
    {
	if( !mp.empty() )
	{
	    if( encryption==ENC_NONE )
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat(_("Format software RAID %1$s (%2$s) for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str());
	    }
	    else
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat(_("Format encrypted software RAID %1$s (%2$s) for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str());
	    }
	}
	else
	{
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/md0
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat(_("Format software RAID %1$s (%2$s) with %3$s"),
			  dev.c_str(), sizeString().c_str(), fsTypeString().c_str());
	}
    }
    return txt;
}


bool Md::matchRegex( const string& dev )
    {
    static Regex md( "^md[0123456789]+$" );
    return( md.match(dev));
    }

bool Md::mdStringNum( const string& name, unsigned& num )
    {
    bool ret=false;
    string d = undevDevice(name);
    if( matchRegex( d ))
	{
	d.substr( 2 )>>num;
	ret = true;
	}
    return( ret );
    }

string Md::mdDevice( unsigned num )
    {
    string dev( "/dev/md" );
    dev += decString(num);
    return( dev );
    }

void Md::setPersonality( MdType val )
    {
    md_type=val;
    computeSize();
    }

int Md::setParity( MdParity val )
    {
    int ret = 0;
    list<int> pars = getStorage()->getMdAllowedParity( md_type, devs.size() );
    if( find( pars.begin(), pars.end(), val )!=pars.end() )
	md_parity=val;
    else
	ret = MD_INVALID_PARITY;
    return( ret );
    }

unsigned Md::mdMajor()
    {
    if( md_major==0 )
    {
	md_major = getMajorDevices("md");
	y2mil("md_major:" << md_major);
    }
    return( md_major );
    }


    string
    Md::sysfsPath() const
    {
	return SYSFSDIR "/" + procName();
    }


void Md::getInfo( MdInfo& tinfo ) const
    {
    Volume::getInfo(info.v);
    info.nr = num;
    info.type = md_type;
    info.uuid = md_uuid;
    info.sb_ver = sb_ver;
    info.chunkSizeK = chunk_k;
    info.parity = md_parity;

    info.devices = boost::join(devs, " ");
    info.spares = boost::join(spare, " ");

    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Md& m )
    {
    s << "Md " << dynamic_cast<const Volume&>(m)
      << " Personality:" << toString(m.md_type);
    if (m.chunk_k > 0)
	s << " ChunkK:" << m.chunk_k;
    if (m.md_parity != PAR_DEFAULT)
	s << " Parity:" << toString(m.md_parity);
    if( !m.sb_ver.empty() )
	s << " SbVer:" << m.sb_ver;
    if (!m.md_uuid.empty())
	s << " md_uuid:" << m.md_uuid; 
    if (!m.md_name.empty())
	s << " md_name:" << m.md_name;
    if( m.destrSb )
	s << " destroySb";
    s << " Devices:" << m.devs;
    if( !m.spare.empty() )
	s << " Spares:" << m.spare;
    return s;
    }


bool Md::equalContent( const Md& rhs ) const
    {
    return( Volume::equalContent(rhs) &&
            md_type==rhs.md_type && md_parity==rhs.md_parity &&
	    chunk_k==rhs.chunk_k && md_uuid==rhs.md_uuid && sb_ver==rhs.sb_ver &&
	    destrSb==rhs.destrSb && devs == rhs.devs && spare==rhs.spare );
    }


    void
    Md::logDifference(std::ostream& log, const Md& rhs) const
    {
	Volume::logDifference(log, rhs);

	logDiffEnum(log, "md_type", md_type, rhs.md_type);
	logDiffEnum(log, "md_parity", md_parity, rhs.md_parity);
	logDiff(log, "chunk_k", chunk_k, rhs.chunk_k);
	logDiff(log, "sb_ver", sb_ver, rhs.sb_ver);
	logDiff(log, "md_uuid", md_uuid, rhs.md_uuid);
	logDiff(log, "md_name", md_name, rhs.md_name);
	logDiff(log, "destrSb", destrSb, rhs.destrSb);
	logDiff(log, "devices", devs, rhs.devs);
	logDiff(log, "spares", spare, rhs.spare);

	logDiff(log, "parent_container",  parent_container, rhs.parent_container);
	logDiff(log, "parent_md_name", parent_md_name, rhs.parent_md_name);
	logDiff(log, "parent_metadata", parent_metadata, rhs.parent_metadata);
	logDiff(log, "parent_uuid", parent_uuid, rhs.parent_uuid);
    }


    bool
    Md::updateEntry(EtcMdadm* mdadm) const
    {
	EtcMdadm::mdconf_info info;

	if (!md_name.empty())
	    info.device = "/dev/md/" + md_name;
	else
	    info.device = dev;

	info.uuid = md_uuid;

	if (has_container)
	{
	    info.container_present = true;
	    info.container_uuid = parent_uuid;
	    info.container_metadata = parent_metadata;
	    info.container_member = parent_member;
	}

	return mdadm->updateEntry(info);
    }


unsigned Md::md_major = 0;

}
