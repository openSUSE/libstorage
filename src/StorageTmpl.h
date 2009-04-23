#ifndef STORAGE_TMPL_H
#define STORAGE_TMPL_H

#include <functional>
#include <ostream>
#include <sstream>
#include <list>
#include <map>
#include <deque>

#include "y2storage/IterPair.h"
#include "y2storage/FilterIterator.h"
#include "y2storage/DerefIterator.h"
#include "y2storage/AppUtil.h"

namespace storage
{

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
	    y2war( "Expensive ++ CastIterator" );
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
	    y2war( "Expensive -- CastIterator" );
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


template<class Num> string decString(Num number)
{
    std::ostringstream num_str;
    classic(num_str);
    num_str << number;
    return num_str.str();
}

template<class Num> string hexString(Num number)
{
    std::ostringstream num_str;
    classic(num_str);
    num_str << std::hex << number;
    return num_str.str();
}

template<class Value> void operator>>(const string& d, Value& v)
{
    std::istringstream Data(d);
    classic(Data);
    Data >> v;
}

template<class Value> std::ostream& operator<<( std::ostream& s, const std::list<Value>& l )
    {
    s << "<";
    for( typename std::list<Value>::const_iterator i=l.begin(); i!=l.end(); i++ )
	{
	if( i!=l.begin() )
	    s << " ";
	s << *i;
	}
    s << ">";
    return( s );
    }

template<class Value> std::ostream& operator<<( std::ostream& s, const std::deque<Value>& l )
    {
    s << "<";
    for( typename std::deque<Value>::const_iterator i=l.begin(); i!=l.end(); i++ )
	{
	if( i!=l.begin() )
	    s << " ";
	s << *i;
	}
    s << ">";
    return( s );
    }

template<class F, class S> std::ostream& operator<<( std::ostream& s, const std::pair<F,S>& p )
    {
    s << "[" << p.first << ":" << p.second << "]";
    return( s );
    }

template<class Key, class Value> std::ostream& operator<<( std::ostream& s, const std::map<Key,Value>& m )
    {
    s << "<";
    for( typename std::map<Key,Value>::const_iterator i=m.begin(); i!=m.end(); i++ )
	{
	if( i!=m.begin() )
	    s << " ";
	s << i->first << ":" << i->second;
	}
    s << ">";
    return( s );
    }


    template<class Type>
    struct deref_less : public std::binary_function<const Type*, const Type*, bool>
    {
	bool operator()(const Type* x, const Type* y) const { return *x < *y; }
    };


    template<class Type>
    struct deref_equal_to : public std::binary_function<const Type*, const Type*, bool>
    {	
	bool operator()(const Type* x, const Type* y) const { return *x == *y; }
    };


    template <class Type>
    void pointerIntoSortedList(list<Type*>& l, Type* e)
    {
	l.insert(lower_bound(l.begin(), l.end(), e, deref_less<Type>()), e);
    }


    template <class Type>
    void clearPointerList(list<Type*>& l)
    {
	for (typename list<Type*>::iterator i = l.begin(); i != l.end(); ++i)
	    delete *i;
	l.clear();
    }


    template<typename Map, typename Key, typename Value>
    typename Map::iterator mapInsertOrReplace(Map& m, const Key& k, const Value& v)
    {
	typename Map::iterator pos = m.lower_bound(k);
	if (pos != m.end() && !typename Map::key_compare()(k, pos->first))
	    pos->second = v;
	else
	    pos = m.insert(pos, typename Map::value_type(k, v));
	return pos;
    }


template <class T, unsigned int sz>
  inline unsigned int lengthof (T (&)[sz]) { return sz; }

}

#endif
