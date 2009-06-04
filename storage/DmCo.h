#ifndef DM_CO_H
#define DM_CO_H

#include "storage/PeContainer.h"
#include "storage/Dm.h"

namespace storage
{
class ProcPart;

class DmCo : public PeContainer
    {
    friend class Storage;

    public:
	DmCo( Storage * const s, bool detect, ProcPart& ppart );
	DmCo( const DmCo& rhs );
	virtual ~DmCo();
	static storage::CType staticType() { return storage::DM; }
	friend std::ostream& operator<< (std::ostream&, const DmCo& );
	bool equalContent( const Container& rhs ) const;
	void logDifference( const Container& d ) const;
	void updateDmMaps();

	int removeDm( const string& table );
	int removeVolume( Volume* v );
	
    protected:
	DmCo( Storage * const s, const string& File );

	void getDmData( ProcPart& ppart );
	bool findDm( unsigned num, DmIter& i );
	bool findDm( unsigned num ); 
	bool findDm( const string& dev, DmIter& i );
	bool findDm( const string& dev ); 
	void addDm( Dm* m );
	void checkDm( Dm* m );
	void updateEntry( const Dm* m );
	virtual Container* getCopy() const { return( new DmCo( *this ) ); }

	void init();

	storage::EncryptType detectEncryption( const string& device ) const;

	virtual void print( std::ostream& s ) const { s << *this; }

	int doRemove( Volume* v );

	void logData(const string& Dir) const;
    };

}

#endif
