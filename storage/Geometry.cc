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


#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/hdreg.h>

#include "storage/AppUtil.h"
#include "storage/SystemCmd.h"
#include "storage/StorageDefines.h"
#include "storage/Parted.h"
#include "storage/Enum.h"
#include "storage/Partition.h"


namespace storage
{
    using namespace std;


    Geometry::Geometry()
	: cylinders(0), heads(0), sectors(0), logical_sector_size(512)
    {
    }


    Geometry::Geometry(unsigned long cylinders, unsigned int heads, unsigned int sectors,
		       unsigned int logical_sector_size)
	: cylinders(cylinders), heads(heads), sectors(sectors),
	  logical_sector_size(logical_sector_size)
    {
    }


    std::ostream& operator<<(std::ostream& s, const Geometry& geo)
    {
	return s << "cylinders:" << geo.cylinders << " heads:" << geo.heads << " sectors:"
		 << geo.sectors << " logical_sector_size:" << geo.logical_sector_size;
    }


    bool
    getChildValue(const xmlNode* node, const char* name, Geometry& value)
    {
	const xmlNode* tmp = getChildNode(node, name);
	if (!tmp)
	    return false;

	getChildValue(tmp, "cylinders", value.cylinders);
	getChildValue(tmp, "heads", value.heads);
	getChildValue(tmp, "sectors", value.sectors);

	if (!getChildValue(tmp, "logical_sector_size", value.logical_sector_size))
	    value.logical_sector_size = 512;

	return true;
    }


    void
    setChildValue(xmlNode* node, const char* name, const Geometry& value)
    {
	xmlNode* tmp = xmlNewChild(node, name);

	setChildValue(tmp, "cylinders", value.cylinders);
	setChildValue(tmp, "heads", value.heads);
	setChildValue(tmp, "sectors", value.sectors);

	if (value.logical_sector_size != 512)
	    setChildValue(tmp, "logical_sector_size", value.logical_sector_size);
    }


    bool
    detectGeometry(const string& device, Geometry& geo)
    {
	y2mil("device:" << device);

	bool ret = false;

	int fd = open(device.c_str(), O_RDONLY);
	if (fd >= 0)
	{
	    int rcode = ioctl(fd, BLKSSZGET, &geo.logical_sector_size);
	    y2mil("BLKSSZGET rcode:" << rcode << " logical_sector_size:" << geo.logical_sector_size);

	    geo.cylinders = 16;
	    geo.heads = 255;
	    geo.sectors = 63;

	    struct hd_geometry hd_geo;
	    rcode = ioctl(fd, HDIO_GETGEO, &hd_geo);
	    y2mil("HDIO_GETGEO rcode:" << rcode << " cylinders:" << (unsigned) hd_geo.cylinders <<
		  " heads:" << (unsigned) hd_geo.heads << " sectors:" << (unsigned) hd_geo.sectors);
	    if (rcode == 0)
	    {
		geo.cylinders = hd_geo.cylinders > 0 ? hd_geo.cylinders : geo.cylinders;
		geo.heads = hd_geo.heads > 0 ? hd_geo.heads : geo.heads;
		geo.sectors = hd_geo.sectors > 0 ? hd_geo.sectors : geo.sectors;
	    }

	    uint64_t bytes = 0;
	    rcode = ioctl(fd, BLKGETSIZE64, &bytes);
	    y2mil("BLKGETSIZE64 rcode:" << rcode << " bytes:" << bytes);
	    if (rcode == 0 && bytes != 0)
	    {
		geo.cylinders = bytes / (uint64_t)(geo.cylinderSize());
		y2mil("calc cylinders:" << geo.cylinders);
		ret = true;
	    }
	    else
	    {
		unsigned long blocks;
		rcode = ioctl(fd, BLKGETSIZE, &blocks);
		y2mil("BLKGETSIZE rcode:" << rcode << " blocks:" << blocks);
		if (rcode == 0 && blocks != 0)
		{
		    geo.cylinders = (uint64_t)(blocks) * 512 / (uint64_t)(geo.cylinderSize());
		    y2mil("calc cylinders:" << geo.cylinders);
		    ret = true;
		}
	    }

	    close(fd);
	}

	y2mil("device:" << device << " ret:" << ret << " " << geo);
	return ret;
    }

}
