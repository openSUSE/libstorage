/*
 * Copyright (c) [2004-2011] Novell, Inc.
 * Copyright (c) [2015] SUSE LLC
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


#ifndef BTRFS_H
#define BTRFS_H

#include "storage/Volume.h"

namespace storage
{
    class BtrfsCo;
    class SystemInfo;


    class Btrfs : public Volume
    {
    public:

	Btrfs( const BtrfsCo& d, const Volume& v, unsigned long long sz,
	       const list<string>& devices );
	Btrfs( const BtrfsCo& d, const Volume& v );
	Btrfs( const BtrfsCo& d, const xmlNode* node );
	Btrfs( const BtrfsCo& c, const Btrfs& v);
	virtual ~Btrfs();

	virtual const string& device() const override;
	virtual const string& mountDevice() const override;
	void clearSubvolumes() { subvolumes.clear(); }
	void addSubvolume(const string& path, bool nocow);
	int detectSubvolumes(SystemInfo& systeminfo);
	list<string> getDevices( bool add_del=false ) const;
	void getDevices( list<string>& devs, bool add_del=false ) const;
	list<Subvolume> getSubvolumes() const { return subvolumes; }

	bool existSubvolume( const string& name );
	int createSubvolume(const string& name, bool nocow);
	int deleteSubvolume( const string& name );
	int extendVolume( const string& dev );
	int extendVolume( const list<string>& devs );
	int shrinkVolume( const string& dev );
	int shrinkVolume( const list<string>& devs );

	void getCommitActions(list<commitAction>& l) const;
	int doDeleteSubvol();
	int doCreateSubvol();
	int doReduce();
	int doExtend();
	Text createSubvolText(bool doing, const Subvolume& subvolume) const;
	Text deleteSubvolText(bool doing, const Subvolume& subvolume) const;
	Text extendText(bool doing, const string& device) const;
	Text reduceText(bool doing, const string& device) const;
	Text removeText( bool doing ) const;
	Text formatText( bool doing ) const;
	int setFormat( bool format, storage::FsType fs );

	virtual string udevPath() const;
	virtual list<string> udevId() const;
	virtual string sysfsPath() const;

	void countSubvolAddDel(unsigned& add, unsigned& rem) const;
	list<Subvolume> getSubvolAddDel(bool) const;

	void saveData(xmlNode* node) const;
	friend std::ostream& operator<< (std::ostream& s, const Btrfs& l );
	virtual void print( std::ostream& s ) const { s << *this; }
	void getInfo( storage::BtrfsInfo& info ) const;
	bool equalContent( const Btrfs& rhs ) const;
	void logDifference(std::ostream& log, const Btrfs& rhs) const;
	void unuseDev() const;
	int clearSignature();

	void changeDeviceName( const string& old, const string& nw );
	Volume const * findRealVolume() const;
	Volume * findRealVolume();
        bool hasBtrfsCoParent() const;

	static bool notDeleted( const Btrfs& l ) { return( !l.deleted() ); }
	static bool needCreateSubvol( const Btrfs& v );
	static bool needDeleteSubvol( const Btrfs& v );
	static bool needReduce( const Btrfs& v );
	static bool needExtend( const Btrfs& v );

    protected:
	BtrfsCo* co();
	list<string> devices;
	list<string> dev_add;
	list<string> dev_rem;
	list<Subvolume> subvolumes;

	virtual int extraFstabAdd(EtcFstab* fstab, const FstabChange& change) override;
	virtual int extraFstabUpdate(EtcFstab* fstab, const FstabKey& key, const FstabChange& change) override;
	virtual int extraFstabRemove(EtcFstab* fstab, const FstabKey& key) override;

	virtual int extraMount() override;
        const string& realDevice() const;

    private:

	Btrfs& operator=(const Btrfs& v); // disallow
	Btrfs(const Btrfs&);		  // disallow

    };

}

#endif
