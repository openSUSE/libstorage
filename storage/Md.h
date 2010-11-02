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


#ifndef MD_H
#define MD_H

#include "storage/StorageInterface.h"
#include "storage/Volume.h"

namespace storage
{
    class MdCo;
    class EtcMdadm;


class Md : public Volume
    {
	friend class MdPartCo;

    public:
	Md(const MdCo& c, const string& name, const string& device, MdType Type,
	   const list<string>& devs, const list<string>& spares);
	Md(const MdCo& c, const string& name, const string& device, SystemInfo& systeminfo);
	Md(const MdCo& c, const Md& v);
	virtual ~Md();

	storage::MdType personality() const { return md_type; }
	void setPersonality( storage::MdType val ); 
	storage::MdParity parity() const { return md_parity; }
	int setParity( storage::MdParity val ); 
	unsigned long chunkSizeK() const { return chunk_k; }
	void setChunkSizeK(unsigned long val) { chunk_k = val; }
	void setMdUuid( const string&val ) { md_uuid=val; }
	bool destroySb() const { return( destrSb ); }
	void setDestroySb( bool val=true ) { destrSb=val; }
	const string& getMdUuid() const { return(md_uuid); }
	list<string> getDevs(bool all = true, bool spare = false) const;
	int checkDevices();
	int addDevice( const string& dev, bool spare=false );
	int removeDevice( const string& dev );
	string createCmd() const;
	static bool matchRegex( const string& dev );
	static unsigned mdMajor();

	void updateData(SystemInfo& systeminfo);
	void setUdevData(SystemInfo& systeminfo);

	virtual list<string> udevId() const { return udev_id; }

	virtual string procName() const { return nm; }
	virtual string sysfsPath() const;

	static bool mdStringNum( const string& name, unsigned& num ); 
	static string mdDevice( unsigned num );

	friend std::ostream& operator<< (std::ostream& s, const Md& m );
	virtual void print( std::ostream& s ) const { s << *this; }
	Text removeText( bool doing ) const;
	Text createText( bool doing ) const;
	Text formatText( bool doing ) const;
	static bool notDeleted( const Md& l ) { return( !l.deleted() ); }
	void changeDeviceName( const string& old, const string& nw );

	void getInfo( storage::MdInfo& info ) const;
	bool equalContent( const Md& rhs ) const;

	void logDifference(std::ostream& log, const Md& rhs) const;

	int getState(MdStateInfo& info) const;

	bool updateEntry(EtcMdadm* mdadm) const;

    protected:

	void computeSize();

	MdType md_type;
	MdParity md_parity;
	unsigned long chunk_k;
	string md_uuid;
	string md_name;		// line in /dev/md/*
	string sb_ver;
	bool destrSb;
	list<string> devs;
	list<string> spare;

	list<string> udev_id;

	// In case of IMSM and DDF raids there is 'container'.
	bool has_container;
	string parent_container;
	string parent_uuid;
	string parent_md_name;
	string parent_metadata;
	string parent_member;

	static unsigned md_major;

	mutable storage::MdInfo info; // workaround for broken ycp bindings

    private:

	Md(const Md&);		      // disallow
	Md& operator=(const Md&);     // disallow

    };

}

#endif
