#ifndef CONTAINER_H
#define CONTAINER_H

#include <list>

using namespace std;

#include "y2storage/Volume.h"
#include "y2storage/FilterIterator.h"
#include "y2storage/IterPair.h"

template<typename L1, typename L2> class ListListIterator;

class Container
    {
    friend class Storage;
    template<typename L1, typename L2> friend class ListListIterator;

    typedef list<Volume*> VCont;
    typedef VCont::iterator VIter;
    typedef VCont::const_iterator CVIter;

    public:
	struct SkipDeleted 
	    { 
	    bool operator()(const Volume&d) const { return( !d.Delete());}
	    };
	static SkipDeleted SkipDel;
	static bool NotDeleted( const Volume&d ) { return( !d.Delete() ); }
	enum CType { UNKNOWN, REAL_DISK, MD, LOOP, LVM, EVMS };

	template< class Pred >
	class ConstVolIter : public FilterIterator< Pred, CVIter >
	    {
	    typedef FilterIterator< Pred, CVIter > _bclass;
	    public:
		ConstVolIter( ) {}
		const Volume& operator*() const { return( *_bclass::operator*() ); }
		const Volume* operator->() const { return( _bclass::operator*() ); }
		ConstVolIter( const CVIter& b, const CVIter& e, const Pred& p,
		              bool atend=false ) :
		    _bclass(b, e, p, atend ) {}
	    };

	template< class Pred >
	class VolIter : public FilterIterator< Pred, VIter > 
	    {
	    typedef FilterIterator< Pred, VIter > _bclass;
	    public:
		VolIter() {}
		Volume& operator*() const { return( *_bclass::operator*() ); }
		Volume* operator->() const { return( _bclass::operator*() ); }
		VolIter( const VIter& b, const VIter& e, const Pred& p,
			 bool atend=false ) :
		    _bclass(b, e, p, atend ) {}
	    };

	class CheckFncVol
	    {
	    public:
		CheckFncVol( bool (* CheckFnc)( const Volume& )=NULL ) :
		    m_check(CheckFnc) {}
		bool operator()(const Volume& d) const
		    { return(m_check==NULL || (*m_check)(d)); }
	    private:
		bool (* m_check)( const Volume& d );
	    };

	class ConstVolIterator : public CheckFncVol, public ConstVolIter<CheckFncVol>
	    {
	    public:
		ConstVolIterator() {}
		ConstVolIterator( const CVIter& b, const CVIter& e,
				  bool (* CheckFnc)( const Volume& )=NULL,
				  bool atend=false ) :
		    CheckFncVol( CheckFnc ),
		    ConstVolIter<CheckFncVol>( b, e, *this, atend ) {}
	    };

	template<class Pred> class VolCondIPair :
	    public IterPair<ConstVolIter<Pred> >
	    {
	    typedef IterPair<ConstVolIter<Pred> > _bclass;
	    public:
		VolCondIPair( const ConstVolIter<Pred>& b,
			       const ConstVolIter<Pred>& e ) :
		    _bclass( b, e ) {}
	    };

	class VolIterator : public CheckFncVol, public VolIter<CheckFncVol>
	    {
	    public:
		VolIterator() {}
		VolIterator( const VIter& b, const VIter& e,
				   bool (* CheckFnc)( const Volume& )=NULL,
				   bool atend=false ) :
		    CheckFncVol( CheckFnc ),
		    VolIter<CheckFncVol>( b, e, *this, atend ) {}
	    };

	typedef IterPair<ConstVolIterator> ConstVolPair;
	ConstVolPair VolPair( bool (* CheckFnc)( const Volume& )=NULL ) const
	    { 
	    return( ConstVolPair( VolBegin( CheckFnc ), VolEnd( CheckFnc ) ));
	    }
	ConstVolIterator VolBegin( bool (* CheckFnc)( const Volume& )=NULL ) const
	    {
	    return( ConstVolIterator( Vols.begin(), Vols.end(), CheckFnc ) );
	    }
	ConstVolIterator VolEnd( bool (* CheckFnc)( const Volume& )=NULL ) const
	    {
	    return( ConstVolIterator( Vols.begin(), Vols.end(), CheckFnc, true ) );
	    }

	template< class Pred > VolCondIPair<Pred> VolCondPair( const Pred& p ) const
	    {
	    return( VolCondIPair<Pred>( VolCondBegin( p ), VolCondEnd( p ) ) );
	    }
	template< class Pred > ConstVolIter<Pred> VolCondBegin( const Pred& p ) const
	    {
	    return( ConstVolIter< Pred >( Vols.begin(), Vols.end(), p ) );
	    }
	template< class Pred > ConstVolIter<Pred> VolCondEnd( const Pred& p ) const
	    {
	    return( ConstVolIter< Pred >( Vols.begin(), Vols.end(), p, true ) );
	    }

	Container( const string& Name );
	virtual ~Container();
	const string& Name() const { return name; }
	const string& Device() const { return device; }
	CType Type() const { return type; }
	bool Delete() const { return deleted; }

    protected:
	typedef CVIter ConstPlainIterator;
	ConstPlainIterator begin() const { return Vols.begin(); }
	ConstPlainIterator end() const { return Vols.end(); }

	typedef VIter PlainIterator;
	PlainIterator begin() { return Vols.begin(); }
	PlainIterator end() { return Vols.end(); }

	typedef IterPair<VolIterator> VolIPair;
	VolIPair VolPair( bool (* CheckFnc)( const Volume& )=NULL )
	    { 
	    return( VolIPair( VBegin( CheckFnc ), VEnd( CheckFnc ) ));
	    }
	VolIterator VBegin( bool (* CheckFnc)( const Volume& )=NULL ) 
	    { 
	    return( VolIterator( Vols.begin(), Vols.end(), CheckFnc ) );
	    }
	VolIterator VEnd( bool (* CheckFnc)( const Volume& )=NULL ) 
	    { 
	    return( VolIterator( Vols.begin(), Vols.end(), CheckFnc, true ) );
	    }
	CType type;
	string name;
	string device;
	bool deleted;
	VCont Vols;
    };

#endif
