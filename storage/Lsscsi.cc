/*
 * Copyright (c) [2010-2014] Novell, Inc.
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


#include <boost/algorithm/string.hpp>

#include "storage/AppUtil.h"
#include "storage/SystemCmd.h"
#include "storage/StorageDefines.h"
#include "storage/StorageTmpl.h"
#include "storage/Lsscsi.h"
#include "storage/Disk.h"
#include "storage/Enum.h"


namespace storage
{
    using namespace std;


    Lsscsi::Lsscsi(bool do_probe)
    {
	if (do_probe)
	    probe();
    }


    void
    Lsscsi::probe()
    {
	SystemCmd cmd(LSSCSIBIN " --transport");
	if (cmd.retcode() == 0)
	    parse(cmd.stdout());
    }


    void
    Lsscsi::parse(const vector<string>& lines)
    {
	data.clear();

	for (vector<string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
	{
	    string line = boost::replace_all_copy(*it, " usb: ", " usb:");

	    string transport = extractNthWord(2, line);
	    string device = extractNthWord(3, line);

	    if (!boost::starts_with(device, "/dev/"))
		continue;

	    Entry entry;

	    if (boost::starts_with(transport, "sbp:"))
		entry.transport = SBP;
	    else if (boost::starts_with(transport, "ata:"))
		entry.transport = ATA;
	    else if (boost::starts_with(transport, "fc:"))
		entry.transport = FC;
	    else if (boost::starts_with(transport, "iqn"))
		entry.transport = ISCSI;
	    else if (boost::starts_with(transport, "sas:"))
		entry.transport = SAS;
	    else if (boost::starts_with(transport, "sata:"))
		entry.transport = SATA;
	    else if (boost::starts_with(transport, "spi:"))
		entry.transport = SPI;
	    else if (boost::starts_with(transport, "usb:"))
		entry.transport = USB;

	    if( entry.transport == FC )
		{
		string link;
		if( readlink( Disk::sysfsPath(device)+"/device", link))
		    {
		    y2mil( "sysfs:" << Disk::sysfsPath(device) << 
			   " link:" << link );
		    string::size_type pos = link.rfind( '/' ) + 1;
		    string nums = link.substr( pos, link.find_first_not_of( "0123456789", pos ));
		    unsigned num = 0;
		    nums >> num;
		    y2mil( "nums:" << nums << " num:" << num );
		    string symname = "/sys/class/fc_host/host" + 
		                     decString(num) + "/symbolic_name";
		    ifstream tmpf( symname.c_str() );
		    string line;
		    getline( tmpf, line );
		    y2mil( "line:" << line );
		    if( line.find( "over eth" )!=string::npos )
			{
			entry.transport = FCOE;
			y2mil( "FCoE device: " << device );
			}
		    }
		}

	    data[device] = entry;
	}

	for (const_iterator it = data.begin(); it != data.end(); ++it)
	    y2mil("data[" << it->first << "] -> " << it->second);
    }


    bool
    Lsscsi::getEntry(const string& device, Entry& entry) const
    {
	const_iterator i = data.find(device);
	if (i == data.end())
	    return false;

	entry = i->second;
	return true;
    }


    std::ostream& operator<<(std::ostream& s, const Lsscsi& lsscsi)
    {
	for (Lsscsi::const_iterator it = lsscsi.data.begin(); it != lsscsi.data.end(); ++it)
	    s << "data[" << it->first << "] -> " << it->second << endl;

	return s;
    }


    std::ostream& operator<<(std::ostream& s, const Lsscsi::Entry& entry)
    {
	return s << "transport:" << toString(entry.transport);
    }

}
