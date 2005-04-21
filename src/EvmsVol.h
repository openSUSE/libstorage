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
	unsigned stripes() const { return stripe; }
	unsigned compatible() const { return compat; }
	friend ostream& operator<< (ostream& s, const EvmsVol &p );

    protected:
	unsigned stripe;
	bool compat;
    };

inline ostream& operator<< (ostream& s, const EvmsVol &p )
    {
    s << "EvmsVol " << *(Volume*)&p;
    if( p.compat )
      s << " compatible";
    if( p.stripe>1 )
      s << " Stripes:" << p.stripe;
    return( s );
    }


#endif
