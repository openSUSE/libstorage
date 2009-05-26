#ifndef MD_CO_H
#define MD_CO_H

#include "storage/Container.h"
#include "storage/Md.h"

namespace storage
{

class EtcRaidtab;

class MdCo : public Container
    {
    friend class Storage;

    public:
	MdCo( Storage * const s, bool detect );
	MdCo( const MdCo& rhs );

	virtual ~MdCo();
	static storage::CType staticType() { return storage::MD; }
	friend std::ostream& operator<< (std::ostream&, const MdCo& );

	int createMd( unsigned num, storage::MdType type, 
	              const std::list<string>& devs );
	int removeMd( unsigned num, bool destroySb=true );
	int extendMd( unsigned num, const string& dev );
	int shrinkMd( unsigned num, const string& dev );
	int changeMdType( unsigned num, storage::MdType ptype );
	int changeMdChunk( unsigned num, unsigned long chunk );
	int changeMdParity( unsigned num, storage::MdParity ptype );
	int checkMd( unsigned num );
	int getMdState(unsigned num, MdStateInfo& info);
	bool equalContent( const Container& rhs ) const;
	void logDifference( const Container& d ) const;

	unsigned unusedNumber();
	void syncRaidtab();
	void changeDeviceName( const string& old, const string& nw );

	static void activate( bool val, const string& tmpDir  );
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
	int checkUse( const string& dev );
	void addMd( Md* m );
	void checkMd( Md* m );
	void updateEntry( const Md* m );
	void initTab();

	void init();

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new MdCo( *this ) ); }

	int doCreate( Volume* v );
	int doRemove( Volume* v );

	void logData(const string& Dir) const;

	EtcRaidtab *tab;

	static bool active;
    };

}

#endif
