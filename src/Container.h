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
	bool operator== ( const Container& rhs ) const
	    { return( type == rhs.type && name == rhs.name && deleted == rhs.deleted ); }
	bool operator!= ( const Container& rhs ) const
	    { return( !(*this==rhs) ); }
	bool operator< ( const Container& rhs ) const
	    { 
	    if( type != rhs.type )
		return( type<rhs.type );
	    else if( name != rhs.name )
		return( name<rhs.name );
	    else 
		return( !deleted );
	    }
	bool operator<= ( const Container& rhs ) const
	    { return( *this<rhs || *this==rhs ); }
	bool operator>= ( const Container& rhs ) const
	    { return( !(*this<rhs) ); }
	bool operator> ( const Container& rhs ) const
	    { return( !(*this<=rhs) ); }

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
	    return( ConstVolIterator( ConstVolPIterator(vols.begin(), vols.end(), CheckFnc )) );
	    }
	ConstVolIterator VolEnd( bool (* CheckFnc)( const Volume& )=NULL ) const
	    {
	    return( ConstVolIterator( ConstVolPIterator(vols.begin(), vols.end(), CheckFnc, true )) );
	    }

	template< class Pred > typename VolCondIPair<Pred>::type VolCondPair( const Pred& p ) const
	    {
	    return( VolCondIPair<Pred>::type( VolCondBegin( p ), VolCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstVolumeI<Pred>::type VolCondBegin( const Pred& p ) const
	    {
	    return( ConstVolumeI<Pred>::type( vols.begin(), vols.end(), p ) );
	    }
	template< class Pred > typename ConstVolumeI<Pred>::type VolCondEnd( const Pred& p ) const
	    {
	    return( ConstVolumeI<Pred>::type( vols.begin(), vols.end(), p, true ) );
	    }

    protected:
	// protected member functions for iterators over volumes
	VolIPair VolPair( bool (* CheckFnc)( const Volume& )=NULL )
	    { 
	    return( VolIPair( VBegin( CheckFnc ), VEnd( CheckFnc ) ));
	    }
	VolIterator VBegin( bool (* CheckFnc)( const Volume& )=NULL ) 
	    { 
	    return( VolIterator( VolPIterator(vols.begin(), vols.end(), CheckFnc )) );
	    }
	VolIterator VEnd( bool (* CheckFnc)( const Volume& )=NULL ) 
	    { 
	    return( VolIterator( VolPIterator(vols.begin(), vols.end(), CheckFnc, true )) );
	    }

    public:
	Container( const string& Name, CType typ );
	virtual ~Container();
	const string& Name() const { return name; }
	const string& Device() const { return device; }
	CType Type() const { return type; }
	bool Delete() const { return deleted; }
	static CType const StaticType() { return UNKNOWN; } 
	friend ostream& operator<< (ostream& s, const Container &c );

    protected:
	typedef CVIter ConstPlainIterator;
	ConstPlainIterator begin() const { return vols.begin(); }
	ConstPlainIterator end() const { return vols.end(); }

	typedef VIter PlainIterator;
	PlainIterator begin() { return vols.begin(); }
	PlainIterator end() { return vols.end(); }
	void print( ostream& s ) const { s << *this; }

	static string type_names[EVMS+1];

	CType type;
	string name;
	string device;
	bool deleted;
	VCont vols;

    };

inline ostream& operator<< (ostream& s, const Container &c )
    {
    s << "Type:" << Container::type_names[c.type] 
      << " Name:" << c.name 
      << " Device:" << c.device 
      << " Del:" << c.deleted 
      << " Vcnt:" << c.vols.size(); 
    return( s );
    }

#endif
