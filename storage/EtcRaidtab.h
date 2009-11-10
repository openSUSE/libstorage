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

class EtcRaidtab
    {
    public:
	EtcRaidtab( const string& prefix="" );
	~EtcRaidtab();
	void updateEntry(unsigned num, const string&);
	void removeEntry( unsigned num );
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

	int mdadm_dev_line;
	map<unsigned, entry> mtab;
	AsciiFile mdadm;
    };

}


#endif
