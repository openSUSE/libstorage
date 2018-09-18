/*
 * Copyright (c) [2004-2014] Novell, Inc.
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


#include "storage/Device.h"
#include "storage/Utils/AppUtil.h"
#include "storage/Utils/StorageTmpl.h"
#include "storage/HumanString.h"
#include "storage/SystemInfo/SystemInfo.h"


namespace storage
{
    using namespace std;


    Device::Device(const string& nm, const string& dev)
	: nm(nm), dev(dev), create(false), del(false), silent(false), size_k(0), mjr(0), mnr(0)
    {
	y2deb("constructed Device " << dev);
    }


    Device::Device(const string& nm, const string& dev, SystemInfo& systeminfo)
	: nm(nm), dev(dev), create(false), del(false), silent(false), size_k(0), mjr(0), mnr(0)
    {
	y2deb("constructed Device " << dev);

	assert(!nm.empty() && !dev.empty());
    }


    Device::Device(const xmlNode* node)
	: nm(), dev(), create(false), del(false), silent(false), size_k(0), mjr(0), mnr(0)
    {
	getChildValue(node, "name", nm);
	getChildValue(node, "device", dev);

	getChildValue(node, "size_k", size_k);

	getChildValue(node, "major", mjr);
	getChildValue(node, "minor", mnr);

	y2deb("constructed Device " << dev);

	assert(!nm.empty() && !dev.empty());
    }


    Device::Device(const Device& d)
	: nm(d.nm), dev(d.dev), create(d.create), del(d.del), silent(d.silent), size_k(d.size_k),
	  mjr(d.mjr), mnr(d.mnr), uby(d.uby), alt_names(d.alt_names), userdata(d.userdata)
    {
	y2deb("copy-constructed Device " << dev);
    }


    Device::~Device()
    {
	y2deb("destructed Device " << dev);
    }


    void
    Device::saveData(xmlNode* node) const
    {
	setChildValue(node, "name", nm);
	setChildValue(node, "device", dev);
	setChildValue(node, "size_k", size_k);
	setChildValue(node, "major", mjr);
	setChildValue(node, "minor", mnr);

	setChildValueIf(node, "used_by", uby, !uby.empty());
    }


    void
    Device::setNameDevice(const string& name, const string& device)
    {
	nm = name;
	dev = device;
    }


    string
    Device::sizeString() const
    {
	return byteToHumanString(1024 * sizeK(), false, 2, false);
    }


    bool
    Device::getMajorMinor()
    {
	return storage::getMajorMinor(dev, mjr, mnr, create);
    }


    void
    Device::getMajorMinor(SystemInfo& systeminfo)
    {
	try
	{
	    const MajorMinor& majorminor = systeminfo.getMajorMinor(dev);
	    mjr = majorminor.getMajor();
	    mnr = majorminor.getMinor();
	}
	catch (const exception& e)
	{
	    y2err("failed to get majorminor for " << dev);
	}
    }


    string
    Device::udevPath() const
    {
	return string();
    }

    list<string>
    Device::udevId() const
    {
	return list<string>();
    }


    void
    Device::setUsedBy(UsedByType type, const string& device)
    {
	uby.clear();
	uby.push_back(UsedBy(type, device));
    }

    void
    Device::addUsedBy(UsedByType type, const string& device)
    {
	uby.push_back(UsedBy(type, device));
    }

    void
    Device::removeUsedBy(UsedByType type, const string& device)
    {
	uby.remove(UsedBy(type, device));
    }

    bool
    Device::isUsedBy(UsedByType type) const
    {
	for (list<UsedBy>::const_iterator it = uby.begin(); it != uby.end(); ++it)
	    if (it->type() == type)
		return true;

	return false;
    }


    bool
    Device::sameDevice(const string& device) const
    {
	string d = normalizeDevice(device);
	return d == dev || contains(alt_names, d);
    }


    void
    Device::getInfo(DeviceInfo& info) const
    {
	info.name = name();
	info.device = device();

	info.udevPath = udevPath();
	info.udevId = udevId();

	info.usedBy = list<UsedByInfo>(uby.begin(), uby.end());

	info.userdata = userdata;
    }


    bool
    Device::equalContent(const Device& rhs) const
    {
	return create == rhs.create && del == rhs.del && silent == rhs.silent &&
	    uby == rhs.uby && userdata == rhs.userdata;
    }


    void
    Device::logDifference(std::ostream& log, const Device& rhs) const
    {
	log << "nm: " + nm;
	if (nm != rhs.nm)
	    log << "-->" << rhs.nm;
	logDiff(log, "dev", dev, rhs.dev);

	logDiff(log, "deleted", del, rhs.del);
	logDiff(log, "created", create, rhs.create);

	logDiff(log, "mjr", mjr, rhs.mjr);
	logDiff(log, "mnr", mnr, rhs.mnr);

	logDiff(log, "userdata", userdata, rhs.userdata);
    }


    std::ostream& operator<<(std::ostream& s, const Device& d)
    {
	s << "Name: " << d.nm << " Device: " << d.dev;
	s << " SizeK: " << d.size_k;
	if (d.mjr != 0 || d.mnr != 0)
	    s << " Node: <" << d.mjr << ":" << d.mnr << ">";
	if (d.create)
	    s << " created";
	if (d.del)
	    s << " deleted";
	if (d.silent)
	    s << " silent";
	if (!d.alt_names.empty())
	    s << " alt_names: " << d.alt_names;
	if (!d.userdata.empty())
	    s << " userdata: " << d.userdata;
	return s;
    }

}
