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


#ifndef DASD_H
#define DASD_H

#include "storage/Disk.h"


namespace storage
{
    using std::list;


    class ProcParts;


class Dasd : public Disk
    {
    friend class Storage;

    public:

	enum DasdFormat { DASDF_NONE, DASDF_LDL, DASDF_CDL };

	Dasd(Storage* s, const string& name, const string& device, unsigned long long Size,
	     SystemInfo& systeminfo);
	Dasd(const Dasd& c);
	virtual ~Dasd();

        int createPartition( storage::PartitionType type, long unsigned start,
	                     long unsigned len, string& device,
			     bool checkRelaxed=false );
        int removePartition( unsigned nr );
        int changePartitionId( unsigned nr, unsigned id ) { return 0; }
        int resizePartition( Partition* p, unsigned long newCyl );
	int initializeDisk( bool value );
	Text fdasdText() const;
	Text dasdfmtText( bool doing ) const;
	static Text dasdfmtTexts(bool doing, const list<string>& devs);
	void getCommitActions(list<commitAction>& l) const;
	void getToCommit(storage::CommitStage stage, list<const Container*>& col,
			 list<const Volume*>& vol) const;
	int commitChanges( storage::CommitStage stage );

    protected:

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new Dasd( *this ) ); }
	bool detectPartitionsFdasd(SystemInfo& systeminfo);
	bool detectPartitions(SystemInfo& systeminfo);
	virtual bool checkPartitionsValid(SystemInfo& systeminfo, const list<Partition*>& pl) const;
	bool checkFdasdOutput(SystemInfo& systeminfo);
	void redetectGeometry() {};
        int doCreate( Volume* v ) { return(doFdasd()); }
        int doRemove( Volume* v ) { return(init_disk?0:doFdasd()); }
	int doFdasd();
        int doResize( Volume* v );
        int doSetType( Volume* v ) { return 0; }
        int doCreateLabel() { return 0; }
	int doDasdfmt();

	DasdFormat fmt;

	friend std::ostream& operator<< (std::ostream&, const Dasd& );

    private:

	Dasd& operator=(const Dasd&); // disallow

    };


    template <> struct EnumInfo<Dasd::DasdFormat> { static const vector<string> names; };

}

#endif
