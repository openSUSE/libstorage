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


#include "storage/AppUtil.h"
#include "storage/Regex.h"
#include "storage/EtcRaidtab.h"
#include "storage/AsciiFile.h"
#include "storage/Md.h"


namespace storage
{
    using namespace std;


EtcRaidtab::EtcRaidtab(const string& prefix)
    : mdadm(prefix + "/etc/mdadm.conf")
{
    mdadm_dev_line = -1;
    buildMdadmMap();
}


EtcRaidtab::~EtcRaidtab()
{
}


void 
EtcRaidtab::updateEntry(unsigned num, const string& mline)
    {
    y2mil("num:" << num);
    string dline;
    map<unsigned, entry>::const_iterator i = mtab.find( num );
    if( i != mtab.end() )
	{
	mdadm.replace( i->second.first, i->second.last-i->second.first+1, mline );
	}
    else
	{
	mdadm.append( mline );
	}
    if( mdadm_dev_line<0 )
	{
	dline = "DEVICE partitions";
	mdadm_dev_line = mdadm.numLines();
	mdadm.insert( 0, dline );
	}
    else
	{
	dline = "DEVICE partitions";
	mdadm[mdadm_dev_line] = dline;
	}
    updateMdadmFile();
    }

void
EtcRaidtab::removeEntry( unsigned num )
    {
    map<unsigned, entry>::const_iterator i = mtab.find( num );
    if( i != mtab.end() )
	{
	mdadm.remove( i->second.first, i->second.last-i->second.first+1 );
	updateMdadmFile();
	}
    }

void
EtcRaidtab::updateMdadmFile()
    {
    mtab.clear();
    mdadm.save();
    buildMdadmMap();
    }

void
EtcRaidtab::buildMdadmMap()
    {
    unsigned lineno = 0;
    unsigned mdnum;
    mdadm_dev_line = -1;
    entry e;
    while (lineno < mdadm.numLines())
	{
	string key = extractNthWord( 0, mdadm[lineno] );
	if( mdadm_dev_line<0 && key == "DEVICE"  )
	    mdadm_dev_line = lineno;
	else if( key == "ARRAY" &&
		 Md::mdStringNum( extractNthWord( 1, mdadm[lineno] ), mdnum ))
	    {
	    e.first = lineno++;
	    while( lineno < mdadm.numLines() && 
	           (key = extractNthWord( 0, mdadm[lineno] ))!="ARRAY" &&
		   key != "DEVICE" )
		{
		key = extractNthWord( 0, mdadm[lineno++] );
		}
	    e.last = lineno-1;
	    mtab[mdnum] = e;
	    }
	else
	    lineno++;
	}
    }

}
