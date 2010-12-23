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


#include <ostream>
#include <sstream>

#include "storage/BtrfsCo.h"
#include "storage/Btrfs.h"
#include "storage/SystemCmd.h"
#include "storage/Dm.h"
#include "storage/SystemInfo.h"
#include "storage/ProcMounts.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/EtcFstab.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    BtrfsCo::BtrfsCo(Storage* s)
	: Container(s, "btrfs", "/dev/", staticType())
    {
	y2deb("constructing BtrfsCo");
    }


    BtrfsCo::BtrfsCo(Storage* s, SystemInfo& systeminfo)
	: Container(s, "btrfs", "/dev/", staticType(), systeminfo)
    {
	y2deb("constructing BtrfsCo");
	getBtrfsData(systeminfo);
    }


    BtrfsCo::BtrfsCo(const BtrfsCo& c)
	: Container(c)
    {
	y2deb("copy-constructed BtrfsCo from " << c.dev);

	ConstBtrfsPair p = c.btrfsPair();
	for (ConstBtrfsIter i = p.begin(); i != p.end(); ++i)
	{
	    Btrfs* p = new Btrfs(*this, *i);
	    vols.push_back(p);
	}
    }


    BtrfsCo::~BtrfsCo()
    {
	y2deb("destructed BtrfsCo " << dev);
    }


void BtrfsCo::getBtrfsData(SystemInfo& systeminfo)
    {
    y2mil("begin");
    list<string> uuids = systeminfo.getCmdBtrfsShow().getUuids();
    CmdBtrfsShow::Entry e;
    for( list<string>::const_iterator i=uuids.begin(); i!=uuids.end(); ++i )
	{
	if( systeminfo.getCmdBtrfsShow().getEntry( *i, e ))
	    {
	    Volume const* cv = NULL;
	    unsigned long long sum_size = 0;
	    for( list<string>::const_iterator d=e.devices.begin(); d!=e.devices.end(); ++d )
		{
		Volume const* v;
		if( getStorage()->findVolume( *d, v ))
		    {
		    if( cv==NULL || 
			(cv->getMount().empty() && !v->getMount().empty()) ||
			(!cv->getFormat() && v->getFormat() ))
			cv = v;
		    sum_size += v->sizeK();
		    }
		else
		    y2war( "device " << *d << " not found" );
		}
	    Btrfs* b = new Btrfs( *this, *cv, sum_size, e.devices );
	    vols.push_back(b);
	    }
	else
	    y2war( "uuid " << *i << " not found" );
	}
    BtrfsPair p( btrfsPair() );
    for( BtrfsIter i=p.begin(); i!=p.end(); ++i )
	{
	y2mil( "dev:" << i->device() );
	bool mounted = false;
	string mp = i->getMount();
	if( !i->isMounted() && getStorage()->mountTmpRo( &(*i), mp ) )
	    mounted = true;
	if( !mp.empty() )
	    {
	    i->clearSubvol();
	    SystemCmd cmd( "btrfs subvolume list " + mp );
	    for( vector<string>::const_iterator s=cmd.stdout().begin(); 
		 s!=cmd.stdout().end(); ++s )
		{
		string subvol;
		string::size_type pos = s->find( " path " );
		if( pos!=string::npos )
		    pos = s->find_first_not_of( app_ws, pos+5 );
		if( pos!=string::npos )
		    subvol = s->substr( pos, s->find_last_not_of( app_ws ) );
		if( !subvol.empty() )
		    i->addSubvol( subvol );
		}
	    }
	if( mounted )
	    {
	    getStorage()->umountDevice( i->device() );
	    rmdir( mp.c_str() );
	    }
	}
    y2mil("end");
    }

bool
BtrfsCo::findBtrfs( const string& id, BtrfsIter& i )
    {
    BtrfsPair p=btrfsPair(Btrfs::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->getUuid()!=id )
	++i;
    return( i!=p.end() );
    }

int BtrfsCo::removeVolume( Volume* v )
    {
    int ret = 0;
    y2mil("name:" << v->name());
    //ret = removeBtrfs( v->getUuid() );
    y2mil("ret:" << ret);
    return( ret );
    }

void
BtrfsCo::logData(const string& Dir) const
    {
    string fname(Dir + "/btrfs.info.tmp");

    XmlFile xml;
    xmlNode* node = xmlNewNode("btrfs");
    xml.setRootElement(node);
    saveData(node);
    xml.save(fname);

    getStorage()->handleLogFile( fname );
    }

void
BtrfsCo::saveData(xmlNode* node) const
    {
    Container::saveData(node);
    ConstBtrfsPair vp = btrfsPair();
    for (ConstBtrfsIter v = vp.begin(); v != vp.end(); ++v)
	v->saveData(xmlNewChild(node, "btrfs"));
    }


std::ostream& operator<<(std::ostream& s, const BtrfsCo& d)
    {
    s << dynamic_cast<const Container&>(d);
    return( s );
    }


void BtrfsCo::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
    const BtrfsCo& rhs = dynamic_cast<const BtrfsCo&>(rhs_c);

    logDifference(log, rhs);
    log << endl;

    ConstBtrfsPair pp = btrfsPair();
    ConstBtrfsPair pc = rhs.btrfsPair();
    logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


bool BtrfsCo::equalContent( const Container& rhs ) const
    {
    const BtrfsCo * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const BtrfsCo*>(&rhs);
    if( ret && p )
	{
	ConstBtrfsPair pp = btrfsPair();
	ConstBtrfsPair pc = p->btrfsPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
	}
    return( ret );
    }

}
