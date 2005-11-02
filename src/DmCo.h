#ifndef DM_CO_H
#define DM_CO_H

#include "y2storage/PeContainer.h"
#include "y2storage/Dm.h"

namespace storage
{

class DmCo : public PeContainer
    {
    friend class Storage;

    public:
	DmCo( Storage * const s, bool detect );
	DmCo( const DmCo& rhs );
	virtual ~DmCo();
	static storage::CType const staticType() { return storage::DM; }
	friend std::ostream& operator<< (std::ostream&, const DmCo& );
	bool equalContent( const DmCo& rhs ) const;
	void logDifference( const DmCo& d ) const;

	int removeDm( const string& table );
	int removeVolume( Volume* v );
	
    protected:
	// iterators over DM volumes
	// protected typedefs for iterators over DM volumes
	typedef CastIterator<VIter, Dm *> DmInter;
	typedef CastIterator<CVIter, const Dm *> DmCInter;
	template< class Pred >
	    struct DmPI { typedef ContainerIter<Pred, DmInter> type; };
	template< class Pred >
	    struct DmCPI { typedef ContainerIter<Pred, DmCInter> type; };
	typedef CheckFnc<const Dm> CheckFncDm;
	typedef CheckerIterator< CheckFncDm, DmPI<CheckFncDm>::type,
				 DmInter, Dm > DmPIterator;
	typedef CheckerIterator< CheckFncDm, DmCPI<CheckFncDm>::type,
				 DmCInter, const Dm > DmCPIterator;
	typedef DerefIterator<DmPIterator,Dm> DmIter;
	typedef DerefIterator<DmCPIterator,const Dm> ConstDmIter;
	typedef IterPair<DmIter> DmPair;
	typedef IterPair<ConstDmIter> ConstDmPair;

	DmPair dmPair( bool (* Check)( const Dm& )=NULL)
	    {
	    return( DmPair( dmBegin( Check ), dmEnd( Check ) ));
	    }
	DmIter dmBegin( bool (* Check)( const Dm& )=NULL)
	    {
	    IterPair<DmInter> p( (DmInter(begin())), (DmInter(end())) );
	    return( DmIter( DmPIterator( p, Check )) );
	    }
	DmIter dmEnd( bool (* Check)( const Dm& )=NULL)
	    {
	    IterPair<DmInter> p( (DmInter(begin())), (DmInter(end())) );
	    return( DmIter( DmPIterator( p, Check, true )) );
	    }

	ConstDmPair dmPair( bool (* Check)( const Dm& )=NULL) const
	    {
	    return( ConstDmPair( dmBegin( Check ), dmEnd( Check ) ));
	    }
	ConstDmIter dmBegin( bool (* Check)( const Dm& )=NULL) const
	    {
	    IterPair<DmCInter> p( (DmCInter(begin())), (DmCInter(end())) );
	    return( ConstDmIter( DmCPIterator( p, Check )) );
	    }
	ConstDmIter dmEnd( bool (* Check)( const Dm& )=NULL) const
	    {
	    IterPair<DmCInter> p( (DmCInter(begin())), (DmCInter(end())) );
	    return( ConstDmIter( DmCPIterator( p, Check, true )) );
	    }

	DmCo( Storage * const s, const string& File );

	void getDmData();
	void getDmData( unsigned num );
	bool findDm( unsigned num, DmIter& i );
	bool findDm( unsigned num ); 
	bool findDm( const string& dev, DmIter& i );
	bool findDm( const string& dev ); 
	void addDm( Dm* m );
	void checkDm( Dm* m );
	void updateEntry( const Dm* m );

	void init();

	virtual void print( std::ostream& s ) const { s << *this; }

	int doRemove( Volume* v );

	void logData( const string& Dir );
    };

}

#endif
