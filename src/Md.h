#ifndef MD_H
#define MD_H

using namespace std;

#include "y2storage/StorageInterface.h"
#include "y2storage/Volume.h"

using namespace storage;


class Md : public Volume
    {
    public:

	enum MdParity { PAR_NONE, LEFT_ASYMMETRIC, LEFT_SYMMETRIC,
	                RIGHT_ASYMMETRIC, RIGHT_SYMMETRIC };

	Md( const Container& d, unsigned Pnr, MdType Type, 
	    const list<string>& devs );
	Md( const Container& d, const string& line, const string& line2 );

	virtual ~Md();
	MdType personality() const { return md_type; }
	MdParity parity() const { return md_parity; }
	unsigned long chunkSize() const { return chunk; }
	const string& pName() const { return md_names[md_type]; }
	const string& ptName() const { return par_names[md_parity]; }
	void getDevs( list<string>& devices, bool all=true, bool spare=false ) const; 
	void addSpareDevice( const string& dev );
	void raidtabLines( list<string>& ) const ; 
	string createCmd() const;

	static const string& pName( MdType t ) { return md_names[t]; }
	static bool mdStringNum( const string& name, unsigned& num ); 
	friend inline ostream& operator<< (ostream& s, const Md& m );
	virtual void print( ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;

    protected:
	void init();
	static MdType toMdType( const string& val );
	static MdParity toMdParity( const string& val );

	MdType md_type;
	MdParity md_parity;
	unsigned long chunk;
	list<string> devs;
	list<string> spare;
	static string md_names[MULTIPATH+1];
	static string par_names[RIGHT_SYMMETRIC+1];
    };

inline ostream& operator<< (ostream& s, const Md& m )
    {
    s << "Md " << *(Volume*)&m
      << " Personality:" << m.pName();
    if( m.chunk>0 )
	s << " Chunk:" << m.chunk;
    if( m.md_parity!=Md::PAR_NONE )
	s << " Parity:" << m.ptName();
    s << " Devices:" << m.devs;
    if( m.spare.size()>0 )
	s << " Spare:" << m.spare;
    return( s );
    }


#endif
