#ifndef LVM_VG_H
#define LVM_VG_H

using namespace std;

#include "y2storage/Container.h"
#include "y2storage/LvmLv.h"

class LvmVg : public Container
    {
    friend class Storage;

    public:
	LvmVg( Storage * const s, const string& Name );
	LvmVg( Storage * const s, const string& Name, bool lvm1 );
	virtual ~LvmVg();
	unsigned long long peSize() const { return pe_size; }
	unsigned numLv() const { return vols.size(); }
	unsigned numPv() const { return pv.size(); }
	static CType const staticType() { return LVM; }
	friend inline ostream& operator<< (ostream&, const LvmVg& );

	int removeVg();
	int extendVg( const list<string>& dl );
	int extendVg( const string& device );
	int reduceVg( const list<string>& dl );
	int reduceVg( const string& device );
	int createLv( const string& name, unsigned long long sizeK, 
		      unsigned stripe, string& device );
	int removeLv( const string& name );

	int setPeSize( long long unsigned );
	int commitChanges( CommitStage stage );
	int checkResize( Volume* v, unsigned long long& newSize, 
	                 bool doit, bool& done );
	static void activate( bool val=true );
	static void getVgs( list<string>& l );
	
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

	struct Pv
	    {
	    string device;
	    string uuid;
	    string status;
	    unsigned long num_pe;
	    unsigned long free_pe;

	    static bool comp_le( const Pv& a, const Pv& b )
		{ return( a.free_pe<b.free_pe ); }
	    static bool no_free( const Pv& a )
		{ return( a.free_pe==0 ); }

	    bool operator== ( const Pv& rhs ) const
		{ return( device==rhs.device ); }
	    bool operator== ( const string& dev ) const
		{ return( device==dev); }
	    Pv() { num_pe = free_pe = 0; }
	    };

	friend inline ostream& operator<< (ostream&, const Pv& );

	LvmVg( Storage * const s, const string& File, int );

	void getVgData( const string& name );
	void init();
	unsigned long long capacityInKb() const {return pe_size*num_pe;}
	void getCommitActions( list<commitAction*>& l ) const;
	virtual void print( ostream& s ) const { s << *this; }
	string createVgText( bool doing ) const;
	string removeVgText( bool doing ) const;
	string extendVgText( bool doing, const string& dev ) const;
	string reduceVgText( bool doing, const string& dev ) const;
	unsigned long leByLvRemove() const;
	int tryUnusePe( const string& dev, list<Pv>& pl, list<Pv>& pladd,
	                list<Pv>& plrem, unsigned long& removed_pe );
	static int addLvPeDistribution( unsigned long le, unsigned stripe,
					list<Pv>& pl, list<Pv>& pladd, 
					map<string,unsigned long>& pe_map );
	static int remLvPeDistribution( unsigned long le, map<string,unsigned long>& pe_map,
					list<Pv>& pl, list<Pv>& pladd );
	bool checkConsistency() const;

	int doCreateVg();
	int doRemoveVg();
	int doExtendVg();
	int doReduceVg();
	int doCreate( Volume* v );
	int doRemove( Volume* v );
	int doResize( Volume* v );
	int doCreatePv( const string& device );
	string metaString();
	string instSysString();

	void logData( const string& Dir );
	void addLv( unsigned long& le, string& name, string& uuid,
	            string& status, string& alloc, bool& ro );
	void addPv( Pv*& p );

	unsigned long long pe_size;
	unsigned long num_pe;
	unsigned long free_pe;
	string status;
	string uuid;
	bool lvm1;
	list<Pv> pv;
	list<Pv> pv_add;
	list<Pv> pv_remove;
	static bool lvm_active;
    };

inline ostream& operator<< (ostream& s, const LvmVg& d )
    {
    s << *((Container*)&d);
    s << " SizeM:" << d.capacityInKb()/1024
      << " PeSize:" << d.pe_size
      << " NumPE:" << d.num_pe
      << " FreePE:" << d.free_pe
      << " status:" << d.status;
    if( d.lvm1 )
      s << " lvm1";
    s << " UUID:" << d.uuid;
    return( s );
    }

inline ostream& operator<< (ostream& s, const LvmVg::Pv& v )
    {
    s << "device:" << v.device
      << " PE:" << v.num_pe
      << " Free:" << v.free_pe
      << " Status:" << v.status
      << " UUID:" << v.uuid;
    return( s );
    }

#endif
