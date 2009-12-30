/*
 * File: MdPart.h
 *
 * Declaration of MdPart class which represents single partition on MD
 * Device (RAID Volume).
 *
 * Copyright (c) 2009, Intel Corporation.
 * Copyright (c) 2009 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MD_PART_H
#define MD_PART_H


#include "storage/Partition.h"


namespace storage
{

class MdPartCo;
class ProcParts;

class MdPart : public Volume
    {
    public:
        MdPart( const MdPartCo& d, unsigned nr, Partition* p=NULL );
        MdPart( const MdPartCo& d, const MdPart& rd );
        virtual ~MdPart();

        friend std::ostream& operator<< (std::ostream& s, const MdPart &p );
        virtual void print( std::ostream& s ) const { s << *this; }
        void getInfo( storage::MdPartInfo& info ) const;
        void getPartitionInfo(storage::PartitionInfo& pinfo);
        bool equalContent( const MdPart& rhs ) const;
        void logDifference( const MdPart& d ) const;
        void setPtr( Partition* pa ) { p=pa; };
        Partition* getPtr() const { return p; };
        unsigned id() const { return p?p->id():0; }
        void updateName();
        void updateMinor();
        void updateSize(const ProcParts& pp);
        void updateSize();
        void getCommitActions(list<commitAction>& l) const;
        void addUdevData();
        virtual list<string> udevId() const;
        virtual Text setTypeText(bool doing = true) const;
        static bool notDeleted( const MdPart& l ) { return( !l.deleted() ); }

    protected:
        void init( const string& name );
        void dataFromPart( const Partition* p );
        virtual const string shortPrintedName() const { return( "MdPart" ); }
        const MdPartCo* co() const;
        void addAltUdevId( unsigned num );
        Partition* p;

        mutable storage::MdPartInfo info;

    private:

	MdPart(const MdPart&);		  // disallow
	MdPart& operator=(const MdPart&); // disallow

    };

}

#endif
