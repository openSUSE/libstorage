#ifndef EVMS_VOL_H
#define EVMS_VOL_H

using namespace std;

#include "y2storage/Volume.h"

class Evms;

class EvmsVol : public Volume
    {
    public:
	EvmsVol( const Evms& d, const string& name, unsigned Stripes=1 );
	virtual ~EvmsVol();
	unsigned Stripes() const { return stripes; }

    protected:
	unsigned stripes;
    };

#endif
