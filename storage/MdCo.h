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


#ifndef MD_CO_H
#define MD_CO_H

#include "storage/Container.h"
#include "storage/Md.h"

namespace storage
{

    class EtcMdadm;

class MdCo : public Container
    {
    friend class Storage;

    public:

	MdCo(Storage * const s);
	MdCo(Storage * const s, SystemInfo& systeminfo);
	MdCo(const MdCo& c);	
	virtual ~MdCo();

	static storage::CType staticType() { return storage::MD; }
	friend std::ostream& operator<< (std::ostream&, const MdCo& );

	int createMd(unsigned num, MdType type, const list<string>& devs, const list<string>& spares);
	int removeMd( unsigned num, bool destroySb=true );
	int extendMd(unsigned num, const list<string>& devs, const list<string>& spares);
	int shrinkMd(unsigned num, const list<string>& devs, const list<string>& spares);
	int changeMdType( unsigned num, storage::MdType ptype );
	int changeMdChunk( unsigned num, unsigned long chunk );
	int changeMdParity( unsigned num, storage::MdParity ptype );
	int checkMd( unsigned num );
	int getMdState(unsigned num, MdStateInfo& info);
	bool equalContent( const Container& rhs ) const;

	virtual void logDifferenceWithVolumes(std::ostream& log, const Container& rhs) const;

	void syncMdadm(EtcMdadm* mdadm) const;

	void changeDeviceName( const string& old, const string& nw );

	static void activate(bool val, const string& tmpDir);
	int removeVolume( Volume* v );
	
	/* returns in 'nums' numbers that are used by Md */
	list<unsigned> usedNumbers() const;

    protected:
	// iterators over MD volumes
	// protected typedefs for iterators over MD volumes
	typedef CastIterator<VIter, Md *> MdInter;
	typedef CastIterator<CVIter, const Md *> MdCInter;
	template< class Pred >
	    struct MdPI { typedef ContainerIter<Pred, MdInter> type; };
	template< class Pred >
	    struct MdCPI { typedef ContainerIter<Pred, MdCInter> type; };
	typedef CheckFnc<const Md> CheckFncMd;
	typedef CheckerIterator< CheckFncMd, MdPI<CheckFncMd>::type,
				 MdInter, Md > MdPIterator;
	typedef CheckerIterator< CheckFncMd, MdCPI<CheckFncMd>::type,
				 MdCInter, const Md > MdCPIterator;
	typedef DerefIterator<MdPIterator,Md> MdIter;
	typedef DerefIterator<MdCPIterator,const Md> ConstMdIter;
	typedef IterPair<MdIter> MdPair;
	typedef IterPair<ConstMdIter> ConstMdPair;

	MdPair mdPair( bool (* Check)( const Md& )=NULL)
	    {
	    return( MdPair( mdBegin( Check ), mdEnd( Check ) ));
	    }
	MdIter mdBegin( bool (* Check)( const Md& )=NULL)
	    {
	    IterPair<MdInter> p( (MdInter(begin())), (MdInter(end())) );
	    return( MdIter( MdPIterator( p, Check )) );
	    }
	MdIter mdEnd( bool (* Check)( const Md& )=NULL)
	    {
	    IterPair<MdInter> p( (MdInter(begin())), (MdInter(end())) );
	    return( MdIter( MdPIterator( p, Check, true )) );
	    }

	ConstMdPair mdPair( bool (* Check)( const Md& )=NULL) const
	    {
	    return( ConstMdPair( mdBegin( Check ), mdEnd( Check ) ));
	    }
	ConstMdIter mdBegin( bool (* Check)( const Md& )=NULL) const
	    {
	    IterPair<MdCInter> p( (MdCInter(begin())), (MdCInter(end())) );
	    return( ConstMdIter( MdCPIterator( p, Check )) );
	    }
	ConstMdIter mdEnd( bool (* Check)( const Md& )=NULL) const
	    {
	    IterPair<MdCInter> p( (MdCInter(begin())), (MdCInter(end())) );
	    return( ConstMdIter( MdCPIterator( p, Check, true )) );
	    }

	bool findMd( unsigned num, MdIter& i );
	bool findMd( unsigned num ); 
	bool findMd( const string& dev, MdIter& i );
	bool findMd( const string& dev ); 
	int checkUse(const list<string>& dev, const list<string>& spares) const;
	void addMd( Md* m );

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new MdCo( *this ) ); }

	int doCreate( Volume* v );
	int doRemove( Volume* v );

	/* Return true if given device is alredy handled by MdPartCo. */
	bool isHandledByMdPart(const string& name) const;

	static bool active;  

    private:

	MdCo& operator=(const MdCo&); // disallow

    };

}

#endif
