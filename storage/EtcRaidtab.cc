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


#include "storage/AppUtil.h"
#include "storage/EtcRaidtab.h"
#include "storage/AsciiFile.h"
#include "storage/StorageTypes.h"


namespace storage
{
    using namespace std;


    EtcRaidtab::EtcRaidtab(const Storage* sto, const string& prefix)
	: sto(sto), mdadm(prefix + "/etc/mdadm.conf")
    {
    }


    EtcRaidtab::~EtcRaidtab()
    {
    }


    bool
    EtcRaidtab::updateEntry(const mdconf_info& info)
    {
	y2mil("uuid:" << info.uuid << " device:" << info.device);

	if (info.uuid.empty())
	{
	    y2err("empty UUID");
	    return false;
	}

	if (info.container_present && info.container_uuid.empty())
	{
	    y2err("empty UUID for container");
	    return false;
	}

	if (info.container_present)
	    setArrayLine(ContLine(info), info.container_uuid);

	setArrayLine(ArrayLine(info), info.uuid);

	setDeviceLine("DEVICE containers partitions");

	if (sto->hasIScsiDisks())
	    setAutoLine("AUTO -all");

	mdadm.save();

	return true;
    }


    bool
    EtcRaidtab::removeEntry(const string& uuid)
    {
	y2mil("uuid:" << uuid);

	const vector<string>& lines = mdadm.lines();
	vector<string>::const_iterator it = findArray(uuid);
	if (it == lines.end())
	{
	    y2war("line not found");
	    return false;
	}

	mdadm.remove(distance(lines.begin(), it), 1);

	mdadm.save();

	return true;
    }


    void
    EtcRaidtab::setDeviceLine(const string& line)
    {
	const vector<string>& lines = mdadm.lines();
	vector<string>::const_iterator it = find_if(lines, string_starts_with("DEVICE"));
	if (it == lines.end())
	    mdadm.insert(0, line);
	else
	    mdadm.replace(distance(lines.begin(), it), 1, line);
    }


    void
    EtcRaidtab::setAutoLine(const string& line)
    {
	const vector<string>& lines = mdadm.lines();
	vector<string>::const_iterator it = find_if(lines, string_starts_with("AUTO"));
	if (it == lines.end())
	    mdadm.insert(0, line);
	else
	    mdadm.replace(distance(lines.begin(), it), 1, line);
    }


    void
    EtcRaidtab::setArrayLine(const string& line, const string& uuid)
    {
	const vector<string>& lines = mdadm.lines();
	vector<string>::const_iterator it = findArray(uuid);
	if (it == lines.end())
	    mdadm.append(line);
	else
	    mdadm.replace(distance(lines.begin(), it), 1, line);
    }


    string
    EtcRaidtab::ContLine(const mdconf_info& info) const
    {
	string line = "ARRAY";
	line += " metadata=" + info.container_metadata;
	line += " UUID=" + info.container_uuid;
	return line;
    }


    string
    EtcRaidtab::ArrayLine(const mdconf_info& info) const
    {
	string line = "ARRAY " + info.device;
	if (info.container_present)
	{
	    line += " container=" + info.container_uuid;
	    line += " member=" + info.container_member;
	}
	line += " UUID=" + info.uuid;
	return line;
    }


    vector<string>::const_iterator
    EtcRaidtab::findArray(const string& uuid) const
    {
	const vector<string>& lines = mdadm.lines();

	if (uuid.empty())
	{
	    y2err("empty UUID");
	    return lines.end();
	}

	for (vector<string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
	{
	    if (boost::starts_with(*it, "ARRAY"))
	    {
		string tmp = getUuid(*it);
		if (!tmp.empty() && tmp == uuid)
		    return it;
	    }
	}

	return lines.end();
    }


    string
    EtcRaidtab::getUuid(const string& line) const
    {
	string::size_type pos1 = line.find("UUID=");
	if (pos1 != string::npos)
	{
	    pos1 += 5;
	    string::size_type pos2 = line.find_first_not_of("0123456789abcdefABCDEF:", pos1);
	    return line.substr(pos1, pos2 - pos1);
	}
	else
	{
	    return "";
	}
    }

}
