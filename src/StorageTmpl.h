#ifndef STORAGE_TMPL_H
#define STORAGE_TMPL_H

#include <y2storage/IterPair.h>
#include <y2storage/FilterIterator.h>
#include <y2storage/DerefIterator.h>

template< class Value > 
class CheckFnc
    {
    public:
        CheckFnc( bool (* ChkFnc)( Value& )=NULL ) : m_check(ChkFnc) {}
	bool operator()(Value& d) const
	    { return(m_check==NULL || (*m_check)(d)); }
	private:
	    bool (* m_check)( Value& d );
    };

template< class Pred, class Iter >
class ContainerIter : public FilterIterator< Pred, Iter >
    {
    typedef FilterIterator< Pred, Iter > _bclass;
    public:
	ContainerIter() : _bclass() {}
	ContainerIter( const Iter& b, const Iter& e, const Pred& p,
		       bool atend=false ) :
	    _bclass(b, e, p, atend ) {}
	ContainerIter( const IterPair<Iter>& pair, const Pred& p,
		       bool atend=false ) :
	    _bclass(pair, p, atend ) {}
	ContainerIter( const ContainerIter& i) { *this=i;}
    };

template< class Pred, class Iter, class Value >
class ContainerDerIter : public DerefIterator<Iter,Value>
    {
    typedef DerefIterator<Iter,Value> _bclass;
    public:
	ContainerDerIter() : _bclass() {}
	ContainerDerIter( const _bclass& i ) : _bclass(i) {}
	ContainerDerIter( const ContainerDerIter& i) { *this=i;}
    };

class Container;

template <int Value>
class CheckType 
    {
    public:
	bool operator()( const Container& d ) const
	    {
	    return( d.type()==Value );
	    }
    };

template< class Iter, int Value, class CastResult >
class CastCheckIterator : public CheckType<Value>, 
                          public FilterIterator< CheckType<Value>, Iter >
    {
    typedef FilterIterator<CheckType<Value>, Iter> _bclass;
    public:
	typedef CastResult value_type;
	typedef CastResult& reference;
	typedef CastResult* pointer;

	CastCheckIterator() : _bclass() {}
	CastCheckIterator( const Iter& b, const Iter& e, bool atend=false) : 
	    _bclass( b, e, *this, atend ) {}
	CastCheckIterator( const IterPair<Iter>& pair, bool atend=false) : 
	    _bclass( pair, *this, atend ) {}
	CastCheckIterator( const CastCheckIterator& i) { *this=i;}
	CastResult operator*() const
	    {
	    return( static_cast<CastResult>(_bclass::operator*()) );
	    }
	CastResult* operator->() const
	    {
	    return( static_cast<CastResult*>(_bclass::operator->()) );
	    }
	CastCheckIterator& operator++() 
	    { 
	    _bclass::operator++(); return(*this); 
	    }
	CastCheckIterator operator++(int) 
	    { 
	    y2warning( "Expensive ++ CastCheckIterator" );
	    CastCheckIterator tmp(*this);
	    _bclass::operator++(); 
	    return(tmp); 
	    }
	CastCheckIterator& operator--() 
	    { 
	    _bclass::operator--(); return(*this); 
	    }
	CastCheckIterator operator--(int) 
	    { 
	    y2warning( "Expensive -- CastCheckIterator" );
	    CastCheckIterator tmp(*this);
	    _bclass::operator--(); 
	    return(tmp); 
	    }
    };

template< class Iter, class CastResult >
class CastIterator : public Iter
    {
    public:
	typedef CastResult value_type;
	typedef CastResult& reference;
	typedef CastResult* pointer;

	CastIterator() : Iter() {}
	CastIterator( const Iter& i ) : Iter( i ) {}
	CastIterator( const CastIterator& i ) { *this=i; }
	CastResult operator*() const
	    {
	    return( static_cast<CastResult>(Iter::operator*()) );
	    }
	CastResult* operator->() const
	    {
	    return( static_cast<CastResult*>(Iter::operator->()) );
	    }
	CastIterator& operator++() 
	    { 
	    Iter::operator++(); return(*this); 
	    }
	CastIterator operator++(int) 
	    { 
	    y2warning( "Expensive ++ CastIterator" );
	    CastIterator tmp(*this);
	    Iter::operator++(); 
	    return(tmp); 
	    }
	CastIterator& operator--() 
	    { 
	    Iter::operator--(); return(*this); 
	    }
	CastIterator operator--(int) 
	    { 
	    y2warning( "Expensive -- CastIterator" );
	    CastIterator tmp(*this);
	    Iter::operator--(); 
	    return(tmp); 
	    }
    };

template < class Checker, class ContIter, class Iter, class Value >
class CheckerIterator : public Checker, public ContIter
    {
    public:
	CheckerIterator() {};
	CheckerIterator( const Iter& b, const Iter& e,
			 bool (* CheckFnc)( const Value& )=NULL,
			 bool atend=false ) :
	    Checker( CheckFnc ),
	    ContIter( b, e, *this, atend ) {}
	CheckerIterator( const IterPair<Iter>& p, 
			 bool (* CheckFnc)( const Value& )=NULL,
			 bool atend=false ) :
	    Checker( CheckFnc ),
	    ContIter( p, *this, atend ) {}
	CheckerIterator( const CheckerIterator& i ) { *this=i; }
    };

template < class C >
void pointerIntoSortedList( list<C*>& l, C* e )
    {
    typename list<C*>::iterator i = l.begin();
    while( i!=l.end() && **i < *e )
	i++;
    l.insert( i, e );
    }


template<class Num> string decString(Num number)
    {
    std::ostringstream num_str;
    num_str << number;
    return( num_str.str() );
    }

template<class Num> string hexString(Num number)
    {
    std::ostringstream num_str;
    num_str << std::hex << number;
    return( num_str.str() );
    }

template<class Value> void operator>>( const string& d, Value& v)
    {
    std::istringstream Data( d );
    Data >> v;
    }

template<class Value> ostream& operator<<( ostream& s, const list<Value>& l )
    {
    s << "<";
    for( typename list<Value>::const_iterator i=l.begin(); i!=l.end(); i++ )
	{
	if( i!=l.begin() )
	    s << " ";
	s << *i;
	}
    s << ">";
    return( s );
    }

template<class F, class S> ostream& operator<<( ostream& s, const pair<F,S>& p )
    {
    s << "[" << p.first << ":" << p.second << "]";
    }

template<class Key, class Value> ostream& operator<<( ostream& s, const map<Key,Value>& m )
    {
    s << "<";
    for( typename map<Key,Value>::const_iterator i=m.begin(); i!=m.end(); i++ )
	{
	if( i!=m.begin() )
	    s << " ";
	s << i->first << ":" << i->second;
	}
    s << ">";
    return( s );
    }



#endif
