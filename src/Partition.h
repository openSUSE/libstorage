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

    protected:
	unsigned cyl_start;
	unsigned cyl_size;
    };

#endif
