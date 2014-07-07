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


#include <stdio.h>
#include <string>
#include <ostream>
#include <fstream>
#include <iomanip>
#include <boost/algorithm/string.hpp>

#include "storage/SystemCmd.h"
#include "storage/Storage.h"
#include "storage/OutputProcessor.h"
#include "storage/Dasd.h"
#include "storage/StorageDefines.h"
#include "storage/SystemInfo/SystemInfo.h"


namespace storage
{
    using namespace std;


    Dasd::Dasd(Storage* s, const string& name, const string& device, unsigned long long SizeK,
	       SystemInfo& systeminfo)
	: Disk(s, name, device, SizeK, systeminfo), fmt(DASDF_NONE)
    {
	y2deb("constructed Dasd " << dev);
    }


    Dasd::Dasd(const Dasd& c)
	: Disk(c), fmt(c.fmt)
    {
	y2deb("copy-constructed Dasd from " << c.dev);
    }


    Dasd::~Dasd()
    {
	y2deb("destructed Dasd " << dev);
    }


    bool
    Dasd::detectPartitions(SystemInfo& systeminfo)
    {
	bool ret = true;

	const Dasdview& dasdview = systeminfo.getDasdview(device());
	new_geometry = geometry = dasdview.getGeometry();
	fmt = dasdview.getDasdFormat();
	ronly = fmt != DASDF_CDL;

	if (size_k == 0)
	{
	    size_k = geometry.sizeK();
	    y2mil("New SizeK:" << size_k);
	}

	switch (fmt)
	{
	    case DASDF_NONE:
		break;

	    case DASDF_CDL:
		ret = Disk::detectPartitions(systeminfo);
		break;

	    case DASDF_LDL: {
		max_primary = 1;
		unsigned long long s = cylinderToKb(cylinders());
		Partition *p = new Partition(*this, getPartName(1), getPartDevice(1), 1,
					     systeminfo, s, Region(0, cylinders()), PRIMARY);
		const ProcParts& parts = systeminfo.getProcParts();
		if (parts.getSize(p->device(), s))
		{
		    p->setSize(s);
		}
		addToList(p);
		ret = true;
	    } break;
	}

	y2mil("ret:" << ret << " partitions:" << vols.size() << " detected label:" << label);
	y2mil("geometry:" << geometry << " fmt:" << toString(fmt) << " readonly:" << ronly);

	return ret;
    }


    int
    Dasd::doResize(Volume* v)
    {
	return DASD_NOT_POSSIBLE;
    }


    int
    Dasd::resizePartition(Partition* p, unsigned long newCyl)
    {
	return DASD_NOT_POSSIBLE;
    }


    int
    Dasd::removePartition(unsigned nr)
    {
	y2mil("begin nr:" << nr);
	int ret = Disk::removePartition(nr);
	if (ret == 0)
	{
	    PartPair p = partPair(Partition::notDeleted);
	    changeNumbers(p.begin(), p.end(), nr, -1);
	}
	y2mil("ret:" << ret);
	return ret;
    }


    int
    Dasd::createPartition(PartitionType type, unsigned long start, unsigned long len,
			  string& device, bool checkRelaxed)
    {
	y2mil("begin type:" << toString(type) << " start:" << start << " len:" <<
	      len << " relaxed:" << checkRelaxed);
	int ret = createChecks(type, Region(start, len), checkRelaxed);
	int number = 0;
	if (ret == 0)
	{
	    number = availablePartNumber(type);
	    if (number == 0)
	    {
		ret = DISK_PARTITION_NO_FREE_NUMBER;
	    }
	    else
	    {
		PartPair p = partPair(Partition::notDeleted);
		number = 1;
		PartIter i = p.begin();
		while (i!=p.end() && i->cylStart() < start)
		{
		    number++;
		    ++i;
		}
		y2mil("number:" << number);
		changeNumbers(p.begin(), p.end(), number - 1, 1);
	    }
	}
	if (ret == 0)
	{
	    Partition * p = new Partition(*this, getPartName(number), getPartDevice(number), number,
					  cylinderToKb(len), Region(start, len), type);
	    p->setCreated();
	    device = p->device();
	    for (auto const &i : partPair())
	    {
		if (i.deleted() && i.nr()==p->nr() && !i.getCryptPwd().empty())
		{
		    y2mil("harvesting old password");
		    p->setCryptPwd(i.getCryptPwd());
		}
	    }
	    addToList(p);
	}
	y2mil("ret:" << ret);
	return ret;
    }


    void
    Dasd::getCommitActions(list<commitAction>& l) const
    {
	y2mil("begin:" << name() << " init_disk:" << init_disk);
	Disk::getCommitActions(l);
	if (init_disk)
	{
	    l.remove_if(stage_is(DECREASE));
	    l.push_front(commitAction(DECREASE, staticType(), dasdfmtText(false), this, true));
	}
    }


