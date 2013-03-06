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


#ifndef DMRAID_H
#define DMRAID_H

#include "storage/DmPart.h"

namespace storage
{

class DmraidCo;
class Partition;

class Dmraid : public DmPart
    {
    public:

	Dmraid(const DmraidCo& c, const string& name, const string& device, unsigned nr,
	       Partition* p);
	Dmraid(const DmraidCo& c, const string& name, const string& device, unsigned nr,
	       Partition* p, SystemInfo& si);
	Dmraid(const DmraidCo& c, const Dmraid& v);
	virtual ~Dmraid();

	void getInfo( storage::DmraidInfo& info ) const;
	friend std::ostream& operator<< (std::ostream& s, const Dmraid &p );
	virtual void print( std::ostream& s ) const { s << *this; }
	Text removeText( bool doing ) const;
	Text createText( bool doing ) const;
	Text formatText( bool doing ) const;
	Text resizeText( bool doing ) const;
	Text setTypeText(bool doing) const;
	bool equalContent( const Dmraid& rhs ) const;

	void logDifference(std::ostream& log, const Dmraid& rhs) const;

	static bool notDeleted( const Dmraid& l ) { return( !l.deleted() ); }

    protected:
	virtual const string shortPrintedName() const { return( "Dmraid" ); }

    private:

	Dmraid(const Dmraid&);	// disallow
	Dmraid& operator=(const Dmraid&); // disallow

    };

}

#endif
