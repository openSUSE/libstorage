#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/Partition.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Disk.h"

Partition::Partition( const Disk& d, unsigned PNr, unsigned long long SizeK,
                      unsigned long Start, unsigned long CSize,
		      PartitionType Type, const string& PartedStart,
		      unsigned Id, bool Boot )
    : Volume( d, PNr, SizeK )
    {
    cyl_start = Start;
    cyl_size = CSize;
    bootflag = Boot;
    idt = Id;
    typ = Type;
    parted_start = PartedStart;
    y2milestone( "constructed partition %s on disk %s", dev.c_str(),
                 cont->name().c_str() );
    }

Partition::Partition( const Disk& d, const string& Data ) :
        Volume( d, 0, 0 )
    {
    string ts, rs;
    istringstream i( Data );
    i >> num >> dev >> size_k >> mjr >> mnr >> cyl_start >> cyl_size >>
	 hex >> idt >> dec >> ts >> rs;
    nm = decString(num);
    if( ts == "extended" )
	typ = EXTENDED;
    else if( ts == "logical" )
	typ = LOGICAL;
    else
	typ = PRIMARY;
    if( rs == "boot" )
	bootflag = true;
    else
	bootflag = false;
    y2milestone( "constructed partition %s on disk %s", dev.c_str(),
                 cont->name().c_str() );
    }

ostream& Partition::logData( ostream& file ) const
    {
    file << num << " " << dev << " " << size_k << " " <<  mjr << " "
         << mnr << " ";
    file << cyl_start << " " << cyl_size << " " << hex << idt << dec;
    if( typ == LOGICAL )
	file << " logical";
    else if( typ == EXTENDED )
	file << " extended";
    else
	file << " primary";
    if( bootflag )
	file << " boot";
    return( file );
    }

Partition::~Partition()
    {
    y2milestone( "destructed partition %s", dev.c_str() );
    }
