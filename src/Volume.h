#ifndef VOLUME_H
#define VOLUME_H

using namespace std;

class Container;

class Volume
    {
    public:
	Volume( const Container& d, unsigned Pnr, unsigned long long SizeK );
	Volume( const Container& d, const string& PName, unsigned long long SizeK );
	virtual ~Volume();
	const string& Device() const { return dev; }; 
	bool Delete() const { return deleted; }
	unsigned Nr() const { return nr; }
	unsigned long long SizeK() const { return size_k; }
	const string& Name() const { return name; }
	unsigned long Minor() const { return minor; }
	unsigned long Major() const { return major; }
	void SetMajorMinor( unsigned long Major, unsigned long Minor )
	    { major=Major; minor=Minor; }
	void SetSize( unsigned long long SizeK ) { size_k=SizeK; }

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
	static bool GetMajorMinor( const string& device, 
	                           unsigned long& Major, unsigned long& Minor );

    protected:
	void Init();

	const Container* const cont;
	bool numeric;
	bool deleted;
	string name;
	unsigned nr;
	unsigned long long size_k;
	string dev;
	unsigned long minor;
	unsigned long major;
    };

inline ostream& operator<< (ostream& s, const Volume &v )
    {
    s << "Device:" << v.dev;
    if( v.numeric )
      s << " Nr:" << v.nr;
    else
      s << " Name:" << v.name;
    s << " SizeK:" << v.size_k 
      << " Node <" << v.major
      << ":" << v.minor << ">";
    if( v.deleted )
      s << " deleted";
    return( s );
    }

#endif
