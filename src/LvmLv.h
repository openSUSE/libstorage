#ifndef LVM_LV_H
#define LVM_LV_H

using namespace std;

#include "y2storage/Volume.h"

class LvmVg;

class LvmLv : public Volume
    {
    public:
	LvmLv( const LvmVg& d, const string& name, unsigned long le,
	       const string& uuid, const string& status, const string& alloc );

	virtual ~LvmLv();
	unsigned stripes() const { return stripe; }
	friend ostream& operator<< (ostream& s, const LvmLv &p );

    protected:
	void init();

	string vol_uuid;
	string status;
	string allocation;
	unsigned long num_le;
	unsigned stripe;
    };

inline ostream& operator<< (ostream& s, const LvmLv &p )
    {
    s << "Lv " << Volume(p);
    s << " LE:" << p.num_le;
    if( p.stripe>1 )
      s << " Stripes:" << p.stripe;
    if( p.vol_uuid.size()>0 )
      s << " UUID:" << p.vol_uuid;
    if( p.status.size()>0 )
      s << " " << p.status;
    if( p.allocation.size()>0 )
      s << " " << p.allocation;
    return( s );
    }


#endif
