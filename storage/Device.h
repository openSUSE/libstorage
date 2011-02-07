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


#ifndef DEVICE_H
#define DEVICE_H


#include <string>
#include <list>

#include "storage/StorageTypes.h"
#include "storage/XmlFile.h"


namespace storage
{
    using std::string;
    using std::list;


    class SystemInfo;


    class Device
    {
    public:

	Device(const string& nm, const string& dev);
	Device(const string& nm, const string& dev, SystemInfo& systeminfo);
	Device(const xmlNode* node);
	Device(const Device&);
	virtual ~Device();

	void saveData(xmlNode* node) const;

	const string& name() const { return nm; }
	const string& device() const { return dev; }
	void setNameDevice(const string& nm, const string& dev);

	bool created() const { return create; }
	bool deleted() const { return del; }
	void setCreated(bool val = true) { create = val; }
	void setDeleted(bool val = true) { del = val; }

	bool isSilent() const { return silent; }
	void setSilent(bool val = true) { silent = val; }

	unsigned long long sizeK() const { return size_k; }
	string sizeString() const;

	bool getMajorMinor();
	unsigned long majorNr() const { return mjr; }
	unsigned long minorNr() const { return mnr; }

	const std::list<string>& altNames() const { return( alt_names ); }
	void setAltNames( std::list<string>& li ) { alt_names=li; }
	bool sameDevice( const string& device ) const;

	// udev path and ids (without leading "/dev/disk/by-*/")
	virtual string udevPath() const;
	virtual list<string> udevId() const;

	// name for device in /proc/partitions
	virtual string procName() const { return ""; }

	// full path for device in /sys/block
	virtual string sysfsPath() const { return ""; }

	void clearUsedBy() { uby.clear(); }
	void setUsedBy(UsedByType type, const string& device);
	void addUsedBy(UsedByType type, const string& device);
	void removeUsedBy(UsedByType type, const string& device);
	bool isUsedBy() const { return !uby.empty(); }
	bool isUsedBy(UsedByType type) const;
	const list<UsedBy>& getUsedBy() const { return uby; }

	void logDifference(std::ostream& log, const Device& rhs) const;

	friend std::ostream& operator<<(std::ostream& s, const Device& d);

    protected:

	string nm;
	string dev;

	bool create;
	bool del;

	bool silent;

	unsigned long long size_k;

	unsigned long mjr;
	unsigned long mnr;

	list<UsedBy> uby;
	list<string> alt_names;

    private:

	Device& operator=(const Device&); // disallow

    };

}


#endif
