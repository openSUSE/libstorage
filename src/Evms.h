#ifndef EVMS_H
#define EVMS_H

#include "y2storage/Dm.h"

namespace storage
{

class EvmsCo;

class Evms : public Dm
    {
    public:
	Evms( const EvmsCo& d, const string& name, unsigned long long le, unsigned stripe );
	Evms( const EvmsCo& d, const string& name, unsigned long long le, bool native );
	Evms( const EvmsCo& d, const Evms& rhs );

	virtual ~Evms();
	unsigned compatible() const { return compat; }
	friend std::ostream& operator<< (std::ostream& s, const Evms &p );
	virtual void print( std::ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;
	string resizeText( bool doing ) const;

	void updateMd();
	bool equalContent( const Evms& rhs ) const;
	void logDifference( const Evms& d ) const;
	const EvmsCo* evmsCo() const;

    protected:
	void init( const string& name );
	virtual const string shortPrintedName() const { return( "Evms" ); }
	Evms& operator=( const Evms& );

	static string getMapperName( const EvmsCo& d, const string& name );

	bool compat;

    };

}

#endif
