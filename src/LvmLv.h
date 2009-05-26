#ifndef LVM_LV_H
#define LVM_LV_H

#include "storage/Dm.h"

namespace storage
{

class LvmVg;

class LvmLv : public Dm
    {
    public:
	LvmLv(const LvmVg& d, const string& name, const string& origin, unsigned long le, 
	      const string& uuid, const string& status, const string& alloc);
	LvmLv(const LvmVg& d, const string& name, const string& origin, unsigned long le, 
	      unsigned stripe);
	LvmLv(const LvmVg& d, const LvmLv& l);

	virtual ~LvmLv();

	const LvmVg* vg() const;

	void calcSize();

	void getState(LvmLvSnapshotStateInfo& info);

	void setOrigin( const string& o ) { origin=o; }
	string getOrigin() const { return origin; }

	bool isSnapshot() const { return !origin.empty(); }
	bool hasSnapshots() const;	

	void setUuid( const string& uuid ) { vol_uuid=uuid; }
	void setStatus( const string& s ) { status=s; }
	void setAlloc( const string& a ) { allocation=a; }
	friend std::ostream& operator<< (std::ostream& s, const LvmLv &p );
	virtual void print( std::ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;
	string resizeText( bool doing ) const;
	void getInfo( storage::LvmLvInfo& info ) const;
	bool equalContent( const LvmLv& rhs ) const;
	void logDifference( const LvmLv& d ) const;

    protected:
	static string makeDmTableName(const string& vg_name, const string& lv_name);
	void init( const string& name );
	virtual const string shortPrintedName() const { return "Lv"; }
	LvmLv& operator=( const LvmLv& );

	string origin;		// only for snapshots, empty otherwise

	string vol_uuid;
	string status;
	string allocation;

	mutable storage::LvmLvInfo info; // workaround for broken ycp bindings
    };

}

#endif
