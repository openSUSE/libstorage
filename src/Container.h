#ifndef CONTAINER_H
#define CONTAINER_H

#include <list>

using namespace std;

#include "y2storage/Volume.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/FilterIterator.h"
#include "y2storage/DerefIterator.h"
#include "y2storage/IterPair.h"

template<typename L1, typename L2> class ListListIterator;

class Container
    {
    friend class Storage;
    protected:
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
	typedef enum { UNKNOWN, DISK, MD, LOOP, LVM, EVMS } CType;

// iterators over volumes of a container
    protected:
	// protected typedefs for iterators over volumes
	template< class Pred >
	    struct ConstVolumePI { typedef ContainerIter<Pred, CVIter> type; };
	template< class Pred >
	    struct VolumePI { typedef ContainerIter<Pred, VIter> type; };
	template< class Pred >
	    struct VolumeI { typedef ContainerDerIter<Pred, typename VolumePI<Pred>::type, Volume> type; };
	typedef CheckFnc<const Volume> CheckFncVol;
	typedef CheckerIterator< CheckFncVol, ConstVolumePI<CheckFncVol>::type, 
	                         CVIter, Volume> ConstVolPIterator;
	typedef CheckerIterator< CheckFncVol, VolumePI<CheckFncVol>::type, 
	                         VIter, Volume> VolPIterator;
	typedef DerefIterator<VolPIterator,Volume> VolIterator;
	typedef IterPair<VolIterator> VolIPair;

    public:
	// public typedefs for iterators over volumes
	template< class Pred >
	    struct ConstVolumeI { typedef ContainerDerIter<Pred, typename ConstVolumePI<Pred>::type, const Volume> type; };
	template< class Pred >
	    struct VolCondIPair { typedef MakeCondIterPair<Pred, typename ConstVolumeI<Pred>::type> type;};
	typedef DerefIterator<ConstVolPIterator,const Volume> ConstVolIterator;
	typedef IterPair<ConstVolIterator> ConstVolPair;

	// public member functions for iterators over volumes
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

	template< class Pred > typename VolCondIPair<Pred>::type VolCondPair( const Pred& p ) const
	    {
	    return( VolCondIPair<Pred>::type( VolCondBegin( p ), VolCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstVolumeI<Pred>::type VolCondBegin( const Pred& p ) const
	    {
	    return( ConstVolumeI<Pred>::type( Vols.begin(), Vols.end(), p ) );
	    }
	template< class Pred > typename ConstVolumeI<Pred>::type VolCondEnd( const Pred& p ) const
	    {
	    return( ConstVolumeI<Pred>::type( Vols.begin(), Vols.end(), p, true ) );
	    }

    protected:
	// protected member functions for iterators over volumes
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

    public:
	Container( const string& Name, CType typ );
	virtual ~Container();
	const string& Name() const { return name; }
	const string& Device() const { return device; }
	CType Type() const { return type; }
	bool Delete() const { return deleted; }
	static CType const StaticType() { return UNKNOWN; } 

    protected:
	typedef CVIter ConstPlainIterator;
	ConstPlainIterator begin() const { return Vols.begin(); }
	ConstPlainIterator end() const { return Vols.end(); }

	typedef VIter PlainIterator;
	PlainIterator begin() { return Vols.begin(); }
	PlainIterator end() { return Vols.end(); }

	CType type;
	string name;
	string device;
	bool deleted;
	VCont Vols;
    };

#endif
