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


#ifndef DM_H
#define DM_H

#include <map>

#include "storage/Volume.h"

namespace storage
{

class PeContainer;

class Dm : public Volume
    {
    public:

	Dm(const PeContainer& c, const string& name, const string& device, const string& tname);
	Dm(const PeContainer& c, const string& name, const string& device, const string& tname,
	   SystemInfo& systeminfo);
	Dm(const PeContainer& c, const xmlNode* node);
	Dm(const PeContainer& c, const Dm& v);
	virtual ~Dm();

        typedef std::map<string,unsigned long> PeMap;

	void saveData(xmlNode* node) const;

	const string& getTableName() const { return( tname ); }
	const string& getTargetName() const { return( target ); }
	void setTableName( const string& name ) { tname=name; }
	bool inactive() const { return( inactiv ); }
	unsigned long long getLe() const { return num_le; }
	void setLe( unsigned long long le );
	void modifyPeSize( unsigned long long old, unsigned long long neww );
	bool removeTable();
	virtual void calcSize();
	const PeMap& getPeMap() const { return( pe_map ); }
	void setPeMap( const PeMap& m );
	unsigned long long usingPe( const string& dev ) const;
	bool mapsTo( const string& dev ) const;
	void getTableInfo();
	virtual bool checkConsistency() const;
	unsigned stripes() const { return stripe; }
	unsigned setStripes( unsigned long val ) { return stripe=val; }
	unsigned long long stripeSize() const { return stripe_size; }
	void setStripeSize( unsigned long long val ) { stripe_size=val; }

	virtual string procName() const { return "dm-" + decString(mnr); }
	virtual string sysfsPath() const;

	void updateMajorMinor();

	friend std::ostream& operator<< (std::ostream& s, const Dm &p );
	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Text removeText( bool doing ) const;
	virtual Text formatText( bool doing ) const;
	void getInfo( storage::DmInfo& info ) const;
	void changeDeviceName( const string& old, const string& nw );

	static bool notDeleted( const Dm& l ) { return( !l.deleted() ); }
	static bool isDeleted(const Dm& l) { return l.deleted(); }

	static void activate(bool val);
	static bool isActive() { return active; }

	static string devToTable( const string& dev );
	static string lvmTableToDev( const string& table );

	virtual list<string> getUsing() const;

	bool equalContent(const Dm& rhs) const;

	void logDifference(std::ostream& log, const Dm& rhs) const;

	static unsigned dmMajor();
	static string dmDeviceName( unsigned long num );

    protected:
	void init();
	const PeContainer* pec() const;
	virtual const string shortPrintedName() const { return( "Dm" ); }
        unsigned long computeLe( const string& lestr );
        void computePe( const SystemCmd& cmd, PeMap& pe );
        void accumulatePe( const string& majmin, unsigned long le, PeMap& pe );
        void getMapRecursive( unsigned long mi, PeMap& pe );
        static list<string> extractMajMin( const string& s );

	string tname;
	string target;
	unsigned long long num_le;
	unsigned stripe;
	unsigned long long stripe_size;
	bool inactiv;
	bool pe_larger;   // true for table like e.g. thin-pool where sum of used pe´s is larger than device size
	PeMap pe_map;
	static bool active;
	static unsigned dm_major;

	static const list<string> known_types;

	mutable storage::DmInfo info; // workaround for broken ycp bindings

    private:

	Dm(const Dm&);		  // disallow
	Dm& operator=(const Dm&); // disallow

    };

}

#endif
