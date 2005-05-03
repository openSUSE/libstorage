#ifndef EVMS_CO_H
#define EVMS_CO_H

#include "y2storage/Container.h"

class EvmsCo : public Container
    {
    friend class Storage;

    public:
	EvmsCo( Storage * const s, const string& Name="" );
	virtual ~EvmsCo();
	unsigned long long peSize() const { return pe_size; }
	unsigned numVol() const { return vols.size(); }
	unsigned numPv() const { return num_pv; }
	unsigned isContainer() const { return is_container; }
	static CType const staticType() { return EVMS; }
	friend inline ostream& operator<< (ostream&, const EvmsCo& );

    protected:
	virtual void print( ostream& s ) const { s << *this; }

	unsigned long long pe_size;
	unsigned num_pv;
	bool is_container;
    };

inline ostream& operator<< (ostream& s, const EvmsCo& d )
    {
    s << *((Container*)&d);
    s << " NumPv:" << d.num_pv
      << " PeSize:" << d.pe_size;
    if( d.is_container )
      s << " is_cont";
    return( s );
    }

#endif
