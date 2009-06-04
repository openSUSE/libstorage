#ifndef DMMULTIPATH_H
#define DMMULTIPATH_H

#include "storage/DmPart.h"

namespace storage
{

class DmmultipathCo;
class Partition;

class Dmmultipath : public DmPart
    {
    public:
	Dmmultipath( const DmmultipathCo& d, unsigned nr, Partition* p=NULL );
	Dmmultipath( const DmmultipathCo& d, const Dmmultipath& rd );

	virtual ~Dmmultipath();
	void getInfo( storage::DmmultipathInfo& info ) const;
	friend std::ostream& operator<< (std::ostream& s, const Dmmultipath &p );
	virtual void print( std::ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;
	string resizeText( bool doing ) const;
	string setTypeText( bool doing=true ) const;
	bool equalContent( const Dmmultipath& rhs ) const;
	void logDifference( const Dmmultipath& d ) const;
	static bool notDeleted( const Dmmultipath& l ) { return( !l.deleted() ); }

    protected:
	virtual const string shortPrintedName() const { return( "Dmmultipath" ); }
	Dmmultipath& operator=( const Dmmultipath& );
    };

}

#endif
