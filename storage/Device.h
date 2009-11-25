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


#ifndef DEVICE_H
#define DEVICE_H


#include <string>

#include "storage/StorageTypes.h"


namespace storage
{
    using std::string;


    class AsciiFile;


    class Device
    {
    public:

	Device();
	Device(const string& nm, const string& dev);
	Device(const AsciiFile& file);
	Device(const Device&);
	virtual ~Device();

	const string& name() const { return nm; }
	const string& device() const { return dev; }

	unsigned long long sizeK() const { return size_k; }

	unsigned long majorNr() const { return mjr; }
	unsigned long minorNr() const { return mnr; }

	// udev path and ids (without leading "/dev/disk/by-*/")
	virtual string udevPath() const;
	virtual list<string> udevId() const;

	void clearUsedBy() { uby.clear(); }
	void setUsedBy(UsedByType type, const string& device);
	void addUsedBy(UsedByType type, const string& device);
	void removeUsedBy(UsedByType type, const string& device);
	bool isUsedBy() const { return !uby.empty(); }
	bool isUsedBy(UsedByType type) const;
	const list<UsedBy>& getUsedBy() const { return uby; }

    protected:

	string nm;
	string dev;

	unsigned long long size_k;

	unsigned long mjr;
	unsigned long mnr;

	list<UsedBy> uby;

    private:

	Device& operator=(const Device&); // disallow

    };

}


#endif
