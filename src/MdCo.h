#ifndef MD_CO_H
#define MD_CO_H

#include "y2storage/Container.h"
#include "y2storage/Md.h"

class EtcRaidtab;

class MdCo : public Container
    {
    friend class Storage;

    public:
	MdCo( Storage * const s, bool detect );
	virtual ~MdCo();
	static CType const staticType() { return MD; }
	friend inline ostream& operator<< (ostream&, const MdCo& );

	int createMd( unsigned num, storage::MdType type, 
	              const std::list<string>& devs );
	int removeMd( unsigned num, bool destroySb=true );
	unsigned unusedNumber();
	void syncRaidtab();


	static void activate( bool val=true );
	int removeVolume( Volume* v );
	
    protected:
	// iterators over MD volumes
	// protected typedefs for iterators over MD volumes
	typedef CastIterator<VIter, Md *> MdInter;
	typedef CastIterator<CVIter, const Md *> MdCInter;
	template< class Pred >
	    struct MdPI { typedef ContainerIter<Pred, MdInter> type; };
	template< class Pred >
	    struct MdCPI { typedef ContainerIter<Pred, MdCInter> type; };
	typedef CheckFnc<const Md> CheckFncMd;
	typedef CheckerIterator< CheckFncMd, MdPI<CheckFncMd>::type,
				 MdInter, Md > MdPIterator;
	typedef CheckerIterator< CheckFncMd, MdCPI<CheckFncMd>::type,
				 MdCInter, const Md > MdCPIterator;
	typedef DerefIterator<MdPIterator,Md> MdIter;
	typedef DerefIterator<MdCPIterator,const Md> ConstMdIter;
	typedef IterPair<MdIter> MdPair;
	typedef IterPair<ConstMdIter> ConstMdPair;

	MdPair mdPair( bool (* Check)( const Md& )=NULL)
	    {
	    return( MdPair( mdBegin( Check ), mdEnd( Check ) ));
	    }
	MdIter mdBegin( bool (* Check)( const Md& )=NULL)
	    {
	    IterPair<MdInter> p( (MdInter(begin())), (MdInter(end())) );
	    return( MdIter( MdPIterator( p, Check )) );
	    }
	MdIter mdEnd( bool (* Check)( const Md& )=NULL)
	    {
	    IterPair<MdInter> p( (MdInter(begin())), (MdInter(end())) );
	    return( MdIter( MdPIterator( p, Check, true )) );
	    }

	ConstMdPair mdPair( bool (* Check)( const Md& )=NULL) const
	    {
	    return( ConstMdPair( mdBegin( Check ), mdEnd( Check ) ));
	    }
	ConstMdIter mdBegin( bool (* Check)( const Md& )=NULL) const
	    {
	    IterPair<MdCInter> p( (MdCInter(begin())), (MdCInter(end())) );
	    return( ConstMdIter( MdCPIterator( p, Check )) );
	    }
	ConstMdIter mdEnd( bool (* Check)( const Md& )=NULL) const
	    {
	    IterPair<MdCInter> p( (MdCInter(begin())), (MdCInter(end())) );
	    return( ConstMdIter( MdCPIterator( p, Check, true )) );
	    }

	MdCo( Storage * const s, const string& File );

	void getMdData();
	void getMdData( unsigned num );
	bool findMd( unsigned num, MdIter& i );
	bool findMd( unsigned num ); 
	bool findMd( const string& dev, MdIter& i );
	bool findMd( const string& dev ); 
	void addMd( Md* m );
	void checkMd( Md* m );
	void updateEntry( const Md* m );

	void init();

	virtual void print( ostream& s ) const { s << *this; }

	int doCreate( Volume* v );
	int doRemove( Volume* v );

	void logData( const string& Dir );

	EtcRaidtab *tab;

	static bool md_active;
    };

inline ostream& operator<< (ostream& s, const MdCo& d )
    {
    s << *((Container*)&d);
    return( s );
    }

#endif
