#ifndef EVMS_H
#define EVMS_H

using namespace std;

#include "y2storage/Container.h"

class Evms : public Container
    {
    friend class Storage;

    public:
	Evms( const Storage * const s, const string& Name="" );
	virtual ~Evms();
	unsigned long long peSize() const { return pe_size; }
	unsigned numVol() const { return vols.size(); }
	unsigned numPv() const { return num_pv; }
	unsigned isContainer() const { return is_container; }
	static CType const staticType() { return EVMS; }
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
      << " PeSize:" << d.pe_size;
    if( d.is_container )
      s << " is_cont";
    return( s );
    }

#endif
