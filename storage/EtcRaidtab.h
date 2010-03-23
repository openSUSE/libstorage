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


#ifndef ETC_RAIDTAB_H
#define ETC_RAIDTAB_H

#include <string>
#include <map>

#include "storage/AsciiFile.h"
#include "storage/Storage.h"


namespace storage
{

    class EtcRaidtab
    {
    public:

	EtcRaidtab(const Storage* sto, const string& prefix = "");
	~EtcRaidtab();

	// From this structure line 'ARRAY' will be build in config file.
	// Not all fields are mandatory
	// If container is present then container line will be build
	// before volume line.
	struct mdconf_info
	{
	    string device;
	    string uuid;

	    bool container_present;

	    /* following members only valid if container_present is true */
	    string container_member;
	    string container_metadata;
	    string container_uuid;
	};


	bool updateEntry(const mdconf_info& info);

	bool removeEntry(const string& uuid);

    protected:

	void setDeviceLine(const string& line);
	void setAutoLine(const string& line);
	void setArrayLine(const string& line, const string& uuid);

	string ContLine(const mdconf_info& info) const;
	string ArrayLine(const mdconf_info& info) const;

	vector<string>::const_iterator findArray(const string& uuid) const;

	string getUuid(const string& line) const;

	const Storage* sto;

	AsciiFile mdadm;

    };

}


#endif
