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

template <int Value>
class CheckType 
    {
    public:
	bool operator()( const Container& d ) const
	    {
	    return( d.Type()==Value );
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
	    cerr << "Expensive ++ CastCheckIterator" << endl;
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
	    cerr << "Expensive -- CastCheckIterator" << endl;
	    CastCheckIterator tmp(*this);
	    _bclass::operator--(); 
	    return(tmp); 
	    }
    };

template< class Iter, int Value>
class CheckIterator : public CheckType<Value>, 
                      public FilterIterator< CheckType<Value>, Iter >
    {
    typedef FilterIterator<CheckType<Value>, Iter> _bclass;
    public:
	CheckIterator() : _bclass() {}
	CheckIterator( const Iter& b, const Iter& e, bool atend=false) : 
	    _bclass( b, e, *this, atend ) {}
	CheckIterator( const IterPair<Iter>& pair, bool atend=false) : 
	    _bclass( pair, *this, atend ) {}
	CheckIterator& operator++() 
	    { 
	    _bclass::operator++(); return(*this); 
	    }
	CheckIterator operator++(int) 
	    { 
	    cerr << "Expensive ++ CheckIterator" << endl;
	    CheckIterator tmp(*this);
	    _bclass::operator++(); 
	    return(tmp); 
	    }
	CheckIterator& operator--() 
	    { 
	    _bclass::operator--(); return(*this); 
	    }
	CheckIterator operator--(int) 
	    { 
	    cerr << "Expensive -- CastCheckIterator" << endl;
	    CheckIterator tmp(*this);
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
	    cerr << "Expensive ++ CastIterator" << endl;
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
	    cerr << "Expensive -- CastIterator" << endl;
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

#endif
