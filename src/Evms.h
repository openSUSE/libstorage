#ifndef EVMS_H
#define EVMS_H

using namespace std;

#include "y2storage/Container.h"

class Evms : public Container
    {
    friend class Storage;

    public:
	Evms( const string& Name="" );
	virtual ~Evms();
	unsigned long long PeSize() const { return pe_size; }
	unsigned NumVol() const { return vols.size(); }
	unsigned NumPv() const { return num_pv; }
	unsigned IsContainer() const { return is_container; }
	static CType const StaticType() { return EVMS; }
	friend inline ostream& operator<< (ostream&, const Evms& );

    protected:
	unsigned long long pe_size;
	unsigned num_pv;
	bool is_container;
    };

inline ostream& operator<< (ostream& s, const Evms& d )
    {
    d.print(s);
    s << " NumPv:" << d.num_pv
      << " PeSize:" << d.pe_size
      << " IsCont:" << d.is_container;
    return( s );
    }

#endif
