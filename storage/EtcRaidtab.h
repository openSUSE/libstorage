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


#ifndef ETC_RAIDTAB_H
#define ETC_RAIDTAB_H

#include <string>
#include <map>

#include "storage/AsciiFile.h"


namespace storage
{
    class Md;
    class MdPartCo;


class EtcRaidtab
    {
    public:
	EtcRaidtab( const string& prefix="" );
	~EtcRaidtab();
	void updateEntry(unsigned num, const string&);
	void removeEntry( unsigned num );

	// From this structure line 'ARRAY' will be build in config file.
	// Not all fields are mandatory
	// If container is present then container line will be build
	// before volume line.
	struct mdconf_info
	{
	  bool container_present; // container present
	  struct {
	    string metadata;  // metadata
	    string md_uuid;    // md uuid
	  } container_info;
	  string fs_name;     // word after 'ARRAY'.
	  string md_uuid;     // md uuid
	  string member;      // member of container (if container is present)
	};
	bool updateEntry(const mdconf_info& info);
	bool removeEntry(const mdconf_info& into);

    protected:
	struct entry
	    {
	    entry() : first(0), last(0) {}
	    entry(unsigned f, unsigned l) : first(f), last(l) {}
	    unsigned first;
	    unsigned last;
	    };

	void updateMdadmFile();
	void buildMdadmMap();
	void buildMdadmMap2();

	/* Extracts UUID=uuidNumber from line. */
	string getUUID(const string& line) const;

        enum lineType
        {
          DEVICE=0,
          ARRAY,
          MAILFROM,
          PROGRAM,
          CREATE,
          HOMEHOST,
          AUTO,
          COMMENT,
          EMPTY,
          UNKNOWN
        };

	lineType getLineType(const string& line) const;

	//Gets full array line, possibly consisted of several lines.
	//line - will be updated and will point to first line that does not
	//       belong to initial array line.
	//uuid - will be filled if found.
	bool getArrayLine(unsigned& line, string& uuid);
	string ContLine(const mdconf_info& info);
	string ArrayLine(const mdconf_info& info);
	bool updateContainer(const mdconf_info& info);

	int mdadm_dev_line;
	map<unsigned, entry> mtab;
	map<string, entry> uuidtab; // search by uuid, only for ARRAY lines.
	AsciiFile mdadm;
    };

}


#endif
