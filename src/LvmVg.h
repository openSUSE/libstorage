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
	unsigned numPv() const { return num_pv; }
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
	LvmVg( Storage * const s, const string& File, bool );
	unsigned long long capacityInKb() const;
	void getCommitActions( list<commitAction*>& l ) const;

	int doCreate( Volume* v );
	int doRemove( Volume* v );
	int doResize( Volume* v );

	void logData( const string& Dir );

	unsigned long long pe_size;
	unsigned num_pv;
	static bool active;
    };

inline ostream& operator<< (ostream& s, const LvmVg& d )
    {
    d.print(s);
    s << " NumPv:" << d.num_pv
      << " PeSize:" << d.pe_size;
    return( s );
    }


#endif
