#ifndef DISK_H
#define DISK_H

#include <list>

using namespace std;

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
	friend inline ostream& operator<< (ostream&, const Disk& );

    protected:
	unsigned cylinders;
	unsigned heads;
	unsigned sectors;
    };

inline ostream& operator<< (ostream& s, const Disk& d )
    {
    d.print(s);
    s << " Cyl:" << d.cylinders
      << " Head:" << d.heads
      << " Sect:" << d.sectors;
    return( s );
    }


#endif
