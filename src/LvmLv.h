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

    protected:
	unsigned stripes;
    };

#endif
