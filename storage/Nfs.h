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


#ifndef NFS_H
#define NFS_H

#include "storage/Volume.h"

namespace storage
{
class NfsCo;

class Nfs : public Volume
    {
    public:

	Nfs(const NfsCo& c, const string& NfsDev, bool nfs4);
	Nfs(const NfsCo& c, const Nfs& v);
	virtual ~Nfs();

	friend std::ostream& operator<< (std::ostream& s, const Nfs& l );

	static string canonicalName( const string& dev );
	static bool notDeleted( const Nfs& l ) { return( !l.deleted() ); }

	virtual void print( std::ostream& s ) const { s << *this; }

	void getInfo( storage::NfsInfo& info ) const;
	bool equalContent( const Nfs& rhs ) const;

	void logDifference(std::ostream& log, const Nfs& rhs) const;

	Text removeText(bool doing) const;

    protected:

	mutable storage::NfsInfo info; // workaround for broken ycp bindings

    private:

	Nfs(const Nfs&);	    // disallow
	Nfs& operator=(const Nfs&); // disallow

    };

}

#endif
