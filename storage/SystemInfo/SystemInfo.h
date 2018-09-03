/*
 * Copyright (c) [2004-2014] Novell, Inc.
 * Copyright (c) [2015] SUSE LLC
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


#include <tuple>
#include <boost/noncopyable.hpp>

#include "storage/SystemInfo/ProcParts.h"
#include "storage/SystemInfo/ProcMounts.h"
#include "storage/SystemInfo/ProcMdstat.h"
#include "storage/SystemInfo/CmdBlkid.h"
#include "storage/SystemInfo/CmdLsattr.h"
#include "storage/SystemInfo/CmdLsscsi.h"
#include "storage/SystemInfo/CmdParted.h"
#include "storage/SystemInfo/CmdDasdview.h"
#include "storage/SystemInfo/CmdDmsetup.h"
#include "storage/SystemInfo/CmdCryptsetup.h"
#include "storage/SystemInfo/CmdDmraid.h"
#include "storage/SystemInfo/CmdMultipath.h"
#include "storage/SystemInfo/CmdBtrfs.h"
#include "storage/SystemInfo/CmdLvm.h"
#include "storage/SystemInfo/DevAndSys.h"


namespace storage
{
    using std::map;
    using std::list;


    class SystemInfo : private boost::noncopyable
    {

    public:

	SystemInfo();
	~SystemInfo();

	const Dir& getDir(const string& path) { return dirs.get(path, path); }
	const UdevMap& getUdevMap(const string& path) { return udevmaps.get(path, path); }
	const MdLinks& getMdLinks() { return mdlinks.get(); }
	const ProcParts& getProcParts() { return procparts.get(); }
	const ProcMounts& getProcMounts() { return procmounts.get(); }
	const ProcMdstat& getProcMdstat() { return procmdstat.get(); }
	const MdadmDetail& getMdadmDetail(const string& device) { return mdadmdetails.get(device, device); }
	const MdadmExamine& getMdadmExamine(const list<string>& devices) { return mdadmexamines.get(devices, devices); }
	const Blkid& getBlkid() { return blkid.get(); }
	const Lsscsi& getLsscsi() { return lsscsi.get(); }
	const Parted& getParted(const string& device) { return parteds.get(device, device); }
	const Dasdview& getDasdview(const string& device) { return dasdviews.get(device, device); }
	const CmdDmsetupInfo& getCmdDmsetupInfo() { return cmddmsetupinfo.get(); }
	const CmdCryptsetup& getCmdCryptsetup(const string& name) { return cmdcryptsetups.get(name, name); }
	const CmdDmraid& getCmdDmraid() { return cmddmraid.get(); }
	const CmdMultipath& getCmdMultipath() { return cmdmultipath.get(); }
	const CmdBtrfsShow& getCmdBtrfsShow() { return cmdbtrfsshow.get(); }
	const CmdVgs& getCmdVgs() { return cmdvgs.get(); }
	const CmdVgdisplay& getCmdVgdisplay(const string& name) { return vgdisplays.get(name, name); }
	const MajorMinor& getMajorMinor(const string& device) { return majorminors.get(device, device); }

	// Device is only used for the cache-key.
	bool isCmdBtrfsSubvolumesCached(const string& device) const
	    { return cmdbtrfssubvolumes.includes(device); }
	const CmdBtrfsSubvolumes& getCmdBtrfsSubvolumes(const string& device, const string& mount_point)
	    { return cmdbtrfssubvolumes.get(device, mount_point); }

	// Device is only used for the cache-key.
	bool isCmdLsattrCached(const string& device, const string& path) const
	    { return cmdlsattr.includes(CmdLsattrKey(device, path)); }
	const CmdLsattr& getCmdLsattr(const string& device, const string& mount_point, const string& path)
	    { return cmdlsattr.get(CmdLsattrKey(device, path), mount_point, path); }

    private:

	// The key for the map needs operator< which std::tuple provides.
	using CmdLsattrKey = std::tuple<string, string>;

	/* LazyObject and LazyObjects cache the object and a potential
	   exception during object construction. HelperBase does the common
	   part. */

	template <class Object, typename... Args>
	class HelperBase
	{
	public:

	    const Object& get(Args... args)
            {
		if (ep)
                    std::rethrow_exception(ep);

		if (!object)
		{
		    try
		    {
			object.reset(new Object(args...));
		    }
		    catch (const std::exception &e)
		    {
			ep = std::current_exception();
                        std::rethrow_exception(ep);
		    }
		}

		return *object;
	    }

	private:

            std::shared_ptr<Object> object;
            std::exception_ptr ep;
	};

	template <class Object>
	class LazyObject : public HelperBase<Object>, private boost::noncopyable
	{
	};

	template <class Object, class Key, typename... Args>
	class LazyObjects : private boost::noncopyable
	{
	public:

	    typedef HelperBase<Object, Args...> Helper;

	    bool includes(const Key& key) const
	    {
		typename map<Key, Helper>::const_iterator pos = data.lower_bound(key);
		return pos != data.end() && !typename map<Key, Helper>::key_compare()(key, pos->first);
	    }

	    const Object& get(const Key& key, Args... args)
	    {
		typename map<Key, Helper>::iterator pos = data.lower_bound(key);
		if (pos == data.end() || typename map<Key, Helper>::key_compare()(key, pos->first))
		    pos = data.insert(pos, typename map<Key, Helper>::value_type(key, Helper()));
		return pos->second.get(args...);
	    }

	private:

	    map<Key, Helper> data;

	};

	LazyObjects<Dir, string, string> dirs;
	LazyObjects<UdevMap, string, string> udevmaps;
	LazyObject<MdLinks> mdlinks;
	LazyObject<ProcParts> procparts;
	LazyObject<ProcMounts> procmounts;
	LazyObject<ProcMdstat> procmdstat;
	LazyObjects<MdadmDetail, string, string> mdadmdetails;
	LazyObjects<MdadmExamine, list<string>, list<string>> mdadmexamines;
	LazyObject<Blkid> blkid;
	LazyObject<Lsscsi> lsscsi;
	LazyObjects<Parted, string, string> parteds;
	LazyObjects<Dasdview, string, string> dasdviews;
	LazyObject<CmdDmsetupInfo> cmddmsetupinfo;
	LazyObjects<CmdCryptsetup, string, string> cmdcryptsetups;
	LazyObject<CmdDmraid> cmddmraid;
	LazyObject<CmdMultipath> cmdmultipath;
	LazyObject<CmdBtrfsShow> cmdbtrfsshow;
	LazyObject<CmdVgs> cmdvgs;
	LazyObjects<CmdVgdisplay, string, string> vgdisplays;
	LazyObjects<MajorMinor, string, string> majorminors;
	LazyObjects<CmdBtrfsSubvolumes, string, string> cmdbtrfssubvolumes;
	LazyObjects<CmdLsattr, CmdLsattrKey, string, string> cmdlsattr;

    };

}


#endif
