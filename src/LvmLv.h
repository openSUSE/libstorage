#ifndef LVM_LV_H
#define LVM_LV_H

using namespace std;

#include "y2storage/Volume.h"

class LvmVg;

class LvmLv : public Volume
    {
    public:
	LvmLv( const LvmVg& d, const string& name, unsigned Stripes=1 );
	virtual ~LvmLv();
	unsigned Stripes() const { return stripes; }
	friend ostream& operator<< (ostream& s, const LvmLv &p );

    protected:
	unsigned stripes;
    };

inline ostream& operator<< (ostream& s, const LvmLv &p )
    {
    s << "Lv " << Volume(p)
      << " Stripes:" << p.stripes;
    return( s );
    }


#endif
