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


#ifndef DMMULTIPATH_H
#define DMMULTIPATH_H

#include "storage/DmPart.h"

namespace storage
{

class DmmultipathCo;
class Partition;

class Dmmultipath : public DmPart
    {
    public:

	Dmmultipath(const DmmultipathCo& c, const string& name, const string& device, unsigned nr,
		    Partition* p);
	Dmmultipath(const DmmultipathCo& c, const Dmmultipath& v);
	virtual ~Dmmultipath();

	void getInfo( storage::DmmultipathInfo& info ) const;
	friend std::ostream& operator<< (std::ostream& s, const Dmmultipath &p );
	virtual void print( std::ostream& s ) const { s << *this; }
	Text removeText( bool doing ) const;
	Text createText( bool doing ) const;
	Text formatText( bool doing ) const;
	Text resizeText( bool doing ) const;
	Text setTypeText(bool doing) const;
	bool equalContent( const Dmmultipath& rhs ) const;

	void logDifference(std::ostream& log, const Dmmultipath& rhs) const;

	static bool notDeleted( const Dmmultipath& l ) { return( !l.deleted() ); }

    protected:
	virtual const string shortPrintedName() const { return( "Dmmultipath" ); }

    private:

	Dmmultipath(const Dmmultipath&); // disallow
	Dmmultipath& operator=(const Dmmultipath&); // disallow

    };

}

#endif
