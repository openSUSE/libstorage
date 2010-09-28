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

#include "storage/ProcParts.h"
#include "storage/ProcMounts.h"
#include "storage/ProcMdstat.h"
#include "storage/Blkid.h"
#include "storage/Lsscsi.h"
#include "storage/DmCo.h"
#include "storage/DmraidCo.h"
#include "storage/DmmultipathCo.h"


namespace storage
{

    class SystemInfo : boost::noncopyable
    {

    public:

	SystemInfo();
	~SystemInfo();

	const UdevMap& getUdevMap(const string& path);
	const ProcParts& getProcParts() { return *procparts; }
	const ProcMounts& getProcMounts() { return *procmounts; }
	const ProcMdstat& getProcMdstat() { return *procmdstat; }
	const Blkid& getBlkid() { return *blkid; }
	const Lsscsi& getLsscsi() { return *lsscsi; }
	const CmdDmsetup& getCmdDmsetup() { return *cmddmsetup; }
	const CmdDmraid& getCmdDmraid() { return *cmddmraid; }
	const CmdMultipath& getCmdMultipath() { return *cmdmultipath; }

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

	map<string, UdevMap> udevmaps;

	LazyObject<ProcParts> procparts;
	LazyObject<ProcMounts> procmounts;
	LazyObject<ProcMdstat> procmdstat;
	LazyObject<Blkid> blkid;
	LazyObject<Lsscsi> lsscsi;
	LazyObject<CmdDmsetup> cmddmsetup;
	LazyObject<CmdDmraid> cmddmraid;
	LazyObject<CmdMultipath> cmdmultipath;

    };

}


#endif
