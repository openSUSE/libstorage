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


    Device& Device::operator=(const Device& rhs)
    {
	y2deb("operator= from " << rhs.nm);

	nm = rhs.nm;
	dev = rhs.dev;
	size_k = rhs.size_k;
	mjr = rhs.mjr;
	mnr = rhs.mnr;
	uby = rhs.uby;

	return *this;
    }

}
