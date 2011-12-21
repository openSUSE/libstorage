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


#ifndef DASDVIEW_H
#define DASDVIEW_H

#include "storage/StorageInterface.h"
#include "storage/Geometry.h"
#include "storage/Dasd.h"


namespace storage
{
    class SystemCmd;


    class Dasdview
    {

    public:

	Dasdview(const string& device);

	const Geometry& getGeometry() const { return geometry; }
	Dasd::DasdFormat getDasdFormat() const { return dasd_format; }
	Dasd::DasdType getDasdType() const { return dasd_type; }

    private:

	Geometry geometry;
	Dasd::DasdFormat dasd_format;
	Dasd::DasdType dasd_type;

	void scanGeometry(SystemCmd& cmd);

    };


    class Fdasd
    {

    public:

	Fdasd(const string& device);

	struct Entry
	{
	    Entry() : num(0) {}

	    unsigned num;
	    Region cylRegion;
	    Region headRegion;

	    friend std::ostream& operator<<(std::ostream& s, const Entry& e);
	};

	const Geometry& getGeometry() const { return geometry; }

	typedef vector<Entry>::const_iterator const_iterator;

	const vector<Entry>& getEntries() const { return entries; }

	bool getEntry(unsigned num, Entry& entry) const;

    private:

	typedef vector<Entry>::iterator iterator;

	Geometry geometry;
	vector<Entry> entries;

	void scanGeometry(SystemCmd& cmd);
	void scanEntryLine(const string& line);

    };

}

#endif
