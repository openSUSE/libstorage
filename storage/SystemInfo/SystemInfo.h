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


#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H


#include <boost/noncopyable.hpp>

#include "storage/AppUtil.h"
#include "storage/SystemInfo/ProcParts.h"
#include "storage/SystemInfo/ProcMounts.h"
#include "storage/SystemInfo/ProcMdstat.h"
#include "storage/SystemInfo/CmdBlkid.h"
#include "storage/SystemInfo/CmdLsscsi.h"
#include "storage/SystemInfo/CmdParted.h"
#include "storage/SystemInfo/CmdDasdview.h"
#include "storage/SystemInfo/CmdDmsetup.h"
#include "storage/SystemInfo/CmdCryptsetup.h"
#include "storage/SystemInfo/CmdDmraid.h"
#include "storage/SystemInfo/CmdMultipath.h"
#include "storage/SystemInfo/CmdBtrfs.h"
#include "storage/SystemInfo/CmdLvm.h"


namespace storage
{
    using std::map;
    using std::list;


    class SystemInfo : boost::noncopyable
    {

    public:

	SystemInfo();
	~SystemInfo();

	const UdevMap& getUdevMap(const string& path) { return udevmaps.get(path); }
	const ProcParts& getProcParts() { return *procparts; }
	const ProcMounts& getProcMounts() { return *procmounts; }
	const ProcMdstat& getProcMdstat() { return *procmdstat; }
	const MdadmDetails& getMdadmDetails(const string device) { return mdadmdetails.get(device); }
	const MdadmExamine& getMdadmExamine(const list<string>& devices) { return mdadmexamines.get(devices); }
	const Blkid& getBlkid() { return *blkid; }
	const Lsscsi& getLsscsi() { return *lsscsi; }
	const Parted& getParted(const string& device) { return parteds.get(device); }
	const Dasdview& getDasdview(const string& device) { return dasdviews.get(device); }
	const CmdDmsetupInfo& getCmdDmsetupInfo() { return *cmddmsetupinfo; }
	const CmdCryptsetup& getCmdCryptsetup(const string& name) { return cmdcryptsetups.get(name); }
	const CmdDmraid& getCmdDmraid() { return *cmddmraid; }
	const CmdMultipath& getCmdMultipath() { return *cmdmultipath; }
	const CmdBtrfsShow& getCmdBtrfsShow() { return *cmdbtrfsshow; }
	const CmdVgs& getCmdVgs() { return *cmdvgs; }
	const CmdVgdisplay& getCmdVgdisplay(const string& name) { return vgdisplays.get(name); }

    private:

	template <class Type>
	class LazyObject : boost::noncopyable
	{
	public:

	    LazyObject() : ptr(NULL) {}
	    ~LazyObject() { delete ptr; }

	    const Type& operator*() { if (!ptr) ptr = new Type(); return *ptr; }

	private:

	    Type* ptr;

	};

	template <class Type, class Key = string>
	class LazyObjects : boost::noncopyable
	{
	public:

	    const Type& get(const Key& k)
	    {
		typename map<Key, Type>::iterator pos = data.lower_bound(k);
		if (pos == data.end() || typename map<Key, Type>::key_compare()(k, pos->first))
		{
		    Type tmp(k);
		    pos = data.insert(pos, typename map<Key, Type>::value_type(k, tmp));
		}
		return pos->second;
	    }

	private:

	    map<Key, Type> data;

	};

	LazyObjects<UdevMap> udevmaps;
	LazyObject<ProcParts> procparts;
	LazyObject<ProcMounts> procmounts;
	LazyObject<ProcMdstat> procmdstat;
	LazyObjects<MdadmDetails> mdadmdetails;
	LazyObjects<MdadmExamine, list<string>> mdadmexamines;
	LazyObject<Blkid> blkid;
	LazyObject<Lsscsi> lsscsi;
	LazyObjects<Parted> parteds;
	LazyObjects<Dasdview> dasdviews;
	LazyObject<CmdDmsetupInfo> cmddmsetupinfo;
	LazyObjects<CmdCryptsetup> cmdcryptsetups;
	LazyObject<CmdDmraid> cmddmraid;
	LazyObject<CmdMultipath> cmdmultipath;
	LazyObject<CmdBtrfsShow> cmdbtrfsshow;
	LazyObject<CmdVgs> cmdvgs;
	LazyObjects<CmdVgdisplay> vgdisplays;

    };

}


#endif
