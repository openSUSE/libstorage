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
	IterPair( const Iter b, const Iter e ) : m_begin(b), m_end(e) {}
	IterPair( const IterPair& x ) 
	    {
	    *this = x;
	    }
	IterPair& operator=(const IterPair& x) 
	    { 
	    m_begin=x.m_begin; 
	    m_end=x.m_end;
	    return( *this );
	    }
	bool operator==(const IterPair& x) const
	    { 
	    return( m_begin==x.m_begin && m_end==x.m_end );
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
