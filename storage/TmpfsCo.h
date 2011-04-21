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


#ifndef TMPFS_CO_H
#define TMPFS_CO_H

#include "storage/Container.h"
#include "storage/Tmpfs.h"


namespace storage
{
    class SystemInfo;


class TmpfsCo : public Container
    {
    friend class Storage;

    public:

	TmpfsCo(Storage * const s);
	TmpfsCo(Storage * const s, const EtcFstab& fstab, SystemInfo& systeminfo);
	TmpfsCo(const TmpfsCo& c);
	virtual ~TmpfsCo();

	int addTmpfs( const string& mp, const string &opts );
	int removeTmpfs( const string& mp, bool silent=false );

	int doRemove( Volume* v );

	static storage::CType staticType() { return storage::TMPFSC; }
	friend std::ostream& operator<< (std::ostream&, const TmpfsCo& );

	int removeVolume( Volume* v );
	int removeVolume( Volume* v, bool quiet );
	bool equalContent( const Container& rhs ) const;
	void saveData(xmlNode* node) const;

	virtual void logDifferenceWithVolumes(std::ostream& log, const Container& rhs) const;
	virtual void logData(const string& Dir) const;

	
    protected:
	// iterators over TMPFS volumes
	// protected typedefs for iterators over TMPFS volumes
	typedef CastIterator<VIter, Tmpfs *> TmpfsInter;
	typedef CastIterator<CVIter, const Tmpfs *> TmpfsCInter;
	template< class Pred >
	    struct TmpfsPI { typedef ContainerIter<Pred, TmpfsInter> type; };
	template< class Pred >
	    struct TmpfsCPI { typedef ContainerIter<Pred, TmpfsCInter> type; };
	typedef CheckFnc<const Tmpfs> CheckFncTmpfs;
	typedef CheckerIterator< CheckFncTmpfs, TmpfsPI<CheckFncTmpfs>::type,
				 TmpfsInter, Tmpfs > TmpfsPIterator;
	typedef CheckerIterator< CheckFncTmpfs, TmpfsCPI<CheckFncTmpfs>::type,
				 TmpfsCInter, const Tmpfs > TmpfsCPIterator;
	typedef DerefIterator<TmpfsPIterator,Tmpfs> TmpfsIter;
	typedef DerefIterator<TmpfsCPIterator,const Tmpfs> ConstTmpfsIter;
	typedef IterPair<TmpfsIter> TmpfsPair;
	typedef IterPair<ConstTmpfsIter> ConstTmpfsPair;

	TmpfsPair tmpfsPair( bool (* Check)( const Tmpfs& )=NULL)
	    {
	    return( TmpfsPair( tmpfsBegin( Check ), tmpfsEnd( Check ) ));
	    }
	TmpfsIter tmpfsBegin( bool (* Check)( const Tmpfs& )=NULL)
	    {
	    IterPair<TmpfsInter> p( (TmpfsInter(begin())), (TmpfsInter(end())) );
	    return( TmpfsIter( TmpfsPIterator( p, Check )) );
	    }
	TmpfsIter tmpfsEnd( bool (* Check)( const Tmpfs& )=NULL)
	    {
	    IterPair<TmpfsInter> p( (TmpfsInter(begin())), (TmpfsInter(end())) );
	    return( TmpfsIter( TmpfsPIterator( p, Check, true )) );
	    }

	ConstTmpfsPair tmpfsPair( bool (* Check)( const Tmpfs& )=NULL) const
	    {
	    return( ConstTmpfsPair( tmpfsBegin( Check ), tmpfsEnd( Check ) ));
	    }
	ConstTmpfsIter tmpfsBegin( bool (* Check)( const Tmpfs& )=NULL) const
	    {
	    IterPair<TmpfsCInter> p( (TmpfsCInter(begin())), (TmpfsCInter(end())) );
	    return( ConstTmpfsIter( TmpfsCPIterator( p, Check )) );
	    }
	ConstTmpfsIter tmpfsEnd( bool (* Check)( const Tmpfs& )=NULL) const
	    {
	    IterPair<TmpfsCInter> p( (TmpfsCInter(begin())), (TmpfsCInter(end())) );
	    return( ConstTmpfsIter( TmpfsCPIterator( p, Check, true )) );
	    }

	void getTmpfsData(const EtcFstab& fstab, SystemInfo& systeminfo);
	bool findTmpfs( const string& mp, TmpfsIter& i );

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new TmpfsCo( *this ) ); }

    private:

	TmpfsCo& operator=(const TmpfsCo&); // disallow
	
    };

}

#endif
