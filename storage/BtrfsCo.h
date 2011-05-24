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


#ifndef BTRFS_CO_H
#define BTRFS_CO_H

#include "storage/Container.h"
#include "storage/Btrfs.h"


namespace storage
{
    class SystemInfo;


class BtrfsCo : public Container
    {
    friend class Storage;

    public:

	BtrfsCo(Storage * const s);
	BtrfsCo(Storage * const s, SystemInfo& systeminfo);
	BtrfsCo(const BtrfsCo& c);
	virtual ~BtrfsCo();

	void addFromVolume( const Volume& v, string& uuid );
	void eraseVolume( Volume* v );

	int createSubvolume( const string& device, const string& name );
	int removeSubvolume( const string& device, const string& name );
	int extendVolume( const string& device, const string& dev );
	int extendVolume( const string& device, const list<string>& devs );
	int shrinkVolume( const string& device, const string& dev );
	int shrinkVolume( const string& device, const list<string>& devs );
	bool deviceToUuid( const string& device, string& uuid );

	int doRemove( Volume* v );

	static storage::CType staticType() { return storage::BTRFSC; }
	friend std::ostream& operator<< (std::ostream&, const BtrfsCo& );

	int commitChanges( CommitStage stage, Volume* vol );
	void getToCommit( storage::CommitStage stage, list<const Container*>& col,
			  list<const Volume*>& vo ) const;

	int removeVolume( Volume* v );
	int removeVolume( Volume* v, bool quiet );
	int removeUuid( const string& uuid );
	bool equalContent( const Container& rhs ) const;
	void saveData(xmlNode* node) const;

	virtual void logDifferenceWithVolumes(std::ostream& log, const Container& rhs) const;
	virtual void logData(const string& Dir) const;

	
    protected:
	// iterators over BTRFS volumes
	// protected typedefs for iterators over BTRFS volumes
	typedef CastIterator<VIter, Btrfs *> BtrfsInter;
	typedef CastIterator<CVIter, const Btrfs *> BtrfsCInter;
	template< class Pred >
	    struct BtrfsPI { typedef ContainerIter<Pred, BtrfsInter> type; };
	template< class Pred >
	    struct BtrfsCPI { typedef ContainerIter<Pred, BtrfsCInter> type; };
	typedef CheckFnc<const Btrfs> CheckFncBtrfs;
	typedef CheckerIterator< CheckFncBtrfs, BtrfsPI<CheckFncBtrfs>::type,
				 BtrfsInter, Btrfs > BtrfsPIterator;
	typedef CheckerIterator< CheckFncBtrfs, BtrfsCPI<CheckFncBtrfs>::type,
				 BtrfsCInter, const Btrfs > BtrfsCPIterator;
	typedef DerefIterator<BtrfsPIterator,Btrfs> BtrfsIter;
	typedef DerefIterator<BtrfsCPIterator,const Btrfs> ConstBtrfsIter;
	typedef IterPair<BtrfsIter> BtrfsPair;
	typedef IterPair<ConstBtrfsIter> ConstBtrfsPair;

	BtrfsPair btrfsPair( bool (* Check)( const Btrfs& )=NULL)
	    {
	    return( BtrfsPair( btrfsBegin( Check ), btrfsEnd( Check ) ));
	    }
	BtrfsIter btrfsBegin( bool (* Check)( const Btrfs& )=NULL)
	    {
	    IterPair<BtrfsInter> p( (BtrfsInter(begin())), (BtrfsInter(end())) );
	    return( BtrfsIter( BtrfsPIterator( p, Check )) );
	    }
	BtrfsIter btrfsEnd( bool (* Check)( const Btrfs& )=NULL)
	    {
	    IterPair<BtrfsInter> p( (BtrfsInter(begin())), (BtrfsInter(end())) );
	    return( BtrfsIter( BtrfsPIterator( p, Check, true )) );
	    }

	ConstBtrfsPair btrfsPair( bool (* Check)( const Btrfs& )=NULL) const
	    {
	    return( ConstBtrfsPair( btrfsBegin( Check ), btrfsEnd( Check ) ));
	    }
	ConstBtrfsIter btrfsBegin( bool (* Check)( const Btrfs& )=NULL) const
	    {
	    IterPair<BtrfsCInter> p( (BtrfsCInter(begin())), (BtrfsCInter(end())) );
	    return( ConstBtrfsIter( BtrfsCPIterator( p, Check )) );
	    }
	ConstBtrfsIter btrfsEnd( bool (* Check)( const Btrfs& )=NULL) const
	    {
	    IterPair<BtrfsCInter> p( (BtrfsCInter(begin())), (BtrfsCInter(end())) );
	    return( ConstBtrfsIter( BtrfsCPIterator( p, Check, true )) );
	    }

	void getBtrfsData(SystemInfo& systeminfo);
	bool findBtrfs( const string& uuid, BtrfsIter& i );
	void addBtrfs( Btrfs* m );

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new BtrfsCo( *this ) ); }

    private:

	BtrfsCo& operator=(const BtrfsCo&); // disallow
	
    };

}

#endif
