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

	DerefIterator( const Iter& i ) : Iter(i) {}

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
