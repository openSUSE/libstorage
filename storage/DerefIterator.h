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


#ifndef DEREF_ITERATOR_H
#define DEREF_ITERATOR_H

#include "storage/AppUtil.h"

namespace storage
{

template< class Iter, class Value > 
class DerefIterator : public Iter
    {
    public:
	typedef Value value_type;
	typedef Value& reference;
	typedef Value* pointer;
	typedef typename Iter::difference_type difference_type;
	typedef typename Iter::iterator_category iterator_category;

        DerefIterator() {}

	template< class It >
	DerefIterator( const It& i ) : Iter(i) {}

	DerefIterator& operator++() { Iter::operator++(); return(*this); }
	DerefIterator operator++(int) 
	    {
	    y2war( "Expensive ++ DerefIterator" );
	    DerefIterator tmp(*this);
	    Iter::operator++();
	    return(tmp);
	    }
	DerefIterator& operator--() { Iter::operator--(); return(*this); }
	DerefIterator operator--(int) 
	    {
	    y2war( "Expensive -- DerefIterator" );
	    DerefIterator tmp(*this);
	    Iter::operator--();
	    return(tmp);
	    }

	reference operator*() const 
	    {
	    return( *Iter::operator*() );
	    }

	pointer operator->() const 
	    {
	    return( Iter::operator*() );
	    }
    };

}

#endif
