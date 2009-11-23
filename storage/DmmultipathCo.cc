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
	
	Entry entry;

	string line;
	unsigned i = 0;

	if( i<c.numLines() )
	    line = c.getLine(i);

	while( i<c.numLines() )
	{
	    while( i<c.numLines() && (line.empty() || !isalnum(line[0])))
		if( ++i<c.numLines() )
		    line = c.getLine(i);
		
	    y2mil("mp line:" << line);

	    string name = extractNthWord(0, line);
	    y2mil("mp name:" << name);

	    list<string> tmp = splitString(extractNthWord(2, line, true), ",");
	    if (tmp.size() >= 2)
	    {
		list<string>::const_iterator it = tmp.begin();
		entry.vendor = boost::trim_copy(*it++, locale::classic());
		entry.model = boost::trim_copy(*it++, locale::classic());
	    }
	    else
	    {
		entry.vendor.clear();
		entry.model.clear();
	    }
		
	    entry.devices.clear();

	    if( ++i<c.numLines() )
		line = c.getLine(i);
	    while( i<c.numLines() && (line.empty() || !isalnum(line[0])))
	    {
		if (boost::starts_with(line, " \\_"))
		{
		    y2mil("mp element:" << line);
		    string dev = "/dev/" + extractNthWord(2, line);
		    if (find(entry.devices.begin(), entry.devices.end(), dev) == entry.devices.end())
			entry.devices.push_back(dev);
		}
		if( ++i<c.numLines() )
		    line = c.getLine(i);
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


    DmmultipathCo::DmmultipathCo(Storage * const s, const string& name, SystemInfo& systeminfo)
	: DmPartCo(s, "/dev/mapper/" + name, staticType(), systeminfo)
    {
	DmPartCo::init(systeminfo);
	getMultipathData(name, systeminfo);
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
	    
	    for (list<string>::const_iterator it = entry.devices.begin(); it != entry.devices.end(); it++)
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
    dm = new Dmmultipath( *this, num, p );
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


string DmmultipathCo::setDiskLabelText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by multipath name (e.g. 3600508b400105f590000900000300000),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Setting disk label of multipath disk %1$s to %2$s"),
		       name().c_str(), labelName().c_str());
        }
    else
        {
        // displayed text before action, %1$s is replaced by multipath name (e.g. 3600508b400105f590000900000300000),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Set disk label of multipath disk %1$s to %2$s"),
		       name().c_str(), labelName().c_str());
        }
    return( txt );
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
    s << *((DmPartCo*)&d);
    s << " Vendor:" << d.vendor
      << " Model:" << d.model;
    return s;
}


string DmmultipathCo::getDiffString( const Container& d ) const
{
    string log = DmPartCo::getDiffString(d);
    const DmmultipathCo * p = dynamic_cast<const DmmultipathCo*>(&d);
    if (p)
    {
	if (vendor != p->vendor)
	    log += " vendor:" + vendor + "-->" + p->vendor;
	if (model != p->model)
	    log += " model:" + model + "-->" + p->model;
    }
    return log;
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


void
DmmultipathCo::logData(const string& Dir) const
{
}


bool DmmultipathCo::active = false;

}
