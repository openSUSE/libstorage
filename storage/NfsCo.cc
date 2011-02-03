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


#include <ostream>
#include <sstream>

#include "storage/NfsCo.h"
#include "storage/Nfs.h"
#include "storage/AppUtil.h"
#include "storage/SystemInfo.h"
#include "storage/Storage.h"
#include "storage/EtcFstab.h"


namespace storage
{
    using namespace std;


    NfsCo::NfsCo(Storage* s)
	: Container(s, "nfs", "/dev/nfs", staticType())
    {
	y2deb("constructing NfsCo");
    }


    NfsCo::NfsCo(Storage* s, const EtcFstab& fstab, SystemInfo& systeminfo)
	: Container(s, "nfs", "/dev/nfs", staticType(), systeminfo)
    {
	y2deb("constructing NfsCo");
	getNfsData(fstab, systeminfo);
    }


    NfsCo::NfsCo(const NfsCo& c)
	: Container(c)
    {
	y2deb("copy-constructed NfsCo from " << c.dev);

	ConstNfsPair p = c.nfsPair();
	for (ConstNfsIter i = p.begin(); i != p.end(); ++i)
	{
	    Nfs* p = new Nfs(*this, *i);
	    vols.push_back(p);
	}
    }


    NfsCo::~NfsCo()
    {
	y2deb("destructed NfsCo " << dev);
    }


int 
NfsCo::removeVolume( Volume* v )
    {
    y2mil( "v:" << *v );
    int ret = 0;
    NfsIter nfs;
    if( !findNfs( v->device(), nfs ))
	{
	ret = NFS_VOLUME_NOT_FOUND;
	}
    if( readonly() )
	{
	ret = NFS_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( nfs->created() )
	    {
	    if( !removeFromList( &(*nfs) ))
		ret = NFS_REMOVE_VOLUME_CREATE_NOT_FOUND;
	    }
	else
	    nfs->setDeleted();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
NfsCo::doRemove( Volume* v )
    {
    Nfs * p = dynamic_cast<Nfs *>(v);
    int ret = 0;
    if( p != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->removeText(true) );
	    }
	y2mil("doRemove container: " << name() << " name:" << p->name());
	ret = v->prepareRemove();
	if( ret==0 )
	    {
	    if( !removeFromList( p ) )
		ret = NFS_REMOVE_VOLUME_LIST_ERASE;
	    }
	}
    else
	{
	ret = NFS_REMOVE_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int 
NfsCo::addNfs(const string& nfsDev, unsigned long long sizeK, 
              const string& opts, const string& mp, bool nfs4)
    {
    y2mil("nfsDev:" << nfsDev << " sizeK:" << sizeK << "opts:" << opts <<
          " mp:" << mp << " nfs4:" << nfs4);
    Nfs *n = new Nfs(*this, nfsDev, nfs4);
    n->changeMount( mp );
    n->setSize( sizeK );
    n->setFstabOption( opts );
    addToList( n );
    return( 0 );
    }


    list<string>
    NfsCo::filterOpts(const list<string>& opts)
    {
	const char* ign_opt[] = { "hard", "rw", "v3", "v2", "lock" };
	const char* ign_opt_start[] = { "proto=", "addr=", "vers=" };

	list<string> ret = opts;

	for (size_t i = 0; i < lengthof(ign_opt); ++i)
	    ret.remove(ign_opt[i]);

	for (size_t i = 0; i < lengthof(ign_opt_start); ++i)
	    ret.remove_if(string_starts_with(ign_opt_start[i]));

	return ret;
    }


void
NfsCo::getNfsData(const EtcFstab& fstab, SystemInfo& systeminfo)
    {
    const list<FstabEntry> l1 = fstab.getEntries();
    for (list<FstabEntry>::const_iterator i = l1.begin(); i != l1.end(); ++i)
	{
	if( i->fs == "nfs" || i->fs == "nfs4")
	    {
	    Nfs *n = new Nfs(*this, i->device, i->fs == "nfs4");
	    n->setMount( i->mount );
	    string opt = boost::join(i->opts, ",");
	    if (opt != "defaults")
		n->setFstabOption(opt);
	    addToList( n );
	    }
	}

    const list<FstabEntry> l2 = systeminfo.getProcMounts().getEntries();
    for (list<FstabEntry>::const_iterator i = l2.begin(); i != l2.end(); ++i)
	{
	if( i->fs == "nfs" || i->fs == "nfs4")
	    {
	    Nfs *n = NULL;
	    NfsIter nfs;
	    if( findNfs( i->device, nfs ))
		{
		n = &(*nfs);
		}
	    else
		{
		n = new Nfs(*this, i->device, i->fs == "nfs4");
		n->setIgnoreFstab();
		string opt = boost::join(filterOpts(i->opts), ",");
		n->setFstabOption(opt);
		addToList( n );
		}

	    StatVfs vfsbuf;
	    getStatVfs(i->mount, vfsbuf);
	    n->setSize(vfsbuf.sizeK);
	    }
	}
    }


bool
NfsCo::findNfs( const string& dev, NfsIter& i )
    {
    NfsPair p=nfsPair();
    i=p.begin();
    while( i!=p.end() && i->device()!=dev )
	++i;
    return( i!=p.end() );
    }

bool
NfsCo::findNfs( const string& dev )
    {
    NfsIter i;
    return( findNfs( dev, i ));
    }


    std::ostream& operator<<(std::ostream& s, const NfsCo& d)
    {
    s << dynamic_cast<const Container&>(d);
    return( s );
    }


    void
    NfsCo::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
	const NfsCo& rhs = dynamic_cast<const NfsCo&>(rhs_c);

	logDifference(log, rhs);
	log << endl;

	ConstNfsPair pp = nfsPair();
	ConstNfsPair pc = rhs.nfsPair();

	logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


bool NfsCo::equalContent( const Container& rhs ) const
    {
    const NfsCo* p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const NfsCo*>(&rhs);
    if( ret && p )
	{
	ConstNfsPair pp = nfsPair();
	ConstNfsPair pc = p->nfsPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
	}
    return( ret );
    }

}
