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


#include "storage/Dasdview.h"
#include "storage/StorageDefines.h"
#include "storage/SystemCmd.h"


namespace storage
{

    Dasdview::Dasdview(const string& device)
	: dasd_format(Dasd::DASDF_NONE)
    {
	SystemCmd cmd(DASDVIEWBIN " -x " + quote(device));

	if (cmd.retcode() == 0)
	{
	    if (cmd.select("^format") > 0)
	    {
		string tmp = cmd.getLine(0, true);
		y2mil("Format line:" << tmp);
		tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
		tmp = boost::to_lower_copy(extractNthWord(4, tmp), locale::classic());
		if( tmp == "cdl" )
		    dasd_format = Dasd::DASDF_CDL;
		else if( tmp == "ldl" )
		    dasd_format = Dasd::DASDF_LDL;
	    }

	    if (cmd.select("cylinders") > 0)
	    {
		string tmp = cmd.getLine(0, true);
		y2mil("Cylinder line:" << tmp);
		tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
		tmp = extractNthWord( 3, tmp );
		tmp >> geometry.cylinders;
	    }

	    if (cmd.select("tracks per") > 0)
	    {
		string tmp = cmd.getLine(0, true);
		y2mil("Tracks line:" << tmp);
		tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
		tmp = extractNthWord( 3, tmp );
		tmp >> geometry.heads;
	    }

	    if (cmd.select("blocks per") > 0)
	    {
		string tmp = cmd.getLine(0, true);
		y2mil("Blocks line:" << tmp);
		tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
		tmp = extractNthWord( 3, tmp );
		tmp >> geometry.sectors;
	    }

	    if (cmd.select("blocksize") > 0)
	    {
		string tmp = cmd.getLine(0, true);
		y2mil("Bytes line:" << tmp);
		tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
		tmp = extractNthWord( 3, tmp );
		tmp >> geometry.sector_size;
	    }
	}
	else
	{
	    y2err("dasdview failed");

	    geometry.heads = 15;
	    geometry.sectors = 12;
	    geometry.sector_size = 4096;
	}

	y2mil("device: " << device << " geometry:" << geometry << " dasd_format:" <<
	      toString(dasd_format));
    }

}
