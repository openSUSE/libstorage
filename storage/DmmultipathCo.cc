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


#include <ostream>
#include <sstream>
#include <locale>
#include <boost/algorithm/string.hpp>

#include "storage/DmmultipathCo.h"
#include "storage/Dmmultipath.h"
#include "storage/Utils/SystemCmd.h"
#include "storage/Utils/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"
#include "storage/SystemInfo/SystemInfo.h"


namespace storage
{
    using namespace std;


    DmmultipathCo::DmmultipathCo(Storage* s, const string& name, const string& device,
				 SystemInfo& systeminfo)
	: DmPartCo(s, name, device, staticType(), systeminfo)
    {
	getMultipathData(name, systeminfo);

	const UdevMap& by_id = systeminfo.getUdevMap("/dev/disk/by-id");
	UdevMap::const_iterator it = by_id.find("dm-" + decString(minorNr()));
	if (it != by_id.end())
	    setUdevData(it->second);

	y2deb("constructing DmmultipathCo " << name);
    }


    DmmultipathCo::DmmultipathCo(const DmmultipathCo& c)
	: DmPartCo(c), vendor(c.vendor), model(c.model)
    {
	y2deb("copy-constructed DmmultipathCo from " << c.dev);
    }


    DmmultipathCo::~DmmultipathCo()
    {
	y2deb("destructed DmMultipathCo " << dev);
    }


    void
    DmmultipathCo::getMultipathData(const string& name, SystemInfo& systeminfo)
    {
	CmdMultipath::Entry entry;
	if (systeminfo.getCmdMultipath().getEntry(name, entry))
	{
	    vendor = entry.vendor;
	    model = entry.model;

	    for (list<string>::const_iterator it = entry.devices.begin(); it != entry.devices.end(); ++it)
	    {
		Pv pv;
		pv.device = *it;
		addPv(pv);
	    }
	}
    }


    void
    DmmultipathCo::setUdevData(const list<string>& id)
    {
	y2mil("disk:" << nm << " id:" << id);
	udev_id = id;
	partition(udev_id.begin(), udev_id.end(), string_starts_with("wwn-"));
	stable_partition(udev_id.begin(), udev_id.end(), string_starts_with("scsi-"));
	y2mil("id:" << udev_id);

	DmPartCo::setUdevData(udev_id);

	DmmultipathPair pp = dmmultipathPair();
	for( DmmultipathIter p=pp.begin(); p!=pp.end(); ++p )
	{
	    p->addUdevData();
	}
    }


    void
    DmmultipathCo::newP( DmPart*& dm, unsigned num, Partition* p )
    {
	y2mil( "num:" << num );
	dm = new Dmmultipath( *this, getPartName(num), getPartDevice(num), num, p );
    }

    void
    DmmultipathCo::newP( DmPart*& dm, unsigned num, Partition* p, SystemInfo& si )
    {
	y2mil( "num:" << num );
	dm = new Dmmultipath( *this, getPartName(num), getPartDevice(num), num, p, si );
    }

    void
    DmmultipathCo::addPv(const Pv& p)
    {
	PeContainer::addPv(p);
	if (!deleted())
	    getStorage()->setUsedBy(p.device, UB_DMMULTIPATH, device());
    }


    void
    DmmultipathCo::activate(bool val)
    {
	if (getenv("LIBSTORAGE_NO_DMMULTIPATH") != NULL)
	    return;

	y2mil("old active:" << active << " val:" << val);

	if (active != val)
	{
	    bool lvm_active = LvmVg::isActive();
	    LvmVg::activate(false);

	    SystemCmd c;
	    if (val)
	    {
		Dm::activate(true);

		c.execute(MODPROBEBIN " dm-multipath");
		c.execute(MULTIPATHBIN);
		sleep(1);
		c.execute(MULTIPATHDBIN);
	    }
	    else
	    {
                c.execute(MULTIPATHDBIN " -k'shutdown'");
		sleep(1);
		c.execute(MULTIPATHBIN " -F");
	    }

	    LvmVg::activate(lvm_active);

	    active = val;
	}
    }


    list<string>
    DmmultipathCo::getMultipaths(SystemInfo& systeminfo)
    {
	list<string> l;

	list<string> entries = systeminfo.getCmdMultipath().getEntries();
	for (list<string>::const_iterator it = entries.begin(); it != entries.end(); ++it)
	{
	    CmdDmsetupInfo::Entry entry;
	    if (systeminfo.getCmdDmsetupInfo().getEntry(*it, entry) && entry.segments > 0)
		l.push_back(*it);
	    else
		y2mil("ignoring inactive dmmultipath " << *it);
	}

	if (!l.empty())
	    active = true;

	y2mil("detected multipaths " << l);
	return l;
    }


    void DmmultipathCo::getInfo( DmmultipathCoInfo& info ) const
    {
	DmPartCo::getInfo( info.p );
	info.vendor = vendor;
	info.model = model;
    }


    std::ostream& operator<<(std::ostream& s, const DmmultipathCo& d)
    {
	s << dynamic_cast<const DmPartCo&>(d);
	s << " Vendor:" << d.vendor
	  << " Model:" << d.model;
	return s;
    }


    void
    DmmultipathCo::logDifference(std::ostream& log, const DmmultipathCo& rhs) const
    {
	DmPartCo::logDifference(log, rhs);

	logDiff(log, "vendor", vendor, rhs.vendor);
	logDiff(log, "model", model, rhs.model);
    }


    void
    DmmultipathCo::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
	const DmmultipathCo& rhs = dynamic_cast<const DmmultipathCo&>(rhs_c);

	logDifference(log, rhs);
	log << endl;

	ConstDmPartPair pp = dmpartPair();
	ConstDmPartPair pc = rhs.dmpartPair();
	logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


    bool DmmultipathCo::equalContent( const Container& rhs ) const
    {
	bool ret = Container::equalContent(rhs);
	if (ret)
	{
	    const DmmultipathCo *p = dynamic_cast<const DmmultipathCo*>(&rhs);
	    ret = p && DmPartCo::equalContent(*p) &&
		vendor == p->vendor && model == p->model;
	}
	return ret;
    }


    bool DmmultipathCo::active = false;

}
