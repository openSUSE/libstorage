#ifndef LVM_VG_H
#define LVM_VG_H

using namespace std;

#include "y2storage/Container.h"

class LvmVg : public Container
    {
    friend class Storage;

    public:
	LvmVg( const string& Name );
	virtual ~LvmVg();
	unsigned long long PeSize() const { return pe_size; }
	unsigned NumLv() const { return Vols.size(); }
	unsigned NumPv() const { return num_pv; }
	static CType const StaticType() { return LVM; }

    protected:
	unsigned long long pe_size;
	unsigned num_pv;
    };

#endif
