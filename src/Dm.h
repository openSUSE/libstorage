#ifndef DM_H
#define DM_H

#include <map>

#include "y2storage/Volume.h"

class PeContainer;

class Dm : public Volume
    {
    public:
	Dm( const PeContainer& d, const string& tn );

	virtual ~Dm();
	const string& getTableName() { return( tname ); }
	void setTableName( const string& name ) { tname=name; }
	unsigned long getLe() const { return num_le; }
	void setLe( unsigned long le );
	void calcSize();
	const std::map<string,unsigned long>& getPeMap() const { return( pe_map ); }
	void setPeMap( const std::map<string,unsigned long>& m ) { pe_map = m; }
	unsigned long usingPe( const string& dev ) const;
	void getTableInfo();
	virtual bool checkConsistency() const;
	unsigned stripes() const { return stripe; }
	friend ostream& operator<< (ostream& s, const Dm &p );
	virtual void print( ostream& s ) const { s << *this; }
	virtual string removeText( bool doing ) const;
	virtual string formatText( bool doing ) const;

    protected:
	void init();
	const PeContainer* const pec() const;
	virtual const string shortPrintedName() const { return( "Dm" ); }

	string tname;
	unsigned long num_le;
	unsigned stripe;
	std::map<string,unsigned long> pe_map;
    };

inline ostream& operator<< (ostream& s, const Dm &p )
    {
    s << p.shortPrintedName() << " ";
    s << *(Volume*)&p;
    s << " LE:" << p.num_le;
    if( p.stripe>1 )
      s << " Stripes:" << p.stripe;
    if( !p.pe_map.empty() )
      s << " pe_map:" << p.pe_map;
    return( s );
    }


#endif
