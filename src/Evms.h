#ifndef EVMS_H
#define EVMS_H

#include "y2storage/Dm.h"

class EvmsCo;

class Evms : public Dm
    {
    public:
	Evms( const EvmsCo& d, const string& name, unsigned long le, unsigned stripe );
	Evms( const EvmsCo& d, const string& name, unsigned long le );
	virtual ~Evms();
	unsigned compatible() const { return compat; }
	friend std::ostream& operator<< (std::ostream& s, const Evms &p );
	virtual void print( std::ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;
	string resizeText( bool doing ) const;

    protected:
	void init( const string& name );
	virtual const string shortPrintedName() const { return( "Evms" ); }

	bool compat;
    };

inline std::ostream& operator<< (std::ostream& s, const Evms &p )
    {
    s << *(Dm*)&p;
    if( !p.compat )
      s << " native";
    return( s );
    }


#endif
