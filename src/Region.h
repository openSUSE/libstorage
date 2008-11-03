#ifndef REGION_H
#define REGION_H

#include <algorithm>

namespace storage
{

class Region 
    {
    public:
	Region() : s(0), l(0) {}
	Region( unsigned long start, unsigned long len ) : s(start), l(len) {}
	Region( const Region& x ) 
	    { *this = x; }
	Region& operator=(const Region& r)
	    { s = r.start(); l = r.len(); return( *this ); }
	bool doIntersect( const Region& r ) const
	    { return( r.start() <= end() && r.end() >= start() ); }
	Region intersect( const Region& r ) const
	    {
	    Region ret;
	    if (doIntersect(r))
		{
		unsigned long s = std::max( r.start(), start() );
		unsigned long e = std::min( r.end(), end() );
		ret = Region( s, e-s+1 );
		}
	    return( ret );
	    }
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

inline std::ostream& operator<< (std::ostream& s, const Region &p )
    {
    s << "[" << p.start() << "," << p.len() << "]";
    return( s );
    }

inline std::istream& operator>> (std::istream& s, Region &p )
    {
    unsigned long start, len;
    s >> start >> len;
    p = Region( start, len );
    return( s );
    }

}

#endif
