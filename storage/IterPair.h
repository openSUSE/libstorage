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


#ifndef ITER_PAIR_H
#define ITER_PAIR_H

#include <iterator>

namespace storage
{

template< class Iter >
class IterPair
    {
    public:
	typedef Iter itype; 
	IterPair(const Iter& b, const Iter& e) : m_begin(b), m_end(e) {}
	IterPair( const IterPair& x ) 
	    {
	    *this = x;
	    }

	template <class Ip>
	IterPair(const Ip& x) : m_begin(x.begin()), m_end(x.end()) {}

	template< class Ip >
	IterPair& operator=(const Ip& x) 
	    { 
	    m_begin=x.begin(); 
	    m_end=x.end();
	    return( *this );
	    }
	template< class Ip >
	bool operator==(const Ip& x) const
	    { 
	    return( m_begin==x.begin() && m_end==x.end() );
	    }
	bool empty() const { return( m_begin==m_end ); }
	unsigned length() const { return( std::distance( m_begin, m_end )); }
	Iter begin() const { return( m_begin ); }
	Iter end() const { return( m_end ); }
    protected:
	Iter m_begin;
	Iter m_end;
    };

template< class Container, class Iter >
IterPair<Iter> MakeIterPair( Container& c )
    {
    return( IterPair<Iter>( c.begin(), c.end() ));
    }

template< class Pred, class Iter >
class MakeCondIterPair : public IterPair<Iter>
    {
    typedef IterPair<Iter> _bclass;
    public:
	MakeCondIterPair( const Iter& b, const Iter& e ) :
	    _bclass( b, e ) {}
    };

}

#endif
