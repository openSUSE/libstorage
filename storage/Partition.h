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


#ifndef PARTITION_H
#define PARTITION_H

#include "storage/StorageInterface.h"
#include "storage/Volume.h"
#include "storage/Region.h"

namespace storage
{

class Disk;

class Partition : public Volume
    {
    public:

	typedef enum { ID_DOS16=0x6, ID_DOS=0x0c, ID_NTFS=0x07,
	               ID_EXTENDED=0x0f,
		       ID_LINUX=0x83, ID_SWAP=0x82, ID_LVM=0x8e, ID_RAID=0xfd,
		       ID_APPLE_OTHER=0x101, ID_APPLE_HFS=0x102,
		       ID_GPT_BOOT=0x103, ID_GPT_SERVICE=0x104,
		       ID_GPT_MSFTRES=0x105, ID_APPLE_UFS=0x106 } IdNum;

	Partition(const Disk& c, const string& name, const string& device, unsigned Pnr,
		  unsigned long long SizeK, unsigned long Start, unsigned long CSize,
		  PartitionType Type, unsigned id = ID_LINUX, bool Boot = false);
	Partition(const Disk& c, const xmlNode* node);
	Partition(const Disk& c, const Partition& v);
	virtual ~Partition();

	void saveData(xmlNode* node) const;

	unsigned long cylStart() const { return reg.start(); }
	unsigned long cylSize() const { return reg.len(); }
	unsigned long cylEnd() const { return reg.end(); }
	const Region& region() const { return reg; }
	virtual string udevPath() const;
	virtual list<string> udevId() const;

	virtual string procName() const { return nm; }
	virtual string sysfsPath() const;

	bool intersectArea( const Region& r, unsigned fuzz=0 ) const;
	bool contains( const Region& r, unsigned fuzz=0 ) const;
	unsigned OrigNr() const { return( orig_num ); }
	bool boot() const { return bootflag; }
	unsigned id() const { return idt; }
	storage::PartitionType type() const { return typ; }

	void changeRegion( unsigned long Start, unsigned long CSize,
	                   unsigned long long SizeK );
	void changeNumber( unsigned new_num );
	int changeId(unsigned id);
	void changeIdDone();
	void unChangeId();
	Text removeText(bool doing) const;
	Text createText(bool doing) const;
	Text formatText(bool doing) const;
	Text resizeText(bool doing) const;
	void getCommitActions(list<commitAction>& l) const;
	Text setTypeText(bool doing) const;
	int setFormat( bool format=true, storage::FsType fs=storage::REISERFS );
	int changeMount( const string& val );
	const Disk* disk() const;
	bool isWindows() const;
	friend std::ostream& operator<< (std::ostream& s, const Partition &p );
	static bool notDeleted( const Partition&d ) { return( !d.deleted() ); }
	static bool toChangeId( const Partition&d ) { return( !d.deleted() && d.idt!=d.orig_id ); }
	virtual void print( std::ostream& s ) const { s << *this; }
	void setResizedSize( unsigned long long SizeK );
	void forgetResize(); 
	bool canUseDevice() const;

	static PartitionType toPartitionType(const string& val);
	static const string& partitionTypeString(PartitionType val) { return pt_names[val]; }

	void getInfo( storage::PartitionInfo& info ) const;
	void getInfo( storage::PartitionAddInfo& info ) const;

	bool equalContent( const Partition& rhs ) const;
	void logDifference( const Partition& d ) const;
	void addUdevData();

	int zeroIfNeeded() const;

	bool operator== ( const Partition& rhs ) const;
	bool operator!= ( const Partition& rhs ) const
	    { return( !(*this==rhs) ); }
	bool operator< ( const Partition& rhs ) const;
	bool operator<= ( const Partition& rhs ) const
	    { return( *this<rhs || *this==rhs ); }
	bool operator>= ( const Partition& rhs ) const
	    { return( !(*this<rhs) ); }
	bool operator> ( const Partition& rhs ) const
	    { return( !(*this<=rhs) ); }

    protected:

	Region reg;
	bool bootflag;
	storage::PartitionType typ;
	unsigned idt;
	unsigned orig_id;
	unsigned orig_num;

	void addAltUdevId( unsigned num );
	void addAltUdevPath( unsigned num );

	static const string pt_names[storage::PTYPE_ANY + 1];

	mutable storage::PartitionInfo info; // workaround for broken ycp bindings

    private:

	Partition(const Partition&); // disallow
	Partition& operator=(const Partition&); // disallow

    };

}

#endif
