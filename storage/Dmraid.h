#ifndef DMRAID_H
#define DMRAID_H

#include "storage/DmPart.h"

namespace storage
{

class DmraidCo;
class Partition;

class Dmraid : public DmPart
    {
    public:
	Dmraid( const DmraidCo& d, unsigned nr, Partition* p=NULL );
	Dmraid( const DmraidCo& d, const Dmraid& rd );

	virtual ~Dmraid();
	void getInfo( storage::DmraidInfo& info ) const;
	friend std::ostream& operator<< (std::ostream& s, const Dmraid &p );
	virtual void print( std::ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;
	string resizeText( bool doing ) const;
	string setTypeText( bool doing=true ) const;
	bool equalContent( const Dmraid& rhs ) const;
	void logDifference( const Dmraid& d ) const;
	static bool notDeleted( const Dmraid& l ) { return( !l.deleted() ); }

    protected:
	virtual const string shortPrintedName() const { return( "Dmraid" ); }
	Dmraid& operator=( const Dmraid& );
    };

}

#endif
