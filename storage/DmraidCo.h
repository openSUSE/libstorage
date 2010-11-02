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


#ifndef DMRAID_CO_H
#define DMRAID_CO_H

#include <list>

#include "storage/DmPartCo.h"
#include "storage/Dmraid.h"

namespace storage
{
class Storage;
    class SystemInfo;


    class CmdDmraid
    {

    public:

	CmdDmraid();

	struct Entry
	{
	    string raidtype;
	    string controller;
	    list<string> devices;
	};

	list<string> getEntries() const;

	bool getEntry(const string& name, Entry& entry) const;

    private:

	typedef map<string, Entry>::const_iterator const_iterator;

	map<string, Entry> data;

    };


class DmraidCo : public DmPartCo
    {
    friend class Storage;

    public:

	DmraidCo(Storage* s, const string& name, const string& device, SystemInfo& systeminfo);
	DmraidCo(const DmraidCo& c);
	virtual ~DmraidCo();

	static storage::CType staticType() { return storage::DMRAID; }
	friend std::ostream& operator<< (std::ostream&, const DmraidCo& );
	void getInfo( storage::DmraidCoInfo& info ) const;
	void setUdevData(const list<string>& id);

	bool equalContent( const Container& rhs ) const;

	void logDifference(std::ostream& log, const DmraidCo& rhs) const;
	virtual void logDifferenceWithVolumes(std::ostream& log, const Container& rhs) const;

    protected:

	// iterators over partitions
        // protected typedefs for iterators over partitions
        typedef CastIterator<VIter, Dmraid *> DmraidInter;
        typedef CastIterator<CVIter, const Dmraid *> DmraidCInter;
        template< class Pred >
            struct DmraidPI { typedef ContainerIter<Pred, DmraidInter> type; };
        template< class Pred >
            struct DmraidCPI { typedef ContainerIter<Pred, DmraidCInter> type; };
        typedef CheckFnc<const Dmraid> CheckFncDmraid;
        typedef CheckerIterator< CheckFncDmraid, DmraidPI<CheckFncDmraid>::type,
                                 DmraidInter, Dmraid > DmraidPIterator;
        typedef CheckerIterator< CheckFncDmraid, DmraidCPI<CheckFncDmraid>::type,
                                 DmraidCInter, const Dmraid > DmraidCPIterator;
	typedef DerefIterator<DmraidPIterator,Dmraid> DmraidIter;
	typedef DerefIterator<DmraidCPIterator,const Dmraid> ConstDmraidIter;
        typedef IterPair<DmraidIter> DmraidPair;
        typedef IterPair<ConstDmraidIter> ConstDmraidPair;

        DmraidPair dmraidPair( bool (* CheckDmraid)( const Dmraid& )=NULL)
            {
            return( DmraidPair( dmraidBegin( CheckDmraid ), dmraidEnd( CheckDmraid ) ));
            }
        DmraidIter dmraidBegin( bool (* CheckDmraid)( const Dmraid& )=NULL)
            {
	    IterPair<DmraidInter> p( (DmraidInter(begin())), (DmraidInter(end())) );
            return( DmraidIter( DmraidPIterator( p, CheckDmraid )) );
	    }
        DmraidIter dmraidEnd( bool (* CheckDmraid)( const Dmraid& )=NULL)
            {
	    IterPair<DmraidInter> p( (DmraidInter(begin())), (DmraidInter(end())) );
            return( DmraidIter( DmraidPIterator( p, CheckDmraid, true )) );
	    }

        ConstDmraidPair dmraidPair( bool (* CheckDmraid)( const Dmraid& )=NULL) const
            {
            return( ConstDmraidPair( dmraidBegin( CheckDmraid ), dmraidEnd( CheckDmraid ) ));
            }
        ConstDmraidIter dmraidBegin( bool (* CheckDmraid)( const Dmraid& )=NULL) const
            {
	    IterPair<DmraidCInter> p( (DmraidCInter(begin())), (DmraidCInter(end())) );
            return( ConstDmraidIter( DmraidCPIterator( p, CheckDmraid )) );
	    }
        ConstDmraidIter dmraidEnd( bool (* CheckDmraid)( const Dmraid& )=NULL) const
            {
	    IterPair<DmraidCInter> p( (DmraidCInter(begin())), (DmraidCInter(end())) );
            return( ConstDmraidIter( DmraidCPIterator( p, CheckDmraid, true )) );
	    }

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new DmraidCo( *this ) ); }
	static void activate(bool val);
	void getRaidData(const string& name, SystemInfo& systeminfo);
	void addPv(const Pv& pv);
	void newP( DmPart*& dm, unsigned num, Partition* p );

	Text removeText( bool doing ) const;

	static list<string> getRaids(SystemInfo& systeminfo);

	int doRemove();

	string raidtype;
	string controller;

	static bool active; 

    private:

	DmraidCo& operator=(const DmraidCo&); // disallow

    };

}

#endif
