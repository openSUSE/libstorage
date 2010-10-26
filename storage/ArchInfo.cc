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


#include <string.h>
#include <sys/utsname.h>

#include "storage/ArchInfo.h"
#include "storage/AsciiFile.h"
#include "storage/StorageTypes.h"


namespace storage
{

    ArchInfo::ArchInfo()
	: arch("i386"), is_ppc_mac(false), is_ppc_pegasos(false), is_efiboot(false)
    {
    }


    void
    ArchInfo::readData(const xmlNode* node)
    {
	getChildValue(node, "arch", arch);

	if (!getChildValue(node, "efiboot", is_efiboot))
	    is_efiboot = false;
    }


    void
    ArchInfo::saveData(xmlNode* node) const
    {
	setChildValue(node, "arch", arch);

	if (is_efiboot)
	    setChildValue(node, "efiboot", is_efiboot);
    }


    void
    ArchInfo::detect(bool instsys)
    {
	arch = "i386";
	struct utsname buf;
	if (uname(&buf) == 0)
	{
	    if (strncmp(buf.machine, "ppc", 3) == 0)
	    {
		arch = "ppc";
	    }
	    else if (strncmp(buf.machine, "x86_64", 5) == 0)
	    {
		arch = "x86_64";
	    }
	    else if (strncmp(buf.machine, "ia64", 4) == 0)
	    {
		arch = "ia64";
	    }
	    else if (strncmp(buf.machine, "s390", 4) == 0)
	    {
		arch = "s390";
	    }
	    else if (strncmp(buf.machine, "sparc", 5) == 0)
	    {
		arch = "sparc";
	    }
	}

	if (arch == "ppc")
	{
	    AsciiFile cpuinfo("/proc/cpuinfo");
	    vector<string>::const_iterator it = find_if(cpuinfo.lines(), string_starts_with("machine\t"));
	    if (it != cpuinfo.lines().end())
	    {
		y2mil("line:" << *it);

		string tmp1 = extractNthWord(2, *it);
		y2mil("tmp1:" << tmp1);
		is_ppc_mac = boost::starts_with(tmp1, "PowerMac") || boost::starts_with(tmp1, "PowerBook");
		is_ppc_pegasos = boost::starts_with(tmp1, "EFIKA5K2");

		if (!is_ppc_mac && !is_ppc_pegasos)
		{
		    string tmp2 = extractNthWord(3, *it);
		    y2mil("tmp2:" << tmp2);
		    is_ppc_pegasos = boost::starts_with(tmp2, "Pegasos");
		}
	    }
	}

	if (arch == "ia64")
	{
	    is_efiboot = true;
	}
	else
	{
	    string val;
	    if (instsys)
	    {
		InstallInfFile ii("/etc/install.inf");
		if (ii.getValue("EFI", val))
		    is_efiboot = val == "1";
	    }
	    else
	    {
		SysconfigFile sc("/etc/sysconfig/bootloader");
		if (sc.getValue("LOADER_TYPE", val))
		    is_efiboot = val == "elilo";
	    }
	}
    }


    std::ostream& operator<<(std::ostream& s, const ArchInfo& archinfo)
    {
	return s << "arch:" << archinfo.arch << " is_ppc_mac:" << archinfo.is_ppc_mac
		 << " is_ppc_pegasos:" << archinfo.is_ppc_pegasos << " is_efiboot:"
		 << archinfo.is_efiboot;
    }

}
