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


#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H


#include <boost/noncopyable.hpp>

#include "storage/AppUtil.h"
#include "storage/ProcParts.h"
#include "storage/ProcMounts.h"
#include "storage/ProcMdstat.h"
#include "storage/Blkid.h"
#include "storage/Lsscsi.h"
#include "storage/Parted.h"
#include "storage/Dasdview.h"
#include "storage/DmCo.h"
#include "storage/DmraidCo.h"
#include "storage/DmmultipathCo.h"

namespace storage
{
    using std::map;
    using std::list;

    class CmdBtrfsShow
    {
    public:
	CmdBtrfsShow();
	struct Entry
	    {
	    list<string> devices;
	    };
	bool getEntry(const string& uuid, Entry& entry) const;
	list<string> getUuids() const;

    private:
	typedef map<string, Entry>::const_iterator const_iterator;

	map< string, Entry > fs;
    };

    class SystemInfo : boost::noncopyable
    {

    public:

	SystemInfo();
	~SystemInfo();

	const UdevMap& getUdevMap(const string& path) { return udevmaps.get(path); }
	const ProcParts& getProcParts() { return *procparts; }
	const ProcMounts& getProcMounts() { return *procmounts; }
	const ProcMdstat& getProcMdstat() { return *procmdstat; }
	const Blkid& getBlkid() { return *blkid; }
	const Lsscsi& getLsscsi() { return *lsscsi; }
	const Parted& getParted(const string& device) { return parteds.get(device); }
	const Fdasd& getFdasd(const string& device) { return fdasds.get(device); }
	const CmdDmsetup& getCmdDmsetup() { return *cmddmsetup; }
	const CmdDmraid& getCmdDmraid() { return *cmddmraid; }
	const CmdMultipath& getCmdMultipath() { return *cmdmultipath; }
	const CmdBtrfsShow& getCmdBtrfsShow() { return *cmdbtrfsshow; }

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

	template <class Type>
	class LazyObjects : boost::noncopyable
	{
	public:

	    const Type& get(const string& s)
	    {
		typename map<string, Type>::iterator pos = data.lower_bound(s);
		if (pos == data.end() || typename map<string, Type>::key_compare()(s, pos->first))
		{
		    Type tmp(s);
		    pos = data.insert(pos, typename map<string, Type>::value_type(s, tmp));
		}
		return pos->second;
	    }

	private:

	    map<string, Type> data;

	};

	LazyObjects<UdevMap> udevmaps;
	LazyObject<ProcParts> procparts;
	LazyObject<ProcMounts> procmounts;
	LazyObject<ProcMdstat> procmdstat;
	LazyObject<Blkid> blkid;
	LazyObject<Lsscsi> lsscsi;
	LazyObjects<Parted> parteds;
	LazyObjects<Fdasd> fdasds;
	LazyObject<CmdDmsetup> cmddmsetup;
	LazyObject<CmdDmraid> cmddmraid;
	LazyObject<CmdMultipath> cmdmultipath;
	LazyObject<CmdBtrfsShow> cmdbtrfsshow;

    };

}


#endif
