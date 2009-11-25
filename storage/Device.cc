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


#include "storage/Device.h"
#include "storage/AsciiFile.h"
#include "storage/AppUtil.h"
#include "storage/StorageTmpl.h"


namespace storage
{
    using namespace std;


    Device::Device()
	: size_k(0), mjr(0), mnr(0)
    {
    }


    Device::Device(const string& nm, const string& dev)
	: nm(nm), dev(dev), size_k(0), mjr(0), mnr(0)
    {
    }


    Device::Device(const AsciiFile& file)
	: size_k(0), mjr(0), mnr(0)
    {
	const vector<string>& lines = file.lines();
	vector<string>::const_iterator it;

	if ((it = find_if(lines, string_starts_with("Name:"))) != lines.end())
	    nm = extractNthWord(1, *it);
	if ((it = find_if(lines, string_starts_with("Device:"))) != lines.end())
	    dev = extractNthWord(1, *it);

	if ((it = find_if(lines, string_starts_with("SizeK:"))) != lines.end())
	    extractNthWord(1, *it) >> size_k;

	if ((it = find_if(lines, string_starts_with("Major:"))) != lines.end())
	    extractNthWord(1, *it) >> mjr;
	if ((it = find_if(lines, string_starts_with("Minor:"))) != lines.end())
	    extractNthWord(1, *it) >> mnr;

	y2deb("constructed Device " << dev << " from file " << file.name());
    }


    Device::Device(const Device& d)
	: nm(d.nm), dev(d.dev), size_k(d.size_k), mjr(d.mjr), mnr(d.mnr), uby(d.uby)
    {
	y2deb("copy-constructed Device from " << d.dev);
    }


    Device::~Device()
    {
	y2deb("destructed Device " << dev);
    }


    string
    Device::udevPath() const
    {
	return string();
    }

    list<string>
    Device::udevId() const
    {
	return list<string>();
    }


    void
    Device::setUsedBy(UsedByType type, const string& device)
    {
	uby.clear();
	uby.push_back(UsedBy(type, device));
    }

    void
    Device::addUsedBy(UsedByType type, const string& device)
    {
	uby.push_back(UsedBy(type, device));
    }

    void
    Device::removeUsedBy(UsedByType type, const string& device)
    {
	uby.remove(UsedBy(type, device));
    }

    bool
    Device::isUsedBy(UsedByType type) const
    {
	for (list<UsedBy>::const_iterator it = uby.begin(); it != uby.end(); ++it)
	    if (it->type() == type)
		return true;

	return false;
    }

}