    Text
    Dasd::dasdfmtTexts(bool doing, const list<string>& devs)
    {
	Text txt;
	if (doing)
	{
	    // displayed text during action, %1$s is replaced by disk name (e.g. dasda)
	    txt = sformat(_("Executing dasdfmt for disk %1$s",
			    "Executing dasdfmt for disks %1$s", devs.size()),
			  boost::join(devs, " ").c_str());
	}
	else
	{
	    // displayed text during action, %1$s is replaced by disk name (e.g. dasda)
	    txt = sformat(_("Execute dasdfmt on disk %1$s",
			    "Execute dasdfmt on disks %1$s", devs.size()),
			  boost::join(devs, " ").c_str());
	}
	return txt;
    }


    Text
    Dasd::dasdfmtText(bool doing) const
    {
	list<string> tmp;
	tmp.push_back(dev);
	return dasdfmtTexts(doing, tmp);
    }


    int
    Dasd::commitChanges(CommitStage stage)
    {
	y2mil("name:" << name() << " stage:" << stage);
	int ret = 0;
	if (stage == DECREASE && init_disk)
	{
	    ret = doDasdfmt();
	}
	if (ret == 0)
	{
	    ret = Disk::commitChanges(stage);
	}
	y2mil("ret:" << ret);
	return ret;
    }


    static bool
    needDasdfmt(const Disk& d)
    {
	return d.isDasd() && d.initializeDisk();
    }


    int
    Dasd::doDasdfmt()
    {
	y2mil("dasd:" << device());

	int ret = 0;
	list<Disk*> dl;
	list<string> devs;
	getStorage()->getDiskList(needDasdfmt, dl);
	if (!dl.empty())
	{
	    for (auto const *i : dl)
	    {
		devs.push_back(undevDevice(i->device()));
	    }
	    y2mil("devs:" << devs);
	    getStorage()->showInfoCb(dasdfmtTexts(true, devs), silent);
	    for (auto &i : devs)
	    {
		i = "-f " + quote(normalizeDevice(i));
	    }
	    string cmd_line = DASDFMTBIN " -Y -P 4 -b 4096 -y -m 1 -d cdl " +
		boost::join(devs, " ");
	    y2mil("cmdline:" << cmd_line);
	    CallbackProgressBar cb = getStorage()->getCallbackProgressBarTheOne();
	    ProgressBar* progressbar = new DasdfmtProgressBar(cb);
	    SystemCmd cmd;
	    cmd.setOutputProcessor(progressbar);
	    if (execCheckFailed(cmd, cmd_line))
	    {
		ret = DASD_DASDFMT_FAILED;
	    }
	    if (ret == 0)
	    {
		SystemInfo systeminfo;
		for (auto *i : dl)
		{
		    Dasd * ds = static_cast<Dasd *>(i);
		    ds->detectPartitions(systeminfo);
		    ds->resetInitDisk();
		    ds->removeFromMemory();
		}
	    }
	    delete progressbar;
	}

	return ret;
    }


    int
    Dasd::initializeDisk(bool value)
    {
	y2mil("value:" << value << " old:" << init_disk);
	int ret = 0;
	if (init_disk != value)
	{
	    init_disk = value;
	    if (init_disk)
	    {
		new_geometry.heads = geometry.heads = 15;
		new_geometry.sectors = geometry.sectors = 12;
		y2mil("new geometry:" << geometry);
		size_k = geometry.sizeK();
		y2mil("new SizeK:" << size_k);
		ret = destroyPartitionTable("dasd");
	    }
	    else
	    {
		PartPair p = partPair();
		PartIter i = p.begin();
		list<Partition*> rem_list;
		while (i != p.end())
		{
		    if (i->deleted())
		    {
			i->setDeleted(false);
		    }
		    if (i->created())
		    {
			rem_list.push_back(&(*i));
		    }
		    ++i;
		}
		for (auto *pr : rem_list)
		{
		    if (!removeFromList(pr) && ret == 0)
			ret = DISK_REMOVE_PARTITION_LIST_ERASE;
		}
	    }
	}
	return ret;
    }


    string
    Dasd::defaultLabel() const
    {
	string ret = "dasd";
	y2mil("ret:" << ret);
	return ret;
    }


    std::ostream& operator<< (std::ostream& s, const Dasd& d)
    {
	s << dynamic_cast<const Disk&>(d);
	s << " fmt:" << toString(d.fmt);
	return s;
    }


    static const string dasd_type_names[] = {
	"NONE", "ECKD", "FBA"
    };

    const vector<string> EnumInfo<Dasd::DasdType>::names(dasd_type_names, dasd_type_names +
							 lengthof(dasd_type_names));

    static const string dasd_format_names[] = {
	"NONE", "LDL", "CDL"
    };

    const vector<string> EnumInfo<Dasd::DasdFormat>::names(dasd_format_names, dasd_format_names +
							   lengthof(dasd_format_names));

}
