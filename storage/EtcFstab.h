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


#ifndef ETC_FSTAB_H
#define ETC_FSTAB_H

#include <string>
#include <list>
#include <boost/algorithm/string.hpp>

#include "storage/StorageInterface.h"
#include "storage/AppUtil.h"


namespace storage
{
    using std::string;
    using std::list;


class AsciiFile;
struct FstabChange;

struct FstabEntry
    {
	FstabEntry() : freq(0), passno(0), loop(false), dmcrypt(false), noauto(false),
		       cryptotab(false), crypttab(false), tmpcrypt(false), encr(ENC_NONE),
		       mount_by(MOUNTBY_DEVICE) {}

	explicit FstabEntry(const FstabChange& change) { *this = change; }

	FstabEntry& operator=(const FstabChange& rhs);

    friend std::ostream& operator<< (std::ostream& s, const FstabEntry &v );

    string device;
    string dentry;
    string mount;
    string fs;
    std::list<string> opts;
    int freq;
    int passno;
    bool loop;
    bool dmcrypt;
    bool noauto;
    bool cryptotab;
    bool crypttab;
    bool tmpcrypt;
    string loop_dev;
    string cr_opts;
    string cr_key;
    storage::EncryptType encr;
    storage::MountByType mount_by;

    void calcDependent();
    bool optUser() const;
    };


struct FstabChange
    {
	FstabChange() : freq(0), passno(0), encr(ENC_NONE), tmpcrypt(false) {}

	explicit FstabChange(const FstabEntry& entry) { *this = entry; }

	FstabChange& operator=(const FstabEntry& rhs);

    friend std::ostream& operator<< (std::ostream& s, const FstabChange &v );

    string device;
    string dentry;
    string mount;
    string fs;
    std::list<string> opts;
    int freq;
    int passno;
    string loop_dev;
    storage::EncryptType encr;
    bool tmpcrypt;
    };


class EtcFstab
    {
    public:
	EtcFstab( const string& prefix = "", bool rootMounted=true );
	bool findDevice( const string& dev, FstabEntry& entry ) const;
	bool findDevice( const std::list<string>& dl, FstabEntry& entry ) const;
	bool findMount( const string& mount, FstabEntry& entry ) const;
	bool findMount( const string& mount ) const
	    { FstabEntry e; return( findMount( mount,e )); }
	bool findUuidLabel( const string& uuid, const string& label,
			    FstabEntry& entry ) const;
	bool findIdPath(const list<string>& id, const string& path, FstabEntry& entry) const;
	void setDevice( const FstabEntry& entry, const string& device );
	int updateEntry( const FstabChange& entry );
	int addEntry( const FstabChange& entry );
	int removeEntry( const FstabEntry& entry );
	int changeRootPrefix( const string& prfix );
	void getFileBasedLoops( const string& prefix, std::list<FstabEntry>& l ) const;
	list<FstabEntry> getEntries() const;
	Text addText( bool doing, bool crypto, const string& mp ) const;
	Text updateText( bool doing, bool crypto, const string& mp ) const;
	Text removeText( bool doing, bool crypto, const string& mp ) const;
	int flush();

    protected:
	struct Entry
	    {
	    enum operation { NONE, ADD, REMOVE, UPDATE };
	    Entry() : op(NONE) {}
	    operation op;
	    FstabEntry nnew;
	    FstabEntry old;
	    };

	void readFiles();

	int findPrefix( const AsciiFile& tab, const string& mount ) const;

	AsciiFile* findFile( const FstabEntry& e, AsciiFile*& fstab,
			     AsciiFile*& cryptotab, int& lineno ) const;

	bool findCrtab( const FstabEntry& e, const AsciiFile& crtab,
			int& lineno ) const;
	bool findCrtab( const string& device, const AsciiFile& crtab,
			int& lineno ) const;

	string updateLine( const std::list<string>& ol, 
			   const std::list<string>& nl, const string& line ) const;
	string createLine( const std::list<string>& ls, unsigned fields, 
	                   const unsigned* flen ) const;

	string createTabLine( const FstabEntry& e ) const;
	string createCrtabLine( const FstabEntry& e ) const;
	void updateTabLine( list<string>(*fnc)(const FstabEntry& e),
	                    const FstabEntry& old, const FstabEntry& nnew, 
	                    string& line ) const;

	static string fstabEncode(const string&);
	static string fstabDecode(const string&);

	static const unsigned fstabFields[6];
	static const unsigned cryptotabFields[6];
	static const unsigned crypttabFields[6];

	string prefix;
	std::list<Entry> co;
    };

}

#endif
