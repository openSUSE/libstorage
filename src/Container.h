#ifndef CONTAINER_H
#define CONTAINER_H

#include <list>

using namespace std;

#include "y2storage/Volume.h"
#include "y2storage/FilterIterator.h"
#include "y2storage/DerefIterator.h"
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
	class ConstVolPIter : public FilterIterator< Pred, CVIter >
	    {
	    typedef FilterIterator< Pred, CVIter > _bclass;
	    public:
		ConstVolPIter( ) {}
		ConstVolPIter( const CVIter& b, const CVIter& e, const Pred& p,
		               bool atend=false ) :
		    _bclass(b, e, p, atend ) {}
	    };
	template< class Pred > 
	class ConstVolIter : public DerefIterator<ConstVolPIter<Pred>,const Volume>
	    {
	    typedef DerefIterator<ConstVolPIter<Pred>,const Volume> _bclass;
	    public:
		ConstVolIter() : _bclass() {};
		ConstVolIter( const _bclass& i ) : _bclass(i) {};
	    };


	template< class Pred >
	class VolPIter : public FilterIterator< Pred, VIter > 
	    {
	    typedef FilterIterator< Pred, VIter > _bclass;
	    public:
		VolPIter() {}
		VolPIter( const VIter& b, const VIter& e, const Pred& p,
			  bool atend=false ) :
		    _bclass(b, e, p, atend ) {}
	    };
	template< class Pred > 
	class VolIter : public DerefIterator<VolPIter<Pred>,Volume>
	    {
	    typedef DerefIterator<VolPIter<Pred>,Volume> _bclass;
	    public:
		VolIter() : _bclass() {};
		VolIter( const _bclass& i ) : _bclass(i) {};
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

	class ConstVolPIterator : public CheckFncVol, public ConstVolPIter<CheckFncVol>
	    {
	    public:
		ConstVolPIterator() {}
		ConstVolPIterator( const CVIter& b, const CVIter& e,
				  bool (* CheckFnc)( const Volume& )=NULL,
				  bool atend=false ) :
		    CheckFncVol( CheckFnc ),
		    ConstVolPIter<CheckFncVol>( b, e, *this, atend ) {}
	    };
	typedef DerefIterator<ConstVolPIterator,const Volume> ConstVolIterator;

	template<class Pred> class VolCondIPair :
	    public IterPair<ConstVolIter<Pred> >
	    {
	    typedef IterPair<ConstVolIter<Pred> > _bclass;
	    public:
		VolCondIPair( const ConstVolIter<Pred>& b,
			       const ConstVolIter<Pred>& e ) :
		    _bclass( b, e ) {}
	    };

	class VolPIterator : public CheckFncVol, public VolPIter<CheckFncVol>
	    {
	    public:
		VolPIterator() {}
		VolPIterator( const VIter& b, const VIter& e,
			      bool (* CheckFnc)( const Volume& )=NULL,
			      bool atend=false ) :
		    CheckFncVol( CheckFnc ),
		    VolPIter<CheckFncVol>( b, e, *this, atend ) {}
	    };
	typedef DerefIterator<VolPIterator,Volume> VolIterator;

	typedef IterPair<ConstVolIterator> ConstVolPair;
	ConstVolPair VolPair( bool (* CheckFnc)( const Volume& )=NULL ) const
	    { 
	    return( ConstVolPair( VolBegin( CheckFnc ), VolEnd( CheckFnc ) ));
	    }
	ConstVolIterator VolBegin( bool (* CheckFnc)( const Volume& )=NULL ) const
	    {
	    return( ConstVolIterator( ConstVolPIterator(Vols.begin(), Vols.end(), CheckFnc )) );
	    }
	ConstVolIterator VolEnd( bool (* CheckFnc)( const Volume& )=NULL ) const
	    {
	    return( ConstVolIterator( ConstVolPIterator(Vols.begin(), Vols.end(), CheckFnc, true )) );
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
	    return( VolIterator( VolPIterator(Vols.begin(), Vols.end(), CheckFnc )) );
	    }
	VolIterator VEnd( bool (* CheckFnc)( const Volume& )=NULL ) 
	    { 
	    return( VolIterator( VolPIterator(Vols.begin(), Vols.end(), CheckFnc, true )) );
	    }
	CType type;
	string name;
	string device;
	bool deleted;
	VCont Vols;
    };

#endif
