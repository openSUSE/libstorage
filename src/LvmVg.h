#ifndef LVM_VG_H
#define LVM_VG_H

using namespace std;

#include "y2storage/Container.h"

class LvmVg : public Container
    {
    friend class Storage;

    public:
	LvmVg( Storage * const s, const string& Name );
	virtual ~LvmVg();
	unsigned long long peSize() const { return pe_size; }
	unsigned numLv() const { return vols.size(); }
	unsigned numPv() const { return pv.size(); }
	static CType const staticType() { return LVM; }
	friend inline ostream& operator<< (ostream&, const LvmVg& );

	int createLv( const string& name, unsigned long long sizeK, 
	              string& device );
	int removeLv( const string& name );
	int extendVg( const list<string>& dl );
	int extendVg( const string& device );
	int reduceVg( const list<string>& dl );
	int reduceVg( const string& device );
	int commitChanges( CommitStage stage );
	int checkResize( Volume* v, unsigned long long newSize ) const;
	static void activate( bool val=true );
	static void getVgs( list<string>& l );
	
    protected:
	struct Pv
	    {
	    string device;
	    string uuid;
	    string status;
	    unsigned long num_pe;
	    unsigned long free_pe;
	    Pv() { num_pe = free_pe = 0; }
	    };

	friend inline ostream& operator<< (ostream&, const Pv& );

	LvmVg( Storage * const s, const string& File, bool );
	void init();
	unsigned long long capacityInKb() const {return pe_size*num_pe;}
	void getCommitActions( list<commitAction*>& l ) const;
	virtual void print( ostream& s ) const { s << *this; }

	int doCreate( Volume* v );
	int doRemove( Volume* v );
	int doResize( Volume* v );

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
