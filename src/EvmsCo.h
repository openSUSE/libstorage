#ifndef EVMS_CO_H
#define EVMS_CO_H

#include <list>
#include <map>

#include "y2storage/PeContainer.h"
#include "y2storage/Evms.h"

class storage::fdstream;

struct EvmsObj
    {
    EvmsObj( unsigned i=0, const string& n="", unsigned v=0 ) : id(i), vol(v), name(n) {}
    unsigned id;
    unsigned vol;
    string name;
    inline bool operator== ( const EvmsObj& rhs ) const
	{ return( id==rhs.id && name==rhs.name && vol==rhs.vol ); }
    };

struct EvmsVol
    {
    EvmsVol( unsigned i=0, const string& n="", unsigned s=0 ) : id(i),  sizeK(s), name(n) {native=false;}
    unsigned id;
    unsigned long long sizeK;
    bool native;
    string name;
    string device;
    inline bool operator==( const EvmsVol& rhs ) const
	{ return( id==rhs.id && name==rhs.name && sizeK==rhs.sizeK && 
		  device==rhs.device ); }
    };

struct EvmsCont
    {
    struct peinfo
	{
	peinfo() { id=size=free=0; }
	unsigned id;
	unsigned long long size;
	unsigned long long free;
	string uuid;
	inline bool operator==( const peinfo& rhs ) const
	    { return( id==rhs.id && size==rhs.size && free==rhs.free && 
		      uuid==rhs.uuid ); }
	};
	
    EvmsCont( const string& n="" ) : name(n) 
	{lvm1=true; peSize=free=sizeK=0; }
    string name;
    string uuid;
    bool lvm1;
    unsigned long long peSize;
    unsigned long long free;
    unsigned long long sizeK;
    std::list<peinfo> consumes;
    std::list<unsigned> creates;
    inline bool operator==( const EvmsCont& rhs ) const
	{ return( peSize==rhs.peSize && name==rhs.name && lvm1==rhs.lvm1 &&
	          sizeK==rhs.sizeK && consumes==rhs.consumes &&
		  creates==rhs.creates ); }
    };

struct EvmsTree
    {
    EvmsTree() {}
    std::map<unsigned,EvmsObj> objects;
    std::map<unsigned,EvmsVol> volumes;
    std::list<EvmsCont> cont;
    inline bool operator==( const EvmsTree& rhs ) const
	{ return( objects==rhs.objects && volumes==rhs.volumes && cont==rhs.cont ); }
    };

std::ostream& operator<< (std::ostream&, const EvmsTree& );

class EvmsCo : public PeContainer
    {
    friend class Storage;

    public:
	EvmsCo( Storage * const s, const EvmsTree& data );
	EvmsCo( Storage * const s, const EvmsCont& cont, const EvmsTree& data );
	virtual ~EvmsCo();
	unsigned numVol() const { return vols.size(); }
	static storage::CType const staticType() { return storage::EVMS; }
	friend inline std::ostream& operator<< (std::ostream&, const EvmsCo& );

	int removeVg();
	int extendVg( const std::list<string>& dl );
	int extendVg( const string& device );
	int reduceVg( const std::list<string>& dl );
	int reduceVg( const string& device );
	int createLv( const string& name, unsigned long long sizeK, 
		      unsigned stripe, string& device );
	int removeLv( const string& name );

	void getCommitActions( std::list<storage::commitAction*>& l ) const;
	int commitChanges( storage::CommitStage stage );
	int getToCommit( storage::CommitStage stage, std::list<Container*>& col,
			 std::list<Volume*>& vol );
	int resizeVolume( Volume* v, unsigned long long newSize );
	int removeVolume( Volume* v );
	static void activate( bool val=true );
	static void getEvmsList( EvmsTree& data );
	
    protected:
	// iterators over EVMS Volumes
	// protected typedefs for iterators over EVMS Volumes
	typedef CastIterator<VIter, Evms *> EvmsInter;
	typedef CastIterator<CVIter, const Evms *> EvmsCInter;
	template< class Pred >
	    struct EvmsPI { typedef ContainerIter<Pred, EvmsInter> type; };
	template< class Pred >
	    struct EvmsCPI { typedef ContainerIter<Pred, EvmsCInter> type; };
	typedef CheckFnc<const Evms> CheckFncEvms;
	typedef CheckerIterator< CheckFncEvms, EvmsPI<CheckFncEvms>::type,
				 EvmsInter, Evms > EvmsPIterator;
	typedef CheckerIterator< CheckFncEvms, EvmsCPI<CheckFncEvms>::type,
				 EvmsCInter, const Evms > EvmsCPIterator;
	typedef DerefIterator<EvmsPIterator,Evms> EvmsIter;
	typedef DerefIterator<EvmsCPIterator,const Evms> ConstEvmsIter;
	typedef IterPair<EvmsIter> EvmsPair;
	typedef IterPair<ConstEvmsIter> ConstEvmsPair;

	EvmsPair evmsPair( bool (* Check)( const Evms& )=NULL)
	    {
	    return( EvmsPair( evmsBegin( Check ), evmsEnd( Check ) ));
	    }
	EvmsIter evmsBegin( bool (* Check)( const Evms& )=NULL)
	    {
	    IterPair<EvmsInter> p( (EvmsInter(begin())), (EvmsInter(end())) );
	    return( EvmsIter( EvmsPIterator( p, Check )) );
	    }
	EvmsIter evmsEnd( bool (* Check)( const Evms& )=NULL)
	    {
	    IterPair<EvmsInter> p( (EvmsInter(begin())), (EvmsInter(end())) );
	    return( EvmsIter( EvmsPIterator( p, Check, true )) );
	    }

	ConstEvmsPair evmsPair( bool (* Check)( const Evms& )=NULL) const
	    {
	    return( ConstEvmsPair( evmsBegin( Check ), evmsEnd( Check ) ));
	    }
	ConstEvmsIter evmsBegin( bool (* Check)( const Evms& )=NULL) const
	    {
	    IterPair<EvmsCInter> p( (EvmsCInter(begin())), (EvmsCInter(end())) );
	    return( ConstEvmsIter( EvmsCPIterator( p, Check )) );
	    }
	ConstEvmsIter evmsEnd( bool (* Check)( const Evms& )=NULL) const
	    {
	    IterPair<EvmsCInter> p( (EvmsCInter(begin())), (EvmsCInter(end())) );
	    return( ConstEvmsIter( EvmsCPIterator( p, Check, true )) );
	    }

	EvmsCo( Storage * const s, const string& File, int );

	void getCoData( const string& name, const EvmsTree& data, 
	                bool check=false );
	void init();
	virtual void print( std::ostream& s ) const { s << *this; }
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

	void logData( const string& Dir );
	void addLv( unsigned long le, const string& name );
	void addPv( const Pv* p );
	static bool attachToSocket( bool attach=true );
	static bool sendCommand( const string& cmd, bool one_line, 
	                         std::list<string>& lines );
	static int getSocketFd();
	static bool startHelper( bool retry=false );
	static string unEvmsDevice( const string& dev );

	string status;
	string uuid;
	bool lvm1;
	static bool active;
	static int sockfd;
    };

inline std::ostream& operator<< (std::ostream& s, const EvmsCo& d )
    {
    s << *((PeContainer*)&d);
    s << " status:" << d.status;
    if( d.lvm1 )
      s << " lvm1";
    s << " UUID:" << d.uuid;
    return( s );
    }

#endif
