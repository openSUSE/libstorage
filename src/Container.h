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
	Container operator=( const Container& );
	Container( const Container& );


    public:
	typedef enum { UNKNOWN, DISK, MD, LOOP, LVM, EVMS } CType;
	typedef enum { DECREASE, INCREASE, FORMAT, MOUNT } CommitStage;
	bool operator== ( const Container& rhs ) const
	    { return( typ == rhs.typ && nm == rhs.nm && dltd == rhs.dltd ); }
	bool operator!= ( const Container& rhs ) const
	    { return( !(*this==rhs) ); }
	bool operator< ( const Container& rhs ) const
	    { 
	    if( typ != rhs.typ )
		return( typ<rhs.typ );
	    else if( nm != rhs.nm )
		return( nm<rhs.nm );
	    else 
		return( !dltd );
	    }
	bool operator<= ( const Container& rhs ) const
	    { return( *this<rhs || *this==rhs ); }
	bool operator>= ( const Container& rhs ) const
	    { return( !(*this<rhs) ); }
	bool operator> ( const Container& rhs ) const
	    { return( !(*this<=rhs) ); }

	virtual int commitChanges( CommitStage stage );

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
	typedef IterPair<VolIterator> VolPair;

    public:
	// public typedefs for iterators over volumes
	template< class Pred >
	    struct ConstVolumeI { typedef ContainerDerIter<Pred, typename ConstVolumePI<Pred>::type, const Volume> type; };
	template< class Pred >
	    struct VolCondIPair { typedef MakeCondIterPair<Pred, typename ConstVolumeI<Pred>::type> type;};
	typedef DerefIterator<ConstVolPIterator,const Volume> ConstVolIterator;
	typedef IterPair<ConstVolIterator> ConstVolPair;

	// public member functions for iterators over volumes
	ConstVolPair volPair( bool (* CheckFnc)( const Volume& )=NULL ) const
	    { 
	    return( ConstVolPair( volBegin( CheckFnc ), volEnd( CheckFnc ) ));
	    }
	ConstVolIterator volBegin( bool (* CheckFnc)( const Volume& )=NULL ) const
	    {
	    return( ConstVolIterator( ConstVolPIterator(vols.begin(), vols.end(), CheckFnc )) );
	    }
	ConstVolIterator volEnd( bool (* CheckFnc)( const Volume& )=NULL ) const
	    {
	    return( ConstVolIterator( ConstVolPIterator(vols.begin(), vols.end(), CheckFnc, true )) );
	    }

	template< class Pred > typename VolCondIPair<Pred>::type volCondPair( const Pred& p ) const
	    {
	    return( VolCondIPair<Pred>::type( volCondBegin( p ), volCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstVolumeI<Pred>::type volCondBegin( const Pred& p ) const
	    {
	    return( ConstVolumeI<Pred>::type( vols.begin(), vols.end(), p ) );
	    }
	template< class Pred > typename ConstVolumeI<Pred>::type volCondEnd( const Pred& p ) const
	    {
	    return( ConstVolumeI<Pred>::type( vols.begin(), vols.end(), p, true ) );
	    }

    protected:
	// protected member functions for iterators over volumes
	VolPair volPair( bool (* CheckFnc)( const Volume& )=NULL )
	    { 
	    return( VolPair( vBegin( CheckFnc ), vEnd( CheckFnc ) ));
	    }
	VolIterator vBegin( bool (* CheckFnc)( const Volume& )=NULL ) 
	    { 
	    return( VolIterator( VolPIterator(vols.begin(), vols.end(), CheckFnc )) );
	    }
	VolIterator vEnd( bool (* CheckFnc)( const Volume& )=NULL ) 
	    { 
	    return( VolIterator( VolPIterator(vols.begin(), vols.end(), CheckFnc, true )) );
	    }

    public:
	Container( const Storage * const, const string& Name, CType typ );
	virtual ~Container();
	const string& name() const { return nm; }
	const string& device() const { return dev; }
	CType type() const { return typ; }
	bool deleted() const { return dltd; }
	void setDeleted( bool val=true ) { dltd=val; }
	bool readonly() const { return rdonly; }
	static CType const staticType() { return UNKNOWN; } 
	friend ostream& operator<< (ostream& s, const Container &c );

    protected:
	typedef CVIter ConstPlainIterator;
	ConstPlainIterator begin() const { return vols.begin(); }
	ConstPlainIterator end() const { return vols.end(); }

	typedef VIter PlainIterator;
	PlainIterator begin() { return vols.begin(); }
	PlainIterator end() { return vols.end(); }
	void print( ostream& s ) const { s << *this; }
	void addToList( Volume* e )
	{ pointerIntoSortedList<Volume>( vols, e ); }
	virtual int doCreate( Volume * v ); 
	virtual int doRemove( Volume * v ); 


	static string type_names[EVMS+1];

	const Storage * const sto;
	CType typ;
	string nm;
	string dev;
	bool dltd;
	bool rdonly;
	VCont vols;

    };

inline ostream& operator<< (ostream& s, const Container &c )
{
    s << "Type:" << Container::type_names[c.typ] 
	<< " Name:" << c.nm 
	<< " Device:" << c.dev 
	<< " Vcnt:" << c.vols.size(); 
    if( c.dltd )
	s << " deleted";
    if( c.rdonly )
      s << " readonly";
    return( s );
    }

#endif
