/*
 * Copyright (c) [2004-2011] Novell, Inc.
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


#ifndef TMPFS_H
#define TMPFS_H

#include "storage/Volume.h"

namespace storage
{
class TmpfsCo;
class SystemInfo;


class Tmpfs : public Volume
    {
    public:

	Tmpfs( const TmpfsCo& d, const string& mp, bool mounted );
	Tmpfs( const TmpfsCo& d, const Tmpfs& v );
	Tmpfs( const TmpfsCo& d, const xmlNode* node );
	virtual ~Tmpfs();

	Text removeText( bool doing ) const;
	Text mountText( bool doing ) const;

	void saveData(xmlNode* node) const;
	friend std::ostream& operator<< (std::ostream& s, const Tmpfs& l );
	virtual void print( std::ostream& s ) const { s << *this; }
	void getInfo( storage::TmpfsInfo& info ) const;
	bool equalContent( const Tmpfs& rhs ) const;
	void logDifference(std::ostream& log, const Tmpfs& rhs) const;

	static bool notDeleted( const Tmpfs& l ) { return( !l.deleted() ); }

    protected:
	mutable storage::TmpfsInfo info; // workaround for broken ycp bindings

    private:

	Tmpfs& operator=(const Tmpfs& v); // disallow
	Tmpfs(const Tmpfs&);	          // disallow

    };

}

#endif
