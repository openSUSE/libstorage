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


#include "storage/SystemInfo.h"
#include "storage/ProcParts.h"
#include "storage/ProcMounts.h"
#include "storage/Blkid.h"
#include "storage/DmCo.h"
#include "storage/DmraidCo.h"
#include "storage/DmmultipathCo.h"


namespace storage
{

    SystemInfo::SystemInfo()
	: procparts(NULL), procmounts(NULL), blkid(NULL), cmddmsetup(NULL), cmddmraid(NULL),
	  cmdmultipath(NULL)
    {
	y2deb("constructed SystemInfo");
    }


    SystemInfo::~SystemInfo()
    {
	delete procparts;
	delete procmounts;
	delete blkid;
	delete cmddmsetup;
	delete cmddmraid;
	delete cmdmultipath;

	y2deb(" destructed SystemInfo");
    }


    const ProcParts&
    SystemInfo::getProcParts()
    {
	if (!procparts)
	    procparts = new ProcParts();
	return *procparts;
    }


    const ProcMounts&
    SystemInfo::getProcMounts()
    {
	if (!procmounts)
	    procmounts = new ProcMounts();
	return *procmounts;
    }


    const Blkid&
    SystemInfo::getBlkid()
    {
	if (!blkid)
	    blkid = new Blkid();
	return *blkid;
    }


    const CmdDmsetup&
    SystemInfo::getCmdDmsetup()
    {
	if (!cmddmsetup)
	    cmddmsetup = new CmdDmsetup();
	return *cmddmsetup;
    }


    const CmdDmraid&
    SystemInfo::getCmdDmraid()
    {
	if (!cmddmraid)
	    cmddmraid = new CmdDmraid();
	return *cmddmraid;
    }


    const CmdMultipath&
    SystemInfo::getCmdMultipath()
    {
	if (!cmdmultipath)
	    cmdmultipath = new CmdMultipath();
	return *cmdmultipath;
    }

}
