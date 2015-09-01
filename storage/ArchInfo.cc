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


#include <string.h>
#include <sys/utsname.h>

#include "storage/ArchInfo.h"
#include "storage/Utils/AsciiFile.h"
#include "storage/Utils/AppUtil.h"
#include "storage/StorageTypes.h"


namespace storage
{

    ArchInfo::ArchInfo()
	: arch("unknown"), ppc_mac(false), ppc_pegasos(false), efiboot(false), ppc_p8(false)
    {
    }


    void
    ArchInfo::readData(const xmlNode* node)
    {
	getChildValue(node, "arch", arch);

	if (!getChildValue(node, "efiboot", efiboot))
	    efiboot = false;
    }


    void
    ArchInfo::saveData(xmlNode* node) const
    {
	setChildValue(node, "arch", arch);

	setChildValueIf(node, "efiboot", efiboot, efiboot);
    }


    void
    ArchInfo::probe()
    {
	struct utsname buf;
	if (uname(&buf) == 0)
	    arch = buf.machine;
	else
	    arch = "unknown";

	if (is_ppc())
	{
	    AsciiFile cpuinfo("/proc/cpuinfo");
	    vector<string>::const_iterator it = find_if(cpuinfo.lines(), string_starts_with("machine\t"));
	    if (it != cpuinfo.lines().end())
	    {
		y2mil("line:" << *it);

		string tmp1 = extractNthWord(2, *it);
		y2mil("tmp1:" << tmp1);
		ppc_mac = boost::starts_with(tmp1, "PowerMac") || boost::starts_with(tmp1, "PowerBook");
		ppc_pegasos = boost::starts_with(tmp1, "EFIKA5K2");

		if (!ppc_mac && !ppc_pegasos)
		{
		    string tmp2 = extractNthWord(3, *it);
		    y2mil("tmp2:" << tmp2);
		    ppc_pegasos = boost::starts_with(tmp2, "Pegasos");
		}
	    }
	    it = find_if(cpuinfo.lines(), string_starts_with("cpu\t"));
	    if (it != cpuinfo.lines().end())
	    {
		y2mil("line:" << *it);
		string tmp1 = extractNthWord(2, *it);
		y2mil("tmp1:" << tmp1);
		ppc_p8 = boost::starts_with(tmp1, "POWER8");
	    }
	}

	if (is_ia64())
	{
	    efiboot = true;
	}
	else
	{
	    efiboot = checkDir("/sys/firmware/efi/vars");
	}

	// Efi detection can be overridden by EFI env.var., see bsc
	// #937067. Reading /etc/install.inf is not reliable, see bsc #806490.
	const char* tenv1 = getenv("EFI");
	if (tenv1)
	{
	    efiboot = string(tenv1) == "1";
	}

	// Efi detection can also be overridden by LIBSTORAGE_EFI env.var. for
	// compatibility.
	const char* tenv2 = getenv("LIBSTORAGE_EFI");
	if (tenv2)
	{
	    efiboot = string(tenv2) == "yes";
	}
    }


    bool
    ArchInfo::is_ia64() const
    {
	return boost::starts_with(arch, "ia64");
    }


    bool
    ArchInfo::is_ppc() const
    {
	return boost::starts_with(arch, "ppc");
    }


    bool
    ArchInfo::is_ppc64le() const
    {
	return boost::starts_with(arch, "ppc64le");
    }


    bool
    ArchInfo::is_s390() const
    {
	return boost::starts_with(arch, "s390");
    }


    bool
    ArchInfo::is_sparc() const
    {
	return boost::starts_with(arch, "sparc");
    }


    bool
    ArchInfo::is_x86() const
    {
	return arch == "i386" || arch == "i486" || arch == "i586" || arch == "i686" ||
	    arch == "x86_64";
    }


    std::ostream& operator<<(std::ostream& s, const ArchInfo& archinfo)
    {
	return s << "arch:" << archinfo.arch << " ppc_mac:" << archinfo.ppc_mac
		 << " ppc_pegasos:" << archinfo.ppc_pegasos << " efiboot:"
		 << archinfo.efiboot << " ppc_p8:" << archinfo.ppc_p8;
    }

}
