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

	virtual ~LvmLv();
	void setUuid( const string& uuid ) { vol_uuid=uuid; }
	void setStatus( const string& s ) { status=s; }
	void setAlloc( const string& a ) { allocation=a; }
	friend ostream& operator<< (ostream& s, const LvmLv &p );
	virtual void print( ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;
	string resizeText( bool doing ) const;

    protected:
	void init( const string& name );

	string vol_uuid;
	string status;
	string allocation;
	virtual const string shortPrintedName() const { return( "Lv" ); }
    };

inline ostream& operator<< (ostream& s, const LvmLv &p )
    {
    s << *(Dm*)&p;
    if( !p.vol_uuid.empty() )
      s << " UUID:" << p.vol_uuid;
    if( !p.status.empty() )
      s << " " << p.status;
    if( !p.allocation.empty() )
      s << " " << p.allocation;
    return( s );
    }


#endif
