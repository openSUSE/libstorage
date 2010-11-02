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


#ifndef LOOP_CO_H
#define LOOP_CO_H

#include "storage/Container.h"
#include "storage/Loop.h"


namespace storage
{
    class SystemInfo;


class LoopCo : public Container
    {
    friend class Storage;

    public:

	LoopCo(Storage * const s);
	LoopCo(Storage * const s, SystemInfo& systeminfo);
	LoopCo(const LoopCo& c);
	virtual ~LoopCo();

	static storage::CType staticType() { return storage::LOOP; }
	friend std::ostream& operator<< (std::ostream&, const LoopCo& );

	int createLoop( const string& file, bool reuseExisting, 
	                unsigned long long sizeK, bool dmcr, string& device );
	int updateLoop( const string& device, const string& file, 
	                bool reuseExisting, unsigned long long sizeK );
	int removeLoop( const string& file, bool removeFile = false );

	list<unsigned> usedNumbers() const;

	int removeVolume( Volume* v );
	bool equalContent( const Container& rhs ) const;

	virtual void logDifferenceWithVolumes(std::ostream& log, const Container& rhs) const;
	
    protected:
	// iterators over LOOP volumes
	// protected typedefs for iterators over LOOP volumes
	typedef CastIterator<VIter, Loop *> LoopInter;
	typedef CastIterator<CVIter, const Loop *> LoopCInter;
	template< class Pred >
	    struct LoopPI { typedef ContainerIter<Pred, LoopInter> type; };
	template< class Pred >
	    struct LoopCPI { typedef ContainerIter<Pred, LoopCInter> type; };
	typedef CheckFnc<const Loop> CheckFncLoop;
	typedef CheckerIterator< CheckFncLoop, LoopPI<CheckFncLoop>::type,
				 LoopInter, Loop > LoopPIterator;
	typedef CheckerIterator< CheckFncLoop, LoopCPI<CheckFncLoop>::type,
				 LoopCInter, const Loop > LoopCPIterator;
	typedef DerefIterator<LoopPIterator,Loop> LoopIter;
	typedef DerefIterator<LoopCPIterator,const Loop> ConstLoopIter;
	typedef IterPair<LoopIter> LoopPair;
	typedef IterPair<ConstLoopIter> ConstLoopPair;

	LoopPair loopPair( bool (* Check)( const Loop& )=NULL)
	    {
	    return( LoopPair( loopBegin( Check ), loopEnd( Check ) ));
	    }
	LoopIter loopBegin( bool (* Check)( const Loop& )=NULL)
	    {
	    IterPair<LoopInter> p( (LoopInter(begin())), (LoopInter(end())) );
	    return( LoopIter( LoopPIterator( p, Check )) );
	    }
	LoopIter loopEnd( bool (* Check)( const Loop& )=NULL)
	    {
	    IterPair<LoopInter> p( (LoopInter(begin())), (LoopInter(end())) );
	    return( LoopIter( LoopPIterator( p, Check, true )) );
	    }

	ConstLoopPair loopPair( bool (* Check)( const Loop& )=NULL) const
	    {
	    return( ConstLoopPair( loopBegin( Check ), loopEnd( Check ) ));
	    }
	ConstLoopIter loopBegin( bool (* Check)( const Loop& )=NULL) const
	    {
	    IterPair<LoopCInter> p( (LoopCInter(begin())), (LoopCInter(end())) );
	    return( ConstLoopIter( LoopCPIterator( p, Check )) );
	    }
	ConstLoopIter loopEnd( bool (* Check)( const Loop& )=NULL) const
	    {
	    IterPair<LoopCInter> p( (LoopCInter(begin())), (LoopCInter(end())) );
	    return( ConstLoopIter( LoopCPIterator( p, Check, true )) );
	    }

	void getLoopData(SystemInfo& systeminfo);
	bool findLoop( unsigned num, LoopIter& i );
	bool findLoop( unsigned num ); 
	bool findLoop( const string& file, LoopIter& i );
	bool findLoop( const string& file ); 
	bool findLoopDev( const string& dev, LoopIter& i );
	void addLoop( Loop* m );

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new LoopCo( *this ) ); }

	int doCreate( Volume* v );
	int doRemove( Volume* v );

    private:

	LoopCo& operator=(const LoopCo&); // disallow
	
    };

}

#endif
