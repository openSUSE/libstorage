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


#ifndef PROC_MDSTAT_H
#define PROC_MDSTAT_H


namespace storage
{

    class ProcMdstat
    {
    public:

	ProcMdstat();

	struct Entry
	{
	    Entry() : md_type(RAID_UNK), md_parity(PAR_DEFAULT), size_k(0), chunk_k(0),
		      readonly(false), inactive(false), is_container(false), has_container(false) {}

	    MdType md_type;
	    MdParity md_parity;

	    string super;

	    unsigned long long size_k;
	    unsigned long chunk_k;

	    bool readonly;
	    bool inactive;

	    list<string> devices;
	    list<string> spares;

	    bool is_container;

	    bool has_container;
	    string container_name;
	    string container_member;
	};

	friend std::ostream& operator<<(std::ostream& s, const Entry& entry);

	list<string> getEntries() const;

	bool getEntry(const string& name, Entry& entry) const;

	typedef map<string, Entry>::const_iterator const_iterator;

	const_iterator begin() const { return data.begin(); }
	const_iterator end() const { return data.end(); }

    private:

	Entry parse(const string& line1, const string& line2);

	map<string, Entry> data;

    };


    struct MdadmDetails
    {
	string uuid;
	string devname;
	string metadata;
    };

    bool getMdadmDetails(const string& dev, MdadmDetails& details);


    bool isImsmPlatform();

}


#endif
