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


#ifndef DM_CO_H
#define DM_CO_H

#include "storage/PeContainer.h"
#include "storage/Dm.h"

namespace storage
{
    class SystemInfo;


    class CmdDmsetup
    {

    public:

	CmdDmsetup();

	struct Entry
	{
	    Entry() : name(), mjr(0), mnr(0), segments(0), uuid() {}

	    string name;
	    unsigned long mjr;
	    unsigned long mnr;
	    unsigned segments;
	    string uuid;
	};

	bool getEntry(const string& name, Entry& entry) const;

	list<string> getEntries() const;

	template<class Pred>
	list<string> getMatchingEntries(Pred pred) const
	{
	    list<string> ret;
	    for (const_iterator i = data.begin(); i != data.end(); ++i)
		if (pred(i->first))
		    ret.push_back(i->first);
	    return ret;
	}

	typedef map<string, Entry>::const_iterator const_iterator;

	const_iterator begin() const { return data.begin(); }
	const_iterator end() const { return data.end(); }

    private:

	map<string, Entry> data;

    };


class DmCo : public PeContainer
    {
    friend class Storage;

    public:

	DmCo(Storage * const s);
	DmCo(Storage * const s, SystemInfo& systeminfo, bool only_crypt);
	DmCo(const DmCo& c);
	virtual ~DmCo();

	void second(SystemInfo& systeminfo, bool only_crypt);

	static storage::CType staticType() { return storage::DM; }
	friend std::ostream& operator<< (std::ostream&, const DmCo& );
	bool equalContent( const Container& rhs ) const;

	virtual void logDifferenceWithVolumes(std::ostream& log, const Container& rhs) const;

	void updateDmMaps();

	int removeDm( const string& table );
	int removeVolume( Volume* v );
	
    protected:

	void getDmData(SystemInfo& systeminfo);
	void getDmDataCrypt(SystemInfo& systeminfo);
	bool getProcSize(SystemInfo& systeminfo, unsigned nr, unsigned long long& s);
	bool findDm( unsigned num, DmIter& i );
	bool findDm( unsigned num ); 
	bool findDm( const string& dev, DmIter& i );
	bool findDm( const string& dev ); 
	void addDm( Dm* m );
	void checkDm( Dm* m );

	virtual Container* getCopy() const { return( new DmCo( *this ) ); }

	storage::EncryptType detectEncryption( const string& device ) const;

	virtual void print( std::ostream& s ) const { s << *this; }

	int doRemove( Volume* v );

    private:

	DmCo& operator=(const DmCo&);	// disallow

    };

}

#endif
