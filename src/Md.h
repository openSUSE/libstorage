#ifndef MD_H
#define MD_H

#include "y2storage/StorageInterface.h"
#include "y2storage/Volume.h"

class Md : public Volume
    {
    public:
	Md( const Container& d, unsigned Pnr, storage::MdType Type, 
	    const std::list<string>& devs );
	Md( const Container& d, const string& line, const string& line2 );

	virtual ~Md();
	storage::MdType personality() const { return md_type; }
	void setPersonality( storage::MdType val ); 
	storage::MdParity parity() const { return md_parity; }
	void setParity( storage::MdParity val ) { md_parity=val; }
	unsigned long chunkSize() const { return chunk; }
	void setChunkSize( unsigned long val ) { chunk=val; }
	void setMdUuid( const string&val ) { md_uuid=val; }
	bool destroySb() const { return( destrSb ); }
	void setDestroySb( bool val=true ) { destrSb=val; }
	const string& getMdUuid() const { return(md_uuid); }
	const string& pName() const { return md_names[md_type]; }
	const string& ptName() const { return par_names[md_parity]; }
	void getDevs( std::list<string>& devices, bool all=true, bool spare=false ) const; 
	void addSpareDevice( const string& dev );
	int checkDevices();
	int addDevice( const string& dev, bool spare=false );
	int removeDevice( const string& dev );
	void raidtabLines( std::list<string>& ) const ; 
	string mdadmLine() const; 
	string createCmd() const;

	static const string& pName( storage::MdType t ) { return md_names[t]; }
	static bool mdStringNum( const string& name, unsigned& num ); 
	friend inline std::ostream& operator<< (std::ostream& s, const Md& m );
	virtual void print( std::ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;
	static bool notDeleted( const Md& l ) { return( !l.deleted() ); }

	void getInfo( storage::MdInfo& info ) const;

    protected:
	void init();
	void computeSize();
	static storage::MdType toMdType( const string& val );
	static storage::MdParity toMdParity( const string& val );

	storage::MdType md_type;
	storage::MdParity md_parity;
	unsigned long chunk;
	string md_uuid;
	bool destrSb;
	std::list<string> devs;
	std::list<string> spare;
	static string md_names[storage::MULTIPATH+1];
	static string par_names[storage::RIGHT_SYMMETRIC+1];
    };

inline std::ostream& operator<< (std::ostream& s, const Md& m )
    {
    s << "Md " << *(Volume*)&m
      << " Personality:" << m.pName();
    if( m.chunk>0 )
	s << " Chunk:" << m.chunk;
    if( m.md_parity!=storage::PAR_NONE )
	s << " Parity:" << m.ptName();
    if( !m.md_uuid.empty() )
	s << " MD UUID:" << m.md_uuid;
    if( m.destrSb )
	s << " destroySb";
    s << " Devices:" << m.devs;
    if( !m.spare.empty() )
	s << " Spare:" << m.spare;
    return( s );
    }


#endif
