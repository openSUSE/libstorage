#ifndef PARTITION_H
#define PARTITION_H

using namespace std;

#include "y2storage/Volume.h"

class Disk;

class Partition : public Volume
    {
    public:
	Partition( const Disk& d, unsigned Pnr );
	virtual ~Partition();
	unsigned CylStart() const { return cyl_start; }
	unsigned CylSize() const { return cyl_size; }
	friend ostream& operator<< (ostream& s, const Partition &p );

    protected:
	unsigned cyl_start;
	unsigned cyl_size;
    };


inline ostream& operator<< (ostream& s, const Partition &p )
    {
    s << "Partition " << Volume(p) 
      << " Start:" << p.cyl_start
      << " CylNum:" << p.cyl_size;
    return( s );
    }

#endif
