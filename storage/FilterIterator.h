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


#ifndef FILTER_ITERATOR_H
#define FILTER_ITERATOR_H

#include "storage/AppUtil.h"
#include "storage/IterPair.h"

namespace storage
{

template< class Pred, class Iter > 
class FilterIterator : public std::iterator<std::bidirectional_iterator_tag, 
                                            typename Iter::value_type>
    {
    public:
	typedef typename Iter::value_type value_type;
	typedef typename Iter::reference reference;
	typedef typename Iter::pointer pointer;
	typedef typename Iter::difference_type difference_type;
	typedef typename Iter::iterator_category iterator_category;

        FilterIterator() { }

	template<class It>
	FilterIterator( const It& begin, const It& end, Pred f, bool setend=false ) : 
	    m_begin(begin), m_end(end), m_cur(begin), m_f(f)
	    {
	    initialize( setend );
	    }
	FilterIterator( const IterPair<Iter>& p, Pred f, bool setend=false ) : 
	    m_begin(p.begin()), m_end(p.end()), m_cur(p.begin()), m_f(f)
	    {
	    initialize( setend );
	    }

	FilterIterator( const FilterIterator& x ) 
	    {
	    copyMembers( x );
	    }

	FilterIterator& operator=(const FilterIterator& x)
	    { 
	    copyMembers( x );
	    return *this; 
	    }

	FilterIterator& operator++()
	    { 
	    ++m_cur;
	    assertPred();
	    return *this; 
	    }
	FilterIterator operator++(int)
	    {
	    y2war( "Expensive ++ FilterIterator" );
	    FilterIterator tmp(*this); 
	    ++m_cur;
	    assertPred();
	    return tmp;
	    }

	FilterIterator& operator--()
	    { 
	    --m_cur;
	    assertPred(false);
	    return *this; 
	    }
	FilterIterator operator--(int)
	    {
	    y2war( "Expensive -- FilterIterator" );
	    FilterIterator tmp(*this); 
	    --m_cur;
	    assertPred(false);
	    return tmp;
	    }

	value_type operator*() const 
	    {
	    return( *m_cur );
	    }

	pointer operator->() const 
	    {
	    return( &(*m_cur) );
	    }
	template <class Other>
	bool operator==(const Other& x) const
	    {
	    return( m_cur == x.cur() );
	    }
	template <class Other>
	bool operator!=(const Other& x) const
	    {
	    return( m_cur != x.cur() );
	    }

	Pred pred() const {return m_f;}
	Iter end() const {return m_end;}
	Iter cur() const {return m_cur;}
	Iter begin() const {return m_begin;}

    protected:
	Iter m_begin;
	Iter m_end;
	Iter m_cur;
	Pred m_f;

	void initialize( bool setend )
	    {
	    if( setend )
		m_cur = m_end;
	    else
		assertPred();
	    }

	void copyMembers( const FilterIterator& x ) 
	    { 
	    m_begin = x.begin();
	    m_end = x.end();
	    m_cur = x.cur();
	    m_f = x.pred();
	    }

	void assertPred( bool forward=true ) 
	    {
	    if( forward )
		while( m_cur!=m_end && !m_f(**m_cur) )
		    ++m_cur;
	    else
		while( m_cur!=m_begin && !m_f(**m_cur) )
		    --m_cur;
	    };

    };

}

#endif
