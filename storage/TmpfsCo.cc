/*
 * Copyright (c) [2004-2011] Novell, Inc.
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

#include "storage/TmpfsCo.h"
#include "storage/Tmpfs.h"
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


TmpfsCo::TmpfsCo(Storage* s) : Container(s, "tmpfs", "/dev/tmpfs", staticType())
    {
    y2deb("constructing TmpfsCo");
    }

TmpfsCo::TmpfsCo(Storage* s, const EtcFstab& fstab, SystemInfo& systeminfo)
	: Container(s, "tmpfs", "/dev/tmpfs", staticType(), systeminfo)
    {
    y2deb("constructing TmpfsCo");
    getTmpfsData(fstab, systeminfo);
    }

TmpfsCo::TmpfsCo(const TmpfsCo& c) : Container(c)
    {
    y2deb("copy-constructed TmpfsCo from " << c.dev);

    ConstTmpfsPair p = c.tmpfsPair();
    for (ConstTmpfsIter i = p.begin(); i != p.end(); ++i)
	{
	Tmpfs* p = new Tmpfs(*this, *i);
	vols.push_back(p);
	}
    }

TmpfsCo::~TmpfsCo()
    {
    y2deb("destructed TmpfsCo " << dev);
    }

int TmpfsCo::addTmpfs( const string& mp, const string &opts )
    {
    removeTmpfs( mp, true );
    Tmpfs* t = new Tmpfs( *this, mp, false );
    t->setFstabOption( opts );
    addToList( t );
    return( 0 );
    }

int TmpfsCo::removeTmpfs( const string& mp, bool silent )
    {
    int ret = 0;
    TmpfsIter tmpfs;
    if( findTmpfs( mp, tmpfs ))
	{
	if( silent )
	    tmpfs->setSilent();
	tmpfs->changeMount( "" );
	tmpfs->setDeleted();
	}
    else
	ret = TMPFS_REMOVE_NOT_FOUND;
    return( ret );
    }

void TmpfsCo::getTmpfsData(const EtcFstab& fstab, SystemInfo& systeminfo)
    {
    const list<FstabEntry> l = systeminfo.getProcMounts().getEntries();
    for (list<FstabEntry>::const_iterator i = l.begin(); i != l.end(); ++i)
	{
	if( i->fs == "tmpfs" )
	    {
	    y2mil( "entry:" << *i );
	    TmpfsIter tmpfs;
	    if( !findTmpfs( i->mount, tmpfs ))
		{
		Tmpfs *t = new Tmpfs(*this, i->mount, true);
		if( !fstab.findMount(i->mount) )
		    t->setIgnoreFstab();

		StatVfs vfsbuf;
		getStatVfs(i->mount, vfsbuf);
		t->setSize(vfsbuf.sizeK);
		addToList( t );
		y2mil( "tmpfs:" << *t );
		}
	    }
	}
    }

int TmpfsCo::removeVolume( Volume* v, bool quiet )
    {
    int ret = 0;
    y2mil("qiet:" << quiet);
    y2mil("vol:" << *v);
    v->setDeleted();
    if( quiet )
	v->setSilent();
    y2mil("ret:" << ret);
    return( ret );
    }

int TmpfsCo::removeVolume( Volume* v )
    {
    return( removeVolume( v, false ));
    }

int
TmpfsCo::doRemove( Volume* v )
    {
    int ret = 0;
    Tmpfs *b = dynamic_cast<Tmpfs *>(v);
    if( b != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( b->removeText(true) );
	    }
	ret = v->prepareRemove();
	if( ret==0 && !removeFromList(v) )
	    ret = TMPFS_REMOVE_NO_TMPFS;
	}
    else
	ret = TMPFS_REMOVE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }

bool TmpfsCo::findTmpfs( const string& mp, TmpfsIter& i )
    {
    TmpfsPair p=tmpfsPair(Tmpfs::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->getMount()!=mp )
	++i;
    y2mil( "mp:" << mp << " ret:" << (i!=p.end()) );
    return( i!=p.end() );
    }


void
TmpfsCo::logData(const string& Dir) const
    {
    string fname(Dir + "/tmpfs.info.tmp");

    XmlFile xml;
    xmlNode* node = xmlNewNode("tmpfs");
    xml.setRootElement(node);
    saveData(node);
    xml.save(fname);

    getStorage()->handleLogFile( fname );
    }

void
TmpfsCo::saveData(xmlNode* node) const
    {
    Container::saveData(node);
    ConstTmpfsPair vp = tmpfsPair();
    for (ConstTmpfsIter v = vp.begin(); v != vp.end(); ++v)
	v->saveData(xmlNewChild(node, "tmpfs"));
    }


std::ostream& operator<<(std::ostream& s, const TmpfsCo& d)
    {
    s << dynamic_cast<const Container&>(d);
    return( s );
    }


void TmpfsCo::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
    const TmpfsCo& rhs = dynamic_cast<const TmpfsCo&>(rhs_c);

    logDifference(log, rhs);
    log << endl;

    ConstTmpfsPair pp = tmpfsPair();
    ConstTmpfsPair pc = rhs.tmpfsPair();
    logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


bool TmpfsCo::equalContent( const Container& rhs ) const
    {
    const TmpfsCo * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const TmpfsCo*>(&rhs);
    if( ret && p )
	{
	ConstTmpfsPair pp = tmpfsPair();
	ConstTmpfsPair pc = p->tmpfsPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
	}
    return( ret );
    }

}
