#ifndef LVM_VG_H
#define LVM_VG_H

#include "y2storage/PeContainer.h"
#include "y2storage/LvmLv.h"


namespace storage
{
    using std::list;


class LvmVg : public PeContainer
    {
    friend class Storage;
    friend class LvmLv;

    public:
	LvmVg( Storage * const s, const string& Name );
	LvmVg( Storage * const s, const string& Name, bool lvm1 );
	LvmVg( const LvmVg& c );
	virtual ~LvmVg();
	unsigned numLv() const { return vols.size(); }
	bool lvm2() const { return( !lvm1 ); }
	bool inactive() const { return( inactiv ); }
	static storage::CType staticType() { return storage::LVM; }
	friend std::ostream& operator<< (std::ostream&, const LvmVg& );

	int removeVg();
	int extendVg( const std::list<string>& dl );
	int extendVg( const string& device );
	int reduceVg( const std::list<string>& dl );
	int reduceVg( const string& device );
	int createLv( const string& name, unsigned long long sizeK,
		      unsigned stripe, string& device );
	int removeLv( const string& name );
	int changeStripe( const string& name, unsigned long stripe );
	int changeStripeSize( const string& name,
	                      unsigned long long stripeSize );

	int createLvSnapshot(const string& origin, const string& name,
			     unsigned long long cowSizeK, string& device);
	int removeLvSnapshot(const string& name);
	int getLvSnapshotState(const string& name, LvmLvSnapshotStateInfo& info);

	int setPeSize( long long unsigned peSizeK );
	void normalizeDmDevices();
	void getCommitActions(list<commitAction>& l) const;
	void getToCommit(storage::CommitStage stage, list<const Container*>& col,
			 list<const Volume*>& vol) const;
	int commitChanges( storage::CommitStage stage );
	int resizeVolume( Volume* v, unsigned long long newSize );
	int removeVolume( Volume* v );
	void getInfo( storage::LvmVgInfo& info ) const;
	bool equalContent( const Container& rhs ) const;
	void logDifference( const Container& rhs ) const;

	static void activate( bool val=true );
	static bool isActive() { return active; }

	static void getVgs( std::list<string>& l );
	static bool lvNotDeleted( const LvmLv& l ) { return( !l.deleted() ); }

    protected:
	// iterators over LVM LVs
	// protected typedefs for iterators over LVMLVs
	typedef CastIterator<VIter, LvmLv *> LvmLvInter;
	typedef CastIterator<CVIter, const LvmLv *> LvmLvCInter;
	template< class Pred >
	    struct LvmLvPI { typedef ContainerIter<Pred, LvmLvInter> type; };
	template< class Pred >
	    struct LvmLvCPI { typedef ContainerIter<Pred, LvmLvCInter> type; };
	typedef CheckFnc<const LvmLv> CheckFncLvmLv;
	typedef CheckerIterator< CheckFncLvmLv, LvmLvPI<CheckFncLvmLv>::type,
				 LvmLvInter, LvmLv > LvmLvPIterator;
	typedef CheckerIterator< CheckFncLvmLv, LvmLvCPI<CheckFncLvmLv>::type,
				 LvmLvCInter, const LvmLv > LvmLvCPIterator;
	typedef DerefIterator<LvmLvPIterator,LvmLv> LvmLvIter;
	typedef DerefIterator<LvmLvCPIterator,const LvmLv> ConstLvmLvIter;
	typedef IterPair<LvmLvIter> LvmLvPair;
	typedef IterPair<ConstLvmLvIter> ConstLvmLvPair;

	LvmLvPair lvmLvPair( bool (* Check)( const LvmLv& )=NULL)
	    {
	    return( LvmLvPair( lvmLvBegin( Check ), lvmLvEnd( Check ) ));
	    }
	LvmLvIter lvmLvBegin( bool (* Check)( const LvmLv& )=NULL)
	    {
	    IterPair<LvmLvInter> p( (LvmLvInter(begin())), (LvmLvInter(end())) );
	    return( LvmLvIter( LvmLvPIterator( p, Check )) );
	    }
	LvmLvIter lvmLvEnd( bool (* Check)( const LvmLv& )=NULL)
	    {
	    IterPair<LvmLvInter> p( (LvmLvInter(begin())), (LvmLvInter(end())) );
	    return( LvmLvIter( LvmLvPIterator( p, Check, true )) );
	    }

	ConstLvmLvPair lvmLvPair( bool (* Check)( const LvmLv& )=NULL) const
	    {
	    return( ConstLvmLvPair( lvmLvBegin( Check ), lvmLvEnd( Check ) ));
	    }
	ConstLvmLvIter lvmLvBegin( bool (* Check)( const LvmLv& )=NULL) const
	    {
	    IterPair<LvmLvCInter> p( (LvmLvCInter(begin())), (LvmLvCInter(end())) );
	    return( ConstLvmLvIter( LvmLvCPIterator( p, Check )) );
	    }
	ConstLvmLvIter lvmLvEnd( bool (* Check)( const LvmLv& )=NULL) const
	    {
	    IterPair<LvmLvCInter> p( (LvmLvCInter(begin())), (LvmLvCInter(end())) );
	    return( ConstLvmLvIter( LvmLvCPIterator( p, Check, true )) );
	    }

	LvmVg( Storage * const s, const string& File, int );

	void getVgData( const string& name, bool exists=true );
	void init();
	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new LvmVg( *this ) ); }

	string createVgText( bool doing ) const;
	string removeVgText( bool doing ) const;
	string extendVgText( bool doing, const string& dev ) const;
	string reduceVgText( bool doing, const string& dev ) const;

	int doCreateVg();
	int doRemoveVg();
	int doExtendVg();
	int doReduceVg();
	int doCreate( Volume* v );
	int doRemove( Volume* v );
	int doResize( Volume* v );
	int doCreatePv( const string& device );

	string metaString() const;
	string instSysString() const;

	void logData(const string& Dir) const;

	void addLv(unsigned long& le, string& name, string& origin, string& uuid,
		   string& status, string& alloc, bool& ro);
	void addPv( Pv*& p );

	string status;
	string uuid;
	bool lvm1;
	bool inactiv;
	unsigned num_lv;
	static bool active;

	mutable storage::LvmVgInfo info; // workaround for broken ycp bindings
    };

}

#endif
