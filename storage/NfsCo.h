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


#ifndef NFS_CO_H
#define NFS_CO_H

#include "storage/Container.h"
#include "storage/Nfs.h"

namespace storage
{
    class SystemInfo;


class NfsCo : public Container
    {
    friend class Storage;

    public:

	NfsCo(Storage * const s);
	NfsCo(Storage * const s, const EtcFstab& fstab, SystemInfo& systeminfo);
	NfsCo(const NfsCo& c);
	virtual ~NfsCo();

	static storage::CType staticType() { return storage::NFSC; }
	friend std::ostream& operator<< ( std::ostream&, const NfsCo& );
	int addNfs(const string& nfsDev, unsigned long long sizeK, const string& opts, 
	           const string& mp, bool nfs4);

	int removeVolume( Volume* v );
	int doRemove( Volume* );

	bool equalContent( const Container& rhs ) const;

	virtual void logDifferenceWithVolumes(std::ostream& log, const Container& rhs) const;
	
    protected:
	// iterators over NFS volumes
	// protected typedefs for iterators over NFS volumes
	typedef CastIterator<VIter, Nfs *> NfsInter;
	typedef CastIterator<CVIter, const Nfs *> NfsCInter;
	template< class Pred >
	    struct NfsPI { typedef ContainerIter<Pred, NfsInter> type; };
	template< class Pred >
	    struct NfsCPI { typedef ContainerIter<Pred, NfsCInter> type; };
	typedef CheckFnc<const Nfs> CheckFncNfs;
	typedef CheckerIterator< CheckFncNfs, NfsPI<CheckFncNfs>::type,
				 NfsInter, Nfs > NfsPIterator;
	typedef CheckerIterator< CheckFncNfs, NfsCPI<CheckFncNfs>::type,
				 NfsCInter, const Nfs > NfsCPIterator;
	typedef DerefIterator<NfsPIterator,Nfs> NfsIter;
	typedef DerefIterator<NfsCPIterator,const Nfs> ConstNfsIter;
	typedef IterPair<NfsIter> NfsPair;
	typedef IterPair<ConstNfsIter> ConstNfsPair;

	NfsPair nfsPair( bool (* Check)( const Nfs& )=NULL)
	    {
	    return( NfsPair( nfsBegin( Check ), nfsEnd( Check ) ));
	    }
	NfsIter nfsBegin( bool (* Check)( const Nfs& )=NULL)
	    {
	    IterPair<NfsInter> p( (NfsInter(begin())), (NfsInter(end())) );
	    return( NfsIter( NfsPIterator( p, Check )) );
	    }
	NfsIter nfsEnd( bool (* Check)( const Nfs& )=NULL)
	    {
	    IterPair<NfsInter> p( (NfsInter(begin())), (NfsInter(end())) );
	    return( NfsIter( NfsPIterator( p, Check, true )) );
	    }

	ConstNfsPair nfsPair( bool (* Check)( const Nfs& )=NULL) const
	    {
	    return( ConstNfsPair( nfsBegin( Check ), nfsEnd( Check ) ));
	    }
	ConstNfsIter nfsBegin( bool (* Check)( const Nfs& )=NULL) const
	    {
	    IterPair<NfsCInter> p( (NfsCInter(begin())), (NfsCInter(end())) );
	    return( ConstNfsIter( NfsCPIterator( p, Check )) );
	    }
	ConstNfsIter nfsEnd( bool (* Check)( const Nfs& )=NULL) const
	    {
	    IterPair<NfsCInter> p( (NfsCInter(begin())), (NfsCInter(end())) );
	    return( ConstNfsIter( NfsCPIterator( p, Check, true )) );
	    }

	bool findNfs( const string& dev, NfsIter& i );
	bool findNfs( const string& dev );

	static list<string> filterOpts(const list<string>& opts);
	void getNfsData(const EtcFstab& fstab, SystemInfo& systeminfo);

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new NfsCo( *this ) ); }

    private:

	NfsCo& operator=(const NfsCo&); // disallow

    };

}

#endif
