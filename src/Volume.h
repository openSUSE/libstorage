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

	virtual bool commitChanges() { return true; }

	const string& device() const { return dev; }
	const Container* getContainer() const { return cont; }
	bool deleted() const { return del; }
	bool created() const { return create; }
	void setDeleted( bool val=true ) { del=val; }
	void setCreated( bool val=true ) { create=val; }
	unsigned nr() const { return num; }
	unsigned long long sizeK() const { return size_k; }
	const string& name() const { return nm; }
	unsigned long minorNumber() const { return mnr; }
	unsigned long majorNumber() const { return mjr; }
	void setMajorMinor( unsigned long Major, unsigned long Minor )
	    { mjr=Major; mnr=Minor; }
	void setSize( unsigned long long SizeK ) { size_k=SizeK; }

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
	    bool operator()(const Volume&d) const { return( !d.deleted());}
	    };
	static SkipDeleted SkipDel;
	static bool notDeleted( const Volume&d ) { return( !d.deleted() ); }
	static bool getMajorMinor( const string& device, 
	                           unsigned long& Major, unsigned long& Minor );

    protected:
	void init();
	void setNameDev();

	const Container* const cont;
	bool numeric;
	bool create;
	bool del;
	string nm;
	unsigned num;
	unsigned long long size_k;
	string dev;
	unsigned long mnr;
	unsigned long mjr;
    };

inline ostream& operator<< (ostream& s, const Volume &v )
    {
    s << "Device:" << v.dev;
    if( v.numeric )
      s << " Nr:" << v.num;
    else
      s << " Name:" << v.nm;
    s << " SizeK:" << v.size_k 
      << " Node <" << v.mjr
      << ":" << v.mnr << ">";
    if( v.del )
      s << " deleted";
    if( v.create )
      s << " created";
    return( s );
    }

#endif
