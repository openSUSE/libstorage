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

template< class Value >
class CheckFncType
    {
    public:
        CheckFncType( bool (* ChkFnc)( Value& )=NULL ) : m_check(ChkFnc) {}
	bool operator()(const Container& d) const
	    { return( d.Type()==Value::StaticType() &&
	              (m_check==NULL || (*m_check)(d))); }
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
    };

template< class Pred, class Iter, class Value >
class ContainerDerIter : public DerefIterator<Iter,Value>
    {
    typedef DerefIterator<Iter,Value> _bclass;
    public:
	ContainerDerIter() : _bclass() {}
	ContainerDerIter( const _bclass& i ) : _bclass(i) {}
    };

template <class Value>
class CheckType 
    {
    public:
	bool operator()( const Container& d ) const
	    {
	    return( d.Type()==Value::StaticType() );
	    }
    };

template< class Iter, class Value, class CastResult >
class CastIterator : public CheckType<Value>, 
                     public FilterIterator< CheckType<Value>, Iter >
    {
    typedef FilterIterator<CheckType<Value>, Iter> _bclass;
    public:
	typedef CastResult value_type;
	typedef CastResult& reference;
	typedef CastResult* pointer;

	CastIterator() : _bclass() {};
	CastIterator( const Iter& b, const Iter& e, bool atend=false) : 
	    _bclass( b, e, *this, atend ) {};
	CastIterator( const IterPair<Iter>& pair, bool atend=false) : 
	    _bclass( pair, *this, atend ) {};
	CastResult operator*() const
	    {
	    return( static_cast<CastResult>(_bclass::operator*()) );
	    }
	CastResult* operator->() const
	    {
	    return( static_cast<CastResult*>(_bclass::operator->()) );
	    }
	CastIterator& operator++() 
	    { 
	    _bclass::operator++(); return(*this); 
	    }
	CastIterator operator++(int) 
	    { 
	    cerr << "Expensive ++ CastIterator" << endl;
	    CastIterator tmp(*this);
	    _bclass::operator++(); 
	    return(tmp); 
	    }
	CastIterator& operator--() 
	    { 
	    _bclass::operator--(); return(*this); 
	    }
	CastIterator operator--(int) 
	    { 
	    cerr << "Expensive -- CastIterator" << endl;
	    CastIterator tmp(*this);
	    _bclass::operator--(); 
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
    };

#endif
