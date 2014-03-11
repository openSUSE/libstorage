/*
 * Copyright (c) [2004-2014] Novell, Inc.
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


#include <stdexcept>

#include "storage/SystemInfo/Dev.h"
#include "storage/AppUtil.h"


namespace storage
{
    using namespace std;


    MajorMinor::MajorMinor(const string& device, bool do_probe)
	: device(device)
    {
	if (do_probe)
	    probe();
    }


    void
    MajorMinor::probe()
    {
	struct stat buf;
	if (stat(device.c_str(), &buf) != 0)
	    throw runtime_error(device + " not found");

	if (!S_ISBLK(buf.st_mode))
	    throw runtime_error(device + " not block device");

	majorminor = buf.st_rdev;

	y2mil(*this);
    }


    std::ostream& operator<<(std::ostream& s, const MajorMinor& majorminor)
    {
	s << "device:" << majorminor.device << " majorminor:" << majorminor.getMajor()
	  << ":" << majorminor.getMinor();

	return s;
    }

}
