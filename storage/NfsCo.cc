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
#include "storage/ProcMounts.h"
#include "storage/Storage.h"
#include "storage/EtcFstab.h"


namespace storage
{
    using namespace std;


    NfsCo::NfsCo(Storage * const s, const EtcFstab& fstab, const ProcMounts& mounts)
	: Container(s, "nfs", staticType())
    {
    y2deb("constructing NfsCo detect");
    init();
    getNfsData(fstab, mounts);
    }

NfsCo::NfsCo( Storage * const s ) :
    Container(s,"nfs",staticType())
    {
    y2deb("constructing NfsCo");
    init();
    }

NfsCo::NfsCo( Storage * const s, const string& file ) :
    Container(s,"nfs",staticType())
    {
    y2deb("constructing NfsCo file:" << file);
    init();
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


void
NfsCo::init()
    {
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
NfsCo::addNfs( const string& nfsDev, unsigned long long sizeK,
               const string& mp )
    {
    y2mil( "nfsDev:" << nfsDev << " sizeK:" << sizeK << " mp:" << mp );
    Nfs *n = new Nfs( *this, nfsDev );
    n->changeMount( mp );
    n->setSize( sizeK );
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
NfsCo::getNfsData(const EtcFstab& fstab, const ProcMounts& mounts)
    {
    const list<FstabEntry> l1 = fstab.getEntries();
    for (list<FstabEntry>::const_iterator i = l1.begin(); i != l1.end(); ++i)
	{
	if( i->fs == "nfs" )
	    {
	    Nfs *n = new Nfs( *this, i->device );
	    n->setMount( i->mount );
	    string opt = boost::join(i->opts, ",");
	    if (opt != "defaults")
		n->setFstabOption(opt);
	    addToList( n );
	    }
	}

    const list<FstabEntry> l2 = mounts.getEntries();
    for (list<FstabEntry>::const_iterator i = l2.begin(); i != l2.end(); ++i)
	{
	if( i->fs == "nfs" )
	    {
	    Nfs *n = NULL;
	    NfsIter nfs;
	    if( findNfs( i->device, nfs ))
		{
		n = &(*nfs);
		}
	    else
		{
		n = new Nfs( *this, i->device );
		n->setIgnoreFstab();
		string opt = boost::join(filterOpts(i->opts), ",");
		n->setFstabOption(opt);
		addToList( n );
		}
	    n->setSize(Storage::getDfSize(i->mount));
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


void
NfsCo::logData(const string& Dir) const
{
}


    std::ostream& operator<<(std::ostream& s, const NfsCo& d)
    {
    s << *((Container*)&d);
    return( s );
    }


void NfsCo::logDifference( const Container& d ) const
    {
    y2mil(Container::getDiffString(d));
    const NfsCo* p = dynamic_cast<const NfsCo*>(&d);
    if( p != NULL )
	{
	ConstNfsPair pp=nfsPair();
	ConstNfsIter i=pp.begin();
	while( i!=pp.end() )
	    {
	    ConstNfsPair pc=p->nfsPair();
	    ConstNfsIter j = pc.begin();
	    while( j!=pc.end() &&
		   (i->device()!=j->device() || i->created()!=j->created()) )
		++j;
	    if( j!=pc.end() )
		{
		if( !i->equalContent( *j ) )
		    i->logDifference( *j );
		}
	    else
		y2mil( "  -->" << *i );
	    ++i;
	    }
	pp=p->nfsPair();
	i=pp.begin();
	while( i!=pp.end() )
	    {
	    ConstNfsPair pc=nfsPair();
	    ConstNfsIter j = pc.begin();
	    while( j!=pc.end() &&
		   (i->device()!=j->device() || i->created()!=j->created()) )
		++j;
	    if( j==pc.end() )
		y2mil( "  <--" << *i );
	    ++i;
	    }
	}
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
	ConstNfsIter i = pp.begin();
	ConstNfsIter j = pc.begin();
	while( ret && i!=pp.end() && j!=pc.end() )
	    {
	    ret = ret && i->equalContent( *j );
	    ++i;
	    ++j;
	    }
	ret = ret && i==pp.end() && j==pc.end();
	}
    return( ret );
    }

}
