#ifndef STORAGE_H
#define STORAGE_H

#include <iostream>
#include <list>

#include "y2storage/Container.h"
#include "y2storage/FilterIterator.h"
#include "y2storage/DerefIterator.h"
#include "y2storage/ListListIterator.h"
#include "y2storage/IterPair.h"

using namespace std;

class Storage
    {
    typedef list<Container*> CCont;
    typedef CCont::iterator CIter;
    typedef CCont::const_iterator CCIter;

    public:
	struct SkipDeleted { bool operator()(const Container&d) const {return( !d.Delete());}};
	static SkipDeleted SkipDel;
	static bool NotDeleted( const Container&d ) { return( !d.Delete() ); };
	static bool IsRealDisk( const Container&d ) 
	    { return( d.Type()==Container::REAL_DISK ); };

	Storage( bool ronly=false, bool autodetect=true );
	virtual ~Storage();
	int AddDisk( const string& Name );

	template< class Pred > 
	class ConstContPIter : public FilterIterator< Pred, CCIter >
	    {
	    typedef FilterIterator< Pred, CCIter > _bclass;
	    public:
		ConstContPIter() : _bclass() {};
		ConstContPIter( const CCIter& b, const CCIter& e, const Pred& p,
		                bool atend=false ) : 
		    _bclass(b, e, p, atend ) {}
	    };
	template< class Pred > 
	class ConstContIter : public DerefIterator<ConstContPIter<Pred>,const Container>
	    {
	    typedef DerefIterator<ConstContPIter<Pred>,const Container> _bclass;
	    public:
		ConstContIter() : _bclass() {};
		ConstContIter( const _bclass& i ) : _bclass(i) {};
	    };

	template<class Pred> class ContCondIPair : 
	    public IterPair<ConstContIter<Pred> >
	    {
	    typedef IterPair<ConstContIter<Pred> > _bclass;
	    public:
		ContCondIPair( const ConstContIter<Pred>& b, 
		               const ConstContIter<Pred>& e ) : 
		    _bclass( b, e ) {}
	    };

    protected:
	template< class Pred > 
	class ContPIter : public FilterIterator< Pred, CIter > 
	    {
	    typedef FilterIterator< Pred, CIter > _bclass;
	    public:
		ContPIter() : _bclass() {};
		ContPIter( const CIter& b, const CIter& e, const Pred& p, 
		           bool atend=false ) : 
		    _bclass(b, e, p, atend ) {}
	    };
	template< class Pred > 
	class ContIter : public DerefIterator<ContPIter<Pred>,Container>
	    {
	    typedef DerefIterator<ContPIter<Pred>,Container> _bclass;
	    public:
		ContIter() : _bclass() {};
		ContIter( const _bclass& i ) : _bclass(i) {};
	    };


	class CheckFncCont
	    {
	    public:
		CheckFncCont( bool (* CheckFnc)( const Container& )=NULL ) : 
		    m_check(CheckFnc) {};
		bool operator()(const Container& d) const
		    { return(m_check==NULL || (*m_check)(d)); };
	    private:
		bool (* m_check)( const Container& d );
	    };

	class CheckFncVol
	    {
	    public:
		CheckFncVol( bool (* CheckFnc)( const Volume& )=NULL ) : 
		    m_check(CheckFnc) {};
		bool operator()(const Volume& d) const
		    { return(m_check==NULL || (*m_check)(d)); };
	    private:
		bool (* m_check)( const Volume& d );
	    };

    public:
	class ConstContIteratorP : public CheckFncCont, public ConstContPIter<CheckFncCont>
	    {
	    public: 
		ConstContIteratorP() {};
		ConstContIteratorP( const CCIter& b, const CCIter& e, 
				    bool (* CheckFnc)( const Container& )=NULL,
				    bool atend=false ) :
		    CheckFncCont( CheckFnc ),
		    ConstContPIter<CheckFncCont>( b, e, *this, atend ) {};
	    };
	typedef DerefIterator<ConstContIteratorP,const Container> ConstContIterator;
	typedef ListListIterator<Container::ConstPlainIterator, ConstContIterator> ConstVolPart;
	template< class Pred > 
	class ConstVolPIter : public FilterIterator< Pred, ConstVolPart >
	    {
	    typedef FilterIterator< Pred, ConstVolPart > _bclass;
	    public:
		ConstVolPIter() : _bclass() {};
		ConstVolPIter( const ConstVolPart& b, const ConstVolPart& e, 
		               const Pred& p, bool atend=false ) : 
		    _bclass(b, e, p, atend ) {}
		ConstVolPIter( const IterPair<ConstVolPart>& pair, 
		               const Pred& p, bool atend=false ) : 
		    _bclass(pair, p, atend ) {}
	    };
	template< class Pred >
	class ConstVolIter : public DerefIterator<ConstVolPIter<Pred>,const Volume>
	    {
	    typedef DerefIterator<ConstVolPIter<Pred>,const Volume> _bclass;
	    public:
		ConstVolIter() : _bclass() {};
		ConstVolIter( const _bclass& i ) : _bclass(i) {};
	    };

	template<class Pred> class VolCondIPair : public IterPair<ConstVolIter<Pred> >
	    {
	    typedef IterPair<ConstVolIter<Pred> > _bclass;
	    public:
		VolCondIPair( const ConstVolIter<Pred>& b, 
		              const ConstVolIter<Pred>& e ) : 
		    _bclass( b, e ) {}
	    };

	class ConstVolPIterator : public CheckFncVol, public ConstVolPIter<CheckFncVol>
	    {
	    public: 
		ConstVolPIterator() {};
		ConstVolPIterator( const ConstVolPart& b, const ConstVolPart& e, 
				   bool (* CheckFnc)( const Volume& )=NULL,
				   bool atend=false ) :
		    CheckFncVol( CheckFnc ),
		    ConstVolPIter<CheckFncVol>( b, e, *this, atend ) {};
		ConstVolPIterator( const IterPair<ConstVolPart>& p, 
				   bool (* CheckFnc)( const Volume& )=NULL,
				   bool atend=false ) :
		    CheckFncVol( CheckFnc ),
		    ConstVolPIter<CheckFncVol>( p, *this, atend ) {};
	    };
	typedef DerefIterator<ConstVolPIterator,const Volume> ConstVolIterator;

    protected:
	class ContPIterator : public CheckFncCont, public ContPIter<CheckFncCont>
	    {
	    public: 
		ContPIterator() {};
		ContPIterator( const CIter& b, const CIter& e, 
			       bool (* CheckFnc)( const Container& d )=NULL,
			       bool atend=false ) :
		    CheckFncCont( CheckFnc ),
		    ContPIter<CheckFncCont>( b, e, *this, atend ) {};
	    };
	typedef DerefIterator<ContPIterator,Container> ContIterator;

	typedef ListListIterator<Container::PlainIterator, ContIterator> VolPart;
	template< class Pred > 
	class VolIter : public FilterIterator< Pred, VolPart >
	    {
	    typedef FilterIterator< Pred, VolPart > _bclass;
	    public:
		VolIter() : _bclass() {};
		VolIter( const VolPart& b, const VolPart& e, const Pred& p,
			 bool atend=false ) : 
		    _bclass(b, e, p, atend ) {}
		VolIter( const IterPair<VolPart>& pair, const Pred& p,
			 bool atend=false ) : 
		    _bclass(pair, p, atend ) {}
	    };
	class VolIterator : public CheckFncVol, public VolIter<CheckFncVol>
	    {
	    public: 
		VolIterator() {};
		VolIterator( const VolPart& b, const VolPart& e, 
			     bool (* CheckFnc)( const Volume& )=NULL,
			     bool atend=false ) :
		    CheckFncVol( CheckFnc ),
		    VolIter<CheckFncVol>( b, e, *this, atend ) {};
		VolIterator( const IterPair<VolPart>& p, 
			     bool (* CheckFnc)( const Volume& )=NULL,
			     bool atend=false ) :
		    CheckFncVol( CheckFnc ),
		    VolIter<CheckFncVol>( p, *this, atend ) {};
	    };

    public:
	typedef IterPair<ConstContIterator> ConstContPair;
	ConstContPair ContPair( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContPair( ContBegin( CheckFnc ), ContEnd( CheckFnc ) ));
	    }
	ConstContIterator ContBegin( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContIterator( ConstContIteratorP( Disks.begin(), Disks.end(), CheckFnc )) );
	    }
	ConstContIterator ContEnd( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContIterator( ConstContIteratorP( Disks.begin(), Disks.end(), CheckFnc, true )) );
	    }

	typedef IterPair<ConstVolIterator> ConstVolPair;
	ConstVolPair VolPair( bool (* CheckCnt)( const Container& )) const
	    { 
	    return( ConstVolPair( VolBegin( CheckCnt ), VolEnd( CheckCnt ) ));
	    }
	ConstVolPair VolPair( bool (* CheckVol)( const Volume& )=NULL,
			      bool (* CheckCnt)( const Container& )=NULL) const
	    { 
	    return( ConstVolPair( VolBegin( CheckVol, CheckCnt ),
	                          VolEnd( CheckVol, CheckCnt ) ));
	    }
	ConstVolIterator VolBegin( bool (* CheckCnt)( const Container& )) const
	    { 
	    return( VolBegin( NULL, CheckCnt ) );
	    }
	ConstVolIterator VolBegin( bool (* CheckVol)( const Volume& )=NULL, 
	                           bool (* CheckCnt)( const Container& )=NULL) const
	    {
	    IterPair<ConstVolPart> p( *new ConstVolPart( ContPair( CheckCnt )),
	                              *new ConstVolPart( ContPair( CheckCnt ), true ));
	    return( ConstVolIterator( ConstVolPIterator(p, CheckVol )));
	    }
	ConstVolIterator VolEnd( bool (* CheckCnt)( const Container& )) const
	    { 
	    return( VolEnd( NULL, CheckCnt ) );
	    }
	ConstVolIterator VolEnd( bool (* CheckVol)( const Volume& )=NULL, 
	                         bool (* CheckCnt)( const Container& )=NULL) const
	    { 
	    IterPair<ConstVolPart> p( *new ConstVolPart( ContPair( CheckCnt )),
	                              *new ConstVolPart( ContPair( CheckCnt ), true ));
	    return( ConstVolIterator( ConstVolPIterator(p, CheckVol, true )));
	    }

	template< class Pred > ContCondIPair<Pred> ContCondPair( const Pred& p ) const
	    {
	    return( ContCondIPair<Pred>( ContCondBegin( p ), ContCondEnd( p ) ) );
	    }
	template< class Pred > ConstContIter<Pred> ContCondBegin( const Pred& p ) const
	    {
	    return( ConstContIter< Pred >( ConstContPIter<Pred>( Disks.begin(), Disks.end(), p )) );
	    }
	template< class Pred > ConstContIter<Pred> ContCondEnd( const Pred& p ) const
	    {
	    return( ConstContIter< Pred >( ConstContPIter<Pred>( Disks.begin(), Disks.end(), p, true )) );
	    }
	template< class Pred > VolCondIPair<Pred> VolCondPair( const Pred& p ) const
	    {
	    return( VolCondIPair<Pred>( VolCondBegin( p ), VolCondEnd( p ) ) );
	    }
	template< class Pred > ConstVolIter<Pred> VolCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstVolPart> pair( *new ConstVolPart( ContPair()),
	                                 *new ConstVolPart( ContPair(), true ));
	    return( ConstVolIter< Pred >( ConstVolPIter<Pred>(pair, p) ) );
	    }
	template< class Pred > ConstVolIter<Pred> VolCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstVolPart> pair( *new ConstVolPart( ContPair()),
	                                 *new ConstVolPart( ContPair(), true ));
	    return( ConstVolIter< Pred >( ConstVolPIter<Pred>(pair, p, true )) );
	    }

    protected:
	typedef IterPair<ContIterator> CIPair;
	CIPair CPair( bool (* CheckFnc)( const Container& )=NULL )
	    {
	    return( CIPair( CBegin(CheckFnc), CEnd(CheckFnc) ));
	    }
	ContIterator CBegin( bool (* CheckFnc)( const Container& )=NULL )
	    { 
	    return( ContIterator( ContPIterator( Disks.begin(), Disks.end(), CheckFnc )) );
	    }
	ContIterator CEnd( bool (* CheckFnc)( const Container& )=NULL )
	    { 
	    return( ContIterator( ContPIterator( Disks.begin(), Disks.end(), CheckFnc, true )) );
	    }

	typedef IterPair<VolIterator> VIPair;
	VIPair VPair( bool (* CheckCnt)( const Container& )) 
	    { 
	    return( VIPair( VBegin( CheckCnt ), VEnd( CheckCnt ) ));
	    }
	VIPair VPair( bool (* CheckVol)( const Volume& )=NULL,
		      bool (* CheckCnt)( const Container& )=NULL) 
	    { 
	    return( VIPair( VBegin( CheckVol, CheckCnt ),
			    VEnd( CheckVol, CheckCnt ) ));
	    }
	VolIterator VBegin( bool (* CheckCnt)( const Container& )) 
	    { 
	    return( VBegin( NULL, CheckCnt ) );
	    }
	VolIterator VBegin( bool (* CheckVol)( const Volume& )=NULL, 
			    bool (* CheckCnt)( const Container& )=NULL) 
	    {
	    IterPair<VolPart> p( *new VolPart( CPair( CheckCnt )),
				 *new VolPart( CPair( CheckCnt ), true ));
	    return( VolIterator( p, CheckVol ));
	    }
	VolIterator VEnd( bool (* CheckCnt)( const Container& ))
	    { 
	    return( VEnd( NULL, CheckCnt ) );
	    }
	VolIterator VEnd( bool (* CheckVol)( const Volume& )=NULL, 
			  bool (* CheckCnt)( const Container& )=NULL)
	    { 
	    IterPair<VolPart> p( *new VolPart( CPair( CheckCnt )),
				 *new VolPart( CPair( CheckCnt ), true ));
	    return( VolIterator( p, CheckVol, true ));
	    }

	void AutodetectDisks();
	bool readonly;
	CCont Disks;
    };

Storage::SkipDeleted Storage::SkipDel;


#endif
