#ifndef PARTITION_H
#define PARTITION_H

using namespace std;

#include "y2storage/Volume.h"

class Disk;

class Partition : public Volume
    {
    public:
	typedef enum { ID_DOS=0x0c, ID_NTFS=0x07, ID_EXTENDED=0x0f,
	               ID_LINUX=0x83, ID_SWAP=0x82, ID_LVM=0x8e, 
		       ID_MD=0xfd } IdNum;
	typedef enum { PRIMARY, EXTENDED, LOGICAL } PType;
	Partition( const Disk& d, unsigned Pnr, unsigned long long SizeK,
	           unsigned long Start, unsigned long CSize, PType Type,
		   const string& parted_start, unsigned id=ID_LINUX, 
		   bool Boot=false );
	Partition( const Disk& d, const string& Data );
	virtual ~Partition();

	unsigned cylStart() const { return cyl_start; }
	unsigned cylSize() const { return cyl_size; }
	bool boot() const { return bootflag; }
	unsigned id() const { return idt; }
	PType type() const { return typ; }
	ostream& logData( ostream& file ) const;
	const string& partedStart() const { return parted_start; }
	friend ostream& operator<< (ostream& s, const Partition &p );

    protected:
	unsigned long cyl_start;
	unsigned long cyl_size;
	bool bootflag;
	PType typ;
	unsigned idt;
	string parted_start;

    };


inline ostream& operator<< (ostream& s, const Partition &p )
    {
    s << "Partition " << Volume(p) 
      << " Start:" << p.cyl_start
      << " CylNum:" << p.cyl_size
      << " Id:" << std::hex << p.idt << std::dec;
    if( p.typ!=Partition::PRIMARY )
      s << ((p.typ==Partition::LOGICAL)?" logical":" extended");
    if( p.bootflag )
      s << " boot";
    return( s );
    }

#endif
