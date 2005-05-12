#ifndef PE_CONTAINER_H
#define PE_CONTAINER_H

#include "y2storage/Container.h"
#include "y2storage/Dm.h"

class PeContainer : public Container
    {
    friend class Storage;

    public:
	PeContainer( Storage * const s, CType t );
	virtual ~PeContainer();
	unsigned long long peSize() const { return pe_size; }
	friend inline ostream& operator<< (ostream&, const PeContainer& );

	int setPeSize( long long unsigned, bool lvm1 );
	
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
	    Pv() { num_pe = free_pe = 0; }
	    };

	friend inline ostream& operator<< (ostream&, const Pv& );

	void init();
	unsigned long long capacityInKb() const {return pe_size*num_pe;}
	virtual void print( ostream& s ) const { s << *this; }
	unsigned long leByLvRemove() const;
	int tryUnusePe( const string& dev, std::list<Pv>& pl, std::list<Pv>& pladd,
	                std::list<Pv>& plrem, unsigned long& removed_pe );
	static int addLvPeDistribution( unsigned long le, unsigned stripe,
					std::list<Pv>& pl, std::list<Pv>& pladd, 
					std::map<string,unsigned long>& pe_map );
	static int remLvPeDistribution( unsigned long le, std::map<string,unsigned long>& pe_map,
					std::list<Pv>& pl, std::list<Pv>& pladd );
	virtual bool checkConsistency() const;

	void addPv( Pv*& p );

	unsigned long long pe_size;
	unsigned long num_pe;
	unsigned long free_pe;
	std::list<Pv> pv;
	std::list<Pv> pv_add;
	std::list<Pv> pv_remove;
    };

inline ostream& operator<< (ostream& s, const PeContainer& d )
    {
    s << *((Container*)&d);
    s << " SizeM:" << d.capacityInKb()/1024
      << " PeSize:" << d.pe_size
      << " NumPE:" << d.num_pe
      << " FreePE:" << d.free_pe;
    return( s );
    }

inline ostream& operator<< (ostream& s, const PeContainer::Pv& v )
    {
    s << "device:" << v.device
      << " PE:" << v.num_pe
      << " Free:" << v.free_pe
      << " Status:" << v.status
      << " UUID:" << v.uuid;
    return( s );
    }

#endif
