#ifndef LVM_LV_H
#define LVM_LV_H

#include <map>

#include "y2storage/Volume.h"

class LvmVg;

class LvmLv : public Volume
    {
    public:
	LvmLv( const LvmVg& d, const string& name, unsigned long le,
	       const string& uuid, const string& status, const string& alloc );
	LvmLv( const LvmVg& d, const string& name, unsigned long le,
	       unsigned stripe );

	virtual ~LvmLv();
	unsigned long getLe() const { return num_le; }
	void setLe( unsigned long le );
	void calcSize();
	void setUuid( const string& uuid ) { vol_uuid=uuid; }
	void setStatus( const string& s ) { status=s; }
	void setAlloc( const string& a ) { allocation=a; }
	const std::map<string,unsigned long>& getPeMap() const { return( pe_map ); }
	void setPeMap( const std::map<string,unsigned long>& m ) { pe_map = m; }
	unsigned long usingPe( const string& dev ) const;
	void getTableInfo();
	bool checkConsistency() const;
	unsigned stripes() const { return stripe; }
	friend ostream& operator<< (ostream& s, const LvmLv &p );
	virtual void print( ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;
	string resizeText( bool doing ) const;

    protected:
	void init();
	const LvmVg* const vg() const;

	string vol_uuid;
	string status;
	string allocation;
	unsigned long num_le;
	unsigned stripe;
	std::map<string,unsigned long> pe_map;
    };

inline ostream& operator<< (ostream& s, const LvmLv &p )
    {
    s << "Lv " << *(Volume*)&p;
    s << " LE:" << p.num_le;
    if( p.stripe>1 )
      s << " Stripes:" << p.stripe;
    if( !p.vol_uuid.empty() )
      s << " UUID:" << p.vol_uuid;
    if( !p.status.empty() )
      s << " " << p.status;
    if( !p.allocation.empty() )
      s << " " << p.allocation;
    if( !p.pe_map.empty() )
      s << " pe_map:" << p.pe_map;
    return( s );
    }


#endif
