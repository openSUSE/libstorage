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
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"
#include "storage/SystemInfo.h"


namespace storage
{
    using namespace std;


    CmdMultipath::CmdMultipath()
    {
	SystemCmd c(MULTIPATHBIN " -d -v 2+ -ll");
	if (c.retcode() != 0 || c.numLines() == 0)
	    return;

	const vector<string>& lines = c.stdout();
	vector<string>::const_iterator it1 = lines.begin();

	while (it1 != lines.end())
	{
	    Entry entry;

	    y2mil("mp line:" << *it1);

	    string name = extractNthWord(0, *it1);
	    y2mil("mp name:" << name);

	    bool has_alias = boost::starts_with(extractNthWord(1, *it1), "(");

	    list<string> tmp = splitString(extractNthWord(has_alias ? 3 : 2, *it1, true), ",");
	    if (tmp.size() >= 2)
	    {
		list<string>::const_iterator it2 = tmp.begin();
		entry.vendor = boost::trim_copy(*it2++, locale::classic());
		entry.model = boost::trim_copy(*it2++, locale::classic());
	    }

	    ++it1;

	    if (it1 != lines.end())
		++it1;

	    while (it1 != lines.end())
	    {
		if (it1->empty() || isalnum((*it1)[0]))
		    break;

		if (boost::starts_with(*it1, "| `-") || boost::starts_with(*it1, "  `-") ||
		    boost::starts_with(*it1, "  |-"))
		{
		    string tmp = it1->substr(5);
		    y2mil("mp element:" << tmp);
		    string dev = "/dev/" + extractNthWord(1, tmp);
		    if (find(entry.devices.begin(), entry.devices.end(), dev) == entry.devices.end())
			entry.devices.push_back(dev);
		}

		++it1;
	    }

	    data[name] = entry;
	}

	for (const_iterator it = data.begin(); it != data.end(); ++it)
	    y2mil("data[" << it->first << "] -> vendor:" << it->second.vendor <<
		  " model:" << it->second.model << " devices:" << it->second.devices);
    }


    list<string>
    CmdMultipath::getEntries() const
    {
	list<string> ret;
	for (const_iterator it = data.begin(); it != data.end(); ++it)
	    ret.push_back(it->first);
	return ret;
    }


    bool
    CmdMultipath::getEntry(const string& name, Entry& entry) const
    {
	const_iterator it = data.find(name);
	if (it == data.end())
	    return false;

	entry = it->second;
	return true;
    }


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
    udev_id.remove_if(string_starts_with("dm-"));
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
	    c.execute(MULTIPATHDBIN " -F");
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
	    CmdDmsetup::Entry entry;
	    if (systeminfo.getCmdDmsetup().getEntry(*it, entry) && entry.segments > 0)
		l.push_back(*it);
	    else
		y2mil("ignoring inactive dmmultipath " << *it);
        }

	if (!l.empty())
	    active = true;

	y2mil("detected multipaths " << l);
	return l;
    }


void DmmultipathCo::getInfo( DmmultipathCoInfo& tinfo ) const
    {
    DmPartCo::getInfo( info );
    tinfo.p = info;
    tinfo.vendor = vendor;
    tinfo.model = model;
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
