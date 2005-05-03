#ifndef EVMS_H
#define EVMS_H

#include "y2storage/Volume.h"

class EvmsCo;

class Evms : public Volume
    {
    public:
	Evms( const EvmsCo& d, const string& name, unsigned Stripes=1 );
	virtual ~Evms();
	unsigned stripes() const { return stripe; }
	unsigned compatible() const { return compat; }
	friend ostream& operator<< (ostream& s, const Evms &p );

    protected:
	unsigned stripe;
	bool compat;
    };

inline ostream& operator<< (ostream& s, const Evms &p )
    {
    s << "Evms " << *(Volume*)&p;
    if( p.compat )
      s << " compatible";
    if( p.stripe>1 )
      s << " Stripes:" << p.stripe;
    return( s );
    }


#endif
