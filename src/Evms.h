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
	unsigned NumVol() const { return Vols.size(); }
	unsigned NumPv() const { return num_pv; }
	static CType const StaticType() { return EVMS; }

    protected:
	unsigned long long pe_size;
	unsigned num_pv;
    };

#endif
