#ifndef DISK_H
#define DISK_H

#include <list>

using namespace std;

#include "y2storage/Partition.h"
#include "y2storage/Container.h"

class Disk : public Container
    {
    friend class Storage;

    public:
	Disk( const string& Name );
	virtual ~Disk();
	unsigned Cylinders() const { return cylinders; }
	unsigned Heads() const { return heads; }
	unsigned Sectors() const { return sectors; }
	static CType const StaticType() { return DISK; }

    protected:
	unsigned cylinders;
	unsigned heads;
	unsigned sectors;
    };

#endif
