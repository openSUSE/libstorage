#ifndef LVM_VG_H
#define LVM_VG_H

using namespace std;

#include "y2storage/Container.h"

class LvmVg : public Container
    {
    friend class Storage;

    public:
	LvmVg( const Storage * const s, const string& Name );
	virtual ~LvmVg();
	unsigned long long PeSize() const { return pe_size; }
	unsigned NumLv() const { return vols.size(); }
	unsigned NumPv() const { return num_pv; }
	static CType const StaticType() { return LVM; }
	friend inline ostream& operator<< (ostream&, const LvmVg& );

    protected:
	unsigned long long pe_size;
	unsigned num_pv;
    };

inline ostream& operator<< (ostream& s, const LvmVg& d )
    {
    d.print(s);
    s << " NumPv:" << d.num_pv
      << " PeSize:" << d.pe_size;
    return( s );
    }


#endif
