#ifndef NFS_CO_H
#define NFS_CO_H

#include "y2storage/Container.h"
#include "y2storage/Nfs.h"

namespace storage
{
class NfsCo : public Container
    {
    friend class Storage;

    public:
	NfsCo( Storage * const s, ProcMounts& mounts );
	NfsCo( Storage * const s );
	NfsCo( const NfsCo& rhs );

	virtual ~NfsCo();
	static storage::CType staticType() { return storage::NFSC; }
	friend std::ostream& operator<< ( std::ostream&, const NfsCo& );
	int addNfs( const string& nfsDev, unsigned long long sizeK,
		    const string& mp );

	int removeVolume( Volume* v );
	int doRemove( Volume* );

	bool equalContent( const Container& rhs ) const;
	void logDifference( const Container& d ) const;
	
    protected:
	// iterators over NFS volumes
	// protected typedefs for iterators over NFS volumes
	typedef CastIterator<VIter, Nfs *> NfsInter;
	typedef CastIterator<CVIter, const Nfs *> NfsCInter;
	template< class Pred >
	    struct NfsPI { typedef ContainerIter<Pred, NfsInter> type; };
	template< class Pred >
	    struct NfsCPI { typedef ContainerIter<Pred, NfsCInter> type; };
	typedef CheckFnc<const Nfs> CheckFncNfs;
	typedef CheckerIterator< CheckFncNfs, NfsPI<CheckFncNfs>::type,
				 NfsInter, Nfs > NfsPIterator;
	typedef CheckerIterator< CheckFncNfs, NfsCPI<CheckFncNfs>::type,
				 NfsCInter, const Nfs > NfsCPIterator;
	typedef DerefIterator<NfsPIterator,Nfs> NfsIter;
	typedef DerefIterator<NfsCPIterator,const Nfs> ConstNfsIter;
	typedef IterPair<NfsIter> NfsPair;
	typedef IterPair<ConstNfsIter> ConstNfsPair;

	NfsPair nfsPair( bool (* Check)( const Nfs& )=NULL)
	    {
	    return( NfsPair( nfsBegin( Check ), nfsEnd( Check ) ));
	    }
	NfsIter nfsBegin( bool (* Check)( const Nfs& )=NULL)
	    {
	    IterPair<NfsInter> p( (NfsInter(begin())), (NfsInter(end())) );
	    return( NfsIter( NfsPIterator( p, Check )) );
	    }
	NfsIter nfsEnd( bool (* Check)( const Nfs& )=NULL)
	    {
	    IterPair<NfsInter> p( (NfsInter(begin())), (NfsInter(end())) );
	    return( NfsIter( NfsPIterator( p, Check, true )) );
	    }

	ConstNfsPair nfsPair( bool (* Check)( const Nfs& )=NULL) const
	    {
	    return( ConstNfsPair( nfsBegin( Check ), nfsEnd( Check ) ));
	    }
	ConstNfsIter nfsBegin( bool (* Check)( const Nfs& )=NULL) const
	    {
	    IterPair<NfsCInter> p( (NfsCInter(begin())), (NfsCInter(end())) );
	    return( ConstNfsIter( NfsCPIterator( p, Check )) );
	    }
	ConstNfsIter nfsEnd( bool (* Check)( const Nfs& )=NULL) const
	    {
	    IterPair<NfsCInter> p( (NfsCInter(begin())), (NfsCInter(end())) );
	    return( ConstNfsIter( NfsCPIterator( p, Check, true )) );
	    }

	NfsCo( Storage * const s, const string& File );

	bool findNfs( const string& dev, NfsIter& i );
	bool findNfs( const string& dev );

	static list<string> filterOpts(const list<string>& opts);
	void getNfsData( ProcMounts& mounts );
	void init();

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new NfsCo( *this ) ); }

	void logData(const string& Dir) const;
    };

}

#endif
