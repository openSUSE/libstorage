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


#ifndef REGION_H
#define REGION_H

#include <algorithm>

namespace storage
{

class Region 
    {
    public:
	Region() : s(0), l(0) {}
	Region( unsigned long start, unsigned long len ) : s(start), l(len) {}
	Region( const Region& x ) 
	    { *this = x; }
	Region& operator=(const Region& r)
	    { s = r.start(); l = r.len(); return( *this ); }
	bool doIntersect( const Region& r ) const
	    { return( r.start() <= end() && r.end() >= start() ); }
	Region intersect( const Region& r ) const
	    {
	    Region ret;
	    if (doIntersect(r))
		{
		unsigned long s = std::max( r.start(), start() );
		unsigned long e = std::min( r.end(), end() );
		ret = Region( s, e-s+1 );
		}
	    return( ret );
	    }
	bool inside( const Region& r ) const
	    { return( start()>=r.start() && end() <= r.end() ); }
	bool operator==(const Region& r) const
	    { return( r.start()==s && r.len()==l ); }
	bool operator!=(const Region& r) const
	    { return( ! (*this==r) ); }
	bool operator<(const Region& r) const
	    { return( s < r.start() ); }
	bool operator>(const Region& r) const
	    { return( s > r.start() ); }
	unsigned long start() const { return( s ); }
	unsigned long end() const { return( s+l-1 ); }
	unsigned long len() const { return( l ); }

	friend std::ostream& operator<<(std::ostream& s, const Region& p);
	friend std::istream& operator>>(std::istream& s, Region& p);

    protected:
	unsigned long s;
	unsigned long l;
    };


    inline std::ostream& operator<<(std::ostream& s, const Region& p)
    {
	return s << "[" << p.s << "," << p.l << "]";
    }


    inline std::istream& operator>>(std::istream& s, Region& p)
    {
	return s >> p.s >> p.l;
    }

}

#endif
