#ifndef REGION_H
#define REGION_H

class Region 
    {
    public:
	Region() : s(0), l(0) {};
	Region( unsigned long start, unsigned long len ) : s(start), l(len) {};
	Region( const Region& x ) 
	    { *this = x; }
	Region& operator=(const Region& r)
	    { s = r.start(); l = r.len(); return( *this ); }
	bool intersect( const Region& r ) const
	    { return( r.start() <= end() && r.end() >= start() ); }
	bool inside( const Region& r ) const
	    { return( start()>=r.start() && end() <= r.end() ); }
	bool operator==(const Region& r) const
	    { return( r.start()==s && r.len()==l ); }
	bool operator!=(const Region& r) const
	    { return( ! (*this==r) ); }
	bool operator<(const Region& r) const
	    { return( s < r.start() ); }
	bool operator>(const Region& r) const
	    { return( s > r.start() ); }
	unsigned long start() const { return( s ); }
	unsigned long end() const { return( s+l-1 ); }
	unsigned long len() const { return( l ); }
    protected:
	unsigned long s;
	unsigned long l;
    };

inline ostream& operator<< (ostream& s, const Region &p )
    {
    s << "[" << p.start() << "," << p.len() << "]";
    return( s );
    }

inline istream& operator>> (istream& s, Region &p )
    {
    unsigned long start, len;
    s >> start >> len;
    p = Region( start, len );
    return( s );
    }

#endif
