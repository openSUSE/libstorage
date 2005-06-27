#ifndef LVM_LV_H
#define LVM_LV_H

#include <map>

#include "y2storage/Dm.h"

class LvmVg;

class LvmLv : public Dm
    {
    public:
	LvmLv( const LvmVg& d, const string& name, unsigned long le,
	       const string& uuid, const string& status, const string& alloc );
	LvmLv( const LvmVg& d, const string& name, unsigned long le,
	       unsigned stripe );
	LvmLv( const LvmVg& d, const LvmLv& l );

	virtual ~LvmLv();
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
	void init( const string& name );
	virtual const string shortPrintedName() const { return( "Lv" ); }
	LvmLv& operator=( const LvmLv& );

	string vol_uuid;
	string status;
	string allocation;
    };

#endif
