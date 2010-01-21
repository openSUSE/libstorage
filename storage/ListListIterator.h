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


#ifndef LIST_LIST_ITERATOR_H
#define LIST_LIST_ITERATOR_H

#include "storage/AppUtil.h"
#include "storage/IterPair.h"

namespace storage
{

template< class PIter, class Iter > 
class ListListIterator : public PIter
    {
    public:
	typedef typename PIter::value_type value_type;
	typedef typename PIter::reference reference;
	typedef typename PIter::pointer pointer;
	typedef typename PIter::difference_type difference_type;
	typedef typename PIter::iterator_category iterator_category;

        ListListIterator() { }

	ListListIterator( const Iter& begin, const Iter& end, bool setend=false ) : 
	    m_lcur(begin)
	    {
	    initialize( begin, end, setend );
	    }

	ListListIterator( const IterPair<Iter>& p, bool setend=false ) : 
	    m_lcur(p.begin())
	    {
	    initialize( p.begin(), p.end(), setend );
	    }

	template< class It >
	ListListIterator( const It& x )
	    {
	    m_begin = x.begin();
	    m_end = x.end();
	    m_lcur = x.cur();
	    m_pcur = x.pcur();
	    }

	ListListIterator& operator=(const ListListIterator& x)
	    { 
	    copyMembers( x );
	    return *this; 
	    }

	ListListIterator& operator++()
	    { 
	    increment();
	    return *this; 
	    }
	ListListIterator operator++(int)
	    {
	    y2war( "Expensive ++ ListListIterator" );
	    ListListIterator tmp(*this); 
	    increment();
	    return tmp;
	    }

	ListListIterator& operator--()
	    { 
	    decrement();
	    return *this; 
	    }
	ListListIterator operator--(int)
	    {
	    y2war( "Expensive -- ListListIterator" );
	    ListListIterator tmp(*this); 
	    decrement();
	    return tmp;
	    }

	reference operator*() const 
	    {
	    return( *m_pcur );
	    }

	pointer operator->() const 
	    {
	    return( &(*m_pcur) );
	    }
	template< class It >
	bool operator==(const It& x) const
	    {
	    return( m_pcur == x.pcur() );
	    }
	template< class It >
	bool operator!=(const It& x) const
	    {
	    return( m_pcur != x.pcur() );
	    }

	PIter end() const {return m_end;}
	PIter begin() const {return m_begin;}
	PIter pcur() const {return m_pcur;}
	Iter cur() const {return m_lcur;}

    private:
	PIter m_begin;
	PIter m_end;
	Iter m_lcur;
	PIter m_pcur;
	static const PIter empty;

	void copyMembers( const ListListIterator& x ) 
	    { 
	    m_begin = x.begin();
	    m_end = x.end();
	    m_lcur = x.cur();
	    m_pcur = x.pcur();
	    }

	void increment() 
	    {
	    if( m_pcur != m_end ) 
		{
		++m_pcur;
		while( m_pcur!=m_end && m_pcur == m_lcur->end() )
		    {
		    ++m_lcur;
		    m_pcur = m_lcur->begin();
		    }
		}
	    };

	void decrement() 
	    {
	    if( m_pcur != m_begin ) 
		{
		if( m_pcur != m_lcur->begin() )
		    {
		    --m_pcur;
		    }
		else 
		    {
		    do
			{
			--m_lcur;
			m_pcur = --(m_lcur->end());
			}
		    while( m_pcur!=m_begin && 
		           m_lcur->begin()==m_lcur->end() );
		    }
		}
	    };

	void initialize( const Iter& begin, const Iter& end, bool setend )
	    {
	    m_begin = m_end = m_pcur = empty;
	    while( m_lcur != end && m_lcur->begin()==m_lcur->end() )
		{
		++m_lcur;
		}
	    if( m_lcur != end )
		m_begin = m_lcur->begin();
	    if( begin != end )
		{
		Iter tmp = end;
		--tmp;
		while( tmp != begin && tmp->begin()==tmp->end() )
		    {
		    --tmp;
		    }
		if( tmp != begin || begin->begin()!=begin->end())
		    {
		    m_end = tmp->end();
		    }
		if( setend )
		    {
		    m_pcur = m_end;
		    m_lcur = tmp;
		    }
		else
		    m_pcur = m_begin;
		}
	    }

    };

template< typename PIter, typename Iter > const PIter ListListIterator<PIter,Iter>::empty = PIter();

}

#endif
