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
	unsigned Compatible() const { return compatible; }
	friend ostream& operator<< (ostream& s, const EvmsVol &p );

    protected:
	unsigned stripes;
	bool compatible;
    };

inline ostream& operator<< (ostream& s, const EvmsVol &p )
    {
    s << "Vol " << Volume(p);
    if( p.compatible )
      s << " compatible";
    if( p.stripes>1 )
      s << " Stripes:" << p.stripes;
    return( s );
    }


#endif
