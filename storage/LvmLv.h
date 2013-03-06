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


#ifndef LVM_LV_H
#define LVM_LV_H

#include "storage/Dm.h"


namespace storage
{

class LvmVg;

class LvmLv : public Dm
    {
    public:

	LvmLv(const LvmVg& c, const string& name, const string& device, const string& origin,
	      unsigned long le, const string& uuid, const string& status, const string& alloc,
	      SystemInfo& si);
	LvmLv(const LvmVg& c, const string& name, const string& device, const string& origin,
	      unsigned long le, unsigned stripe );
	LvmLv(const LvmVg& c, const xmlNode* node);
	LvmLv(const LvmVg& c, const LvmLv& v);
	virtual ~LvmLv();

	void saveData(xmlNode* node) const;

	const LvmVg* vg() const;

	void calcSize();

	void getState(LvmLvSnapshotStateInfo& info);

	void setOrigin( const string& o ) { origin=o; }
	string getOrigin() const { return origin; }

	bool isSnapshot() const { return !origin.empty(); }
	bool hasSnapshots() const;	

	bool isPool() const { return pool; }
	void setPool( bool p=true ) { pool=p; }
	const string& usedPool() const { return used_pool; }
	void setUsedPool( const string& val ) { used_pool=val; }
	bool isThin() const { return !used_pool.empty(); }
	unsigned long long chunkSize() const { return chunk_size; }
	void setChunkSize( unsigned long long val ) { chunk_size=val; }

	void setUuid( const string& uuid ) { vol_uuid=uuid; }
	void setStatus( const string& s ) { status=s; }
	void setAlloc( const string& a ) { allocation=a; }

	friend std::ostream& operator<< (std::ostream& s, const LvmLv &p );
        bool operator< ( const LvmLv& rhs ) const;
        bool operator<= ( const LvmLv& rhs ) const
            { return( *this<rhs || *this==rhs ); }
        bool operator>= ( const LvmLv& rhs ) const
            { return( !(*this<rhs) ); }
        bool operator> ( const LvmLv& rhs ) const
            { return( !(*this<=rhs) ); }

	virtual void print( std::ostream& s ) const { s << *this; }
	Text removeText( bool doing ) const;
	Text createText( bool doing ) const;
	Text formatText( bool doing ) const;
	Text resizeText( bool doing ) const;
	int setFormat( bool format, storage::FsType fs );
	int changeMount( const string& val );
	void getInfo( storage::LvmLvInfo& info ) const;
	bool equalContent( const LvmLv& rhs ) const;
	virtual list<string> getUsing() const;
	void logDifference(std::ostream& log, const LvmLv& rhs) const;

	static bool notDeleted(const LvmLv& l) { return !l.deleted(); }

    protected:
	static string makeDmTableName(const string& vg_name, const string& lv_name);

	virtual const string shortPrintedName() const { return "Lv"; }

	string origin;		// only for snapshots, empty otherwise

	string vol_uuid;
	string status;
	string allocation;
	string used_pool;       // indicates a thin volumes, empty otherwise
        unsigned long long chunk_size;
        bool   pool;

	mutable storage::LvmLvInfo info; // workaround for broken ycp bindings

    private:

	LvmLv(const LvmLv&);		// disallow
	LvmLv& operator=(const LvmLv&); // disallow

    };

}

#endif
