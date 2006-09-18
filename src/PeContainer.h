#ifndef PE_CONTAINER_H
#define PE_CONTAINER_H

#include "y2storage/Container.h"
#include "y2storage/Dm.h"

namespace storage
{

class PeContainer : public Container
    {
    friend class Storage;

    public:
	PeContainer( Storage * const s, storage::CType t );
	PeContainer( const PeContainer& c );
	PeContainer& operator= ( const PeContainer& rhs );
	virtual ~PeContainer();

	unsigned long long peSize() const { return pe_size; }
	unsigned long long sizeK() const { return pe_size*num_pe; }
	unsigned long peCount() const { return num_pe; }
	unsigned long peFree() const { return free_pe; }
	unsigned numPv() const { return pv.size(); }
	friend std::ostream& operator<< (std::ostream&, const PeContainer& );

	int setPeSize( long long unsigned, bool lvm1 );
	void unuseDev();
	bool equalContent( const PeContainer& rhs, bool comp_vol=true ) const;
	virtual string logDiff( const PeContainer& d ) const;
	
    protected:
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
	    bool equalContent( const Pv& rhs ) const
		{ return( device==rhs.device && uuid==rhs.uuid &&
		          status==rhs.status && num_pe==rhs.num_pe &&
			  free_pe==rhs.free_pe ); }
	    Pv() { num_pe = free_pe = 0; }
	    };

	friend std::ostream& operator<< (std::ostream&, const Pv& );
	friend void printDevList (std::ostream&, const std::list<Pv>& );

	void init();
	string addList() const;
	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new PeContainer( *this ) ); }

	unsigned long leByLvRemove() const;
	int tryUnusePe( const string& dev, std::list<Pv>& pl, std::list<Pv>& pladd,
	                std::list<Pv>& plrem, unsigned long& removed_pe );
	static int addLvPeDistribution( unsigned long le, unsigned stripe,
					std::list<Pv>& pl, std::list<Pv>& pladd, 
					std::map<string,unsigned long>& pe_map );
	static int remLvPeDistribution( unsigned long le, std::map<string,unsigned long>& pe_map,
					std::list<Pv>& pl, std::list<Pv>& pladd );
	virtual bool checkConsistency() const;

	void addPv( const Pv* p );

	unsigned long long pe_size;
	unsigned long num_pe;
	unsigned long free_pe;
	std::list<Pv> pv;
	std::list<Pv> pv_add;
	std::list<Pv> pv_remove;
    };

}

#endif
