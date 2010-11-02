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


#ifndef DMMULTIPATH_CO_H
#define DMMULTIPATH_CO_H

#include <list>

#include "storage/DmPartCo.h"
#include "storage/Dmmultipath.h"

namespace storage
{

class Storage;
    class SystemInfo;


    class CmdMultipath
    {

    public:

	CmdMultipath();

	struct Entry
	{
	    string vendor;
	    string model;
	    list<string> devices;
	};

	list<string> getEntries() const;

	bool getEntry(const string& name, Entry& entry) const;

    private:

	typedef map<string, Entry>::const_iterator const_iterator;

	map<string, Entry> data;

    };


class DmmultipathCo : public DmPartCo
    {
    friend class Storage;

    public:

	DmmultipathCo(Storage* s, const string& name, const string& device, SystemInfo& systeminfo);
	DmmultipathCo(const DmmultipathCo& c);
	virtual ~DmmultipathCo();

	static storage::CType staticType() { return storage::DMMULTIPATH; }
	friend std::ostream& operator<< (std::ostream&, const DmmultipathCo& );
	void getInfo( storage::DmmultipathCoInfo& info ) const;
	void setUdevData(const list<string>& id);

	bool equalContent( const Container& rhs ) const;

	void logDifference(std::ostream& log, const DmmultipathCo& rhs) const;
	virtual void logDifferenceWithVolumes(std::ostream& log, const Container& rhs) const;

    protected:

	// iterators over partitions
        // protected typedefs for iterators over partitions
        typedef CastIterator<VIter, Dmmultipath *> DmmultipathInter;
        typedef CastIterator<CVIter, const Dmmultipath *> DmmultipathCInter;
        template< class Pred >
            struct DmmultipathPI { typedef ContainerIter<Pred, DmmultipathInter> type; };
        template< class Pred >
            struct DmmultipathCPI { typedef ContainerIter<Pred, DmmultipathCInter> type; };
        typedef CheckFnc<const Dmmultipath> CheckFncDmmultipath;
        typedef CheckerIterator< CheckFncDmmultipath, DmmultipathPI<CheckFncDmmultipath>::type,
                                 DmmultipathInter, Dmmultipath > DmmultipathPIterator;
        typedef CheckerIterator< CheckFncDmmultipath, DmmultipathCPI<CheckFncDmmultipath>::type,
                                 DmmultipathCInter, const Dmmultipath > DmmultipathCPIterator;
	typedef DerefIterator<DmmultipathPIterator,Dmmultipath> DmmultipathIter;
	typedef DerefIterator<DmmultipathCPIterator,const Dmmultipath> ConstDmmultipathIter;
        typedef IterPair<DmmultipathIter> DmmultipathPair;
        typedef IterPair<ConstDmmultipathIter> ConstDmmultipathPair;

        DmmultipathPair dmmultipathPair( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL)
            {
            return( DmmultipathPair( dmmultipathBegin( CheckDmmultipath ), dmmultipathEnd( CheckDmmultipath ) ));
            }
        DmmultipathIter dmmultipathBegin( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL)
            {
	    IterPair<DmmultipathInter> p( (DmmultipathInter(begin())), (DmmultipathInter(end())) );
            return( DmmultipathIter( DmmultipathPIterator( p, CheckDmmultipath )) );
	    }
        DmmultipathIter dmmultipathEnd( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL)
            {
	    IterPair<DmmultipathInter> p( (DmmultipathInter(begin())), (DmmultipathInter(end())) );
            return( DmmultipathIter( DmmultipathPIterator( p, CheckDmmultipath, true )) );
	    }

        ConstDmmultipathPair dmmultipathPair( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL) const
            {
            return( ConstDmmultipathPair( dmmultipathBegin( CheckDmmultipath ), dmmultipathEnd( CheckDmmultipath ) ));
            }
        ConstDmmultipathIter dmmultipathBegin( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL) const
            {
	    IterPair<DmmultipathCInter> p( (DmmultipathCInter(begin())), (DmmultipathCInter(end())) );
            return( ConstDmmultipathIter( DmmultipathCPIterator( p, CheckDmmultipath )) );
	    }
        ConstDmmultipathIter dmmultipathEnd( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL) const
            {
	    IterPair<DmmultipathCInter> p( (DmmultipathCInter(begin())), (DmmultipathCInter(end())) );
            return( ConstDmmultipathIter( DmmultipathCPIterator( p, CheckDmmultipath, true )) );
	    }

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new DmmultipathCo( *this ) ); }
	void getMultipathData(const string& name, SystemInfo& systeminfo);
	void addPv(const Pv& pv);
	void newP( DmPart*& dm, unsigned num, Partition* p );

	static void activate(bool val);
	static bool isActive() { return active; }

	static list<string> getMultipaths(SystemInfo& systeminfo);

	string vendor;
	string model;

	static bool active;

    private:

	DmmultipathCo& operator=(const DmmultipathCo&); // disallow

    };

}

#endif
