#ifndef CONTAINER_H
#define CONTAINER_H

#include <list>

#include "storage/Volume.h"
#include "storage/StorageTypes.h"
#include "storage/StorageTmpl.h"


namespace storage
{
    using std::list;


class Container
    {
    friend class Storage;
    protected:
	template<typename L1, typename L2> friend class ListListIterator;

	typedef std::list<Volume*> VCont;
	typedef VCont::iterator VIter;
	typedef VCont::const_iterator CVIter;
	typedef VCont::reverse_iterator RVIter;
	typedef VCont::const_reverse_iterator CRVIter;


    public:
	bool operator== ( const Container& rhs ) const
	    { return( typ == rhs.typ && nm == rhs.nm && del == rhs.del ); }
	bool operator!= ( const Container& rhs ) const
	    { return( !(*this==rhs) ); }
	bool operator< ( const Container& rhs ) const
	    {
	    if( typ != rhs.typ )
		return( typ<rhs.typ );
	    else if( nm != rhs.nm )
		return( nm<rhs.nm );
	    else
		return( !del );
	    }
	bool operator<= ( const Container& rhs ) const
	    { return( *this<rhs || *this==rhs ); }
	bool operator>= ( const Container& rhs ) const
	    { return( !(*this<rhs) ); }
	bool operator> ( const Container& rhs ) const
	    { return( !(*this<=rhs) ); }
	virtual bool equalContent( const Container& rhs ) const;
	virtual string getDiffString( const Container& c ) const;
	virtual void logDifference( const Container& c ) const;

	virtual void getCommitActions(list<commitAction>& l) const;
	virtual void getToCommit(storage::CommitStage stage, list<const Container*>& col,
				 list<const Volume*>& vol) const;

	virtual int commitChanges( storage::CommitStage stage );
	virtual int commitChanges( storage::CommitStage stage, Volume* vol );
	virtual void changeDeviceName( const string& old, const string& nw ) {}
	unsigned numVolumes() const;
	bool isEmpty() const;
	void getInfo( storage::ContainerInfo& info ) const;
	bool findVolume( const string& device, Volume*& vol );

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
	Container( Storage * const, const string& Name, storage::CType typ );
	Container( const Container& );
	Storage * getStorage() const { return sto; }
	virtual ~Container();
	const string& name() const { return nm; }
	const string& device() const { return dev; }
	storage::CType type() const { return typ; }
	bool deleted() const { return del; }
	bool created() const { return create; }
	void setDeleted( bool val=true ) { del=val; }
	void setCreated( bool val=true ) { create=val; }
	void setSilent( bool val=true ) { silent=val; }

	void clearUsedBy() { uby.clear(); }
	void setUsedBy(storage::UsedByType ub_type, const string& ub_name) { uby.set(ub_type, ub_name); }
	const storage::usedBy& getUsedBy() const { return uby; }
	storage::UsedByType getUsedByType() const { return uby.type(); }

	bool readonly() const { return ronly; }
	unsigned long minorNr() const { return mnr; }
	unsigned long majorNr() const { return mjr; }
	unsigned long long sizeK() const { return size_k; }
	virtual string removeText(bool doing=true) const;
	virtual string createText(bool doing=true) const;
	virtual int resizeVolume( Volume* v, unsigned long long newSize );
	virtual int removeVolume( Volume* v );
	static storage::CType staticType() { return storage::CUNKNOWN; }
	friend std::ostream& operator<< (std::ostream& s, const Container &c );
	virtual Container* getCopy() const { return( new Container( *this ) ); }
	bool compareContainer( const Container* c, bool verbose ) const;
	void setExtError( const string& txt ) const;
	void setExtError( const SystemCmd& cmd, bool serr=true ) const;

    protected:
	typedef CVIter ConstPlainIterator;
	ConstPlainIterator begin() const { return vols.begin(); }
	ConstPlainIterator end() const { return vols.end(); }

	typedef VIter PlainIterator;
	PlainIterator begin() { return vols.begin(); }
	PlainIterator end() { return vols.end(); }

	virtual void print( std::ostream& s ) const { s << *this; }
	void addToList(Volume* e);
	bool removeFromList( Volume* e );
	virtual int doCreate( Volume * v );
	virtual int doRemove( Volume * v );
	virtual int doResize( Volume * v );
	virtual void logData(const string& Dir) const {}
	Container& operator=( const Container& );
	static bool stageDecrease( const Volume& v )
	    { return( v.deleted()||v.needShrink()); }
	static bool stageCreate( const Volume& v )
	    { return( v.created()||v.needExtend()); }

	static const string type_names[COTYPE_LAST_ENTRY];

	Storage * const sto;
	storage::CType typ;
	string nm;
	string dev;
	bool del;
	bool create;
	bool silent;
	bool ronly;
	storage::usedBy uby;
	unsigned long long size_k;
	unsigned long mnr;
	unsigned long mjr;
	VCont vols;

	mutable storage::ContainerInfo info; // workaround for broken ycp bindings
    };

}

#endif
