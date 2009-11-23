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


#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H


#include <boost/noncopyable.hpp>


namespace storage
{
    class ProcParts;
    class ProcMounts;
    class Blkid;
    class CmdDmraid;
    class CmdMultipath;


    class SystemInfo : boost::noncopyable
    {

    public:

	SystemInfo();
	~SystemInfo();

	const ProcParts& getProcParts();
	const ProcMounts& getProcMounts();
	const Blkid& getBlkid();
	const CmdDmraid& getCmdDmraid();
	const CmdMultipath& getCmdMultipath();

    private:

	ProcParts* procparts;
	ProcMounts* procmounts;
	Blkid* blkid;
	CmdDmraid* cmddmraid;
	CmdMultipath* cmdmultipath;

    };

}


#endif
