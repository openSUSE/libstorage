#ifndef VOLUME_H
#define VOLUME_H

using namespace std;

class Container;

class Volume
    {
    public:
	Volume( const Container& d, unsigned Pnr );
	Volume( const Container& d, const string& PName );
	virtual ~Volume();
	const string& Device() const { return dev; }; 
	bool Delete() const { return deleted; }
	unsigned Nr() const { return nr; }
	const string& Name() const { return name; }

        bool operator== ( const Volume& rhs ) const;
        bool operator!= ( const Volume& rhs ) const
            { return( !(*this==rhs) ); }
        bool operator< ( const Volume& rhs ) const;
        bool operator<= ( const Volume& rhs ) const
            { return( *this<rhs || *this==rhs ); }
        bool operator>= ( const Volume& rhs ) const
            { return( !(*this<rhs) ); }
        bool operator> ( const Volume& rhs ) const
            { return( !(*this<=rhs) ); }
	friend ostream& operator<< (ostream& s, const Volume &v );

	struct SkipDeleted
	    {
	    bool operator()(const Volume&d) const { return( !d.Delete());}
	    };
	static SkipDeleted SkipDel;
	static bool NotDeleted( const Volume&d ) { return( !d.Delete() ); }

    protected:
	void Init();

	const Container* const cont;
	bool numeric;
	bool deleted;
	string name;
	unsigned nr;
	string dev;
    };

inline ostream& operator<< (ostream& s, const Volume &v )
    {
    s << "Device:" << v.dev
      << " Del:" << v.deleted;
    if( v.numeric )
      s << " Nr:" << v.nr;
    else
      s << " Name:" << v.name;
    return( s );
    }

#endif
