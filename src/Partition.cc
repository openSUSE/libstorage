#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/Partition.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Disk.h"

Partition::Partition( const Disk& d, unsigned PNr, unsigned long long SizeK,
                      unsigned long Start, unsigned long CSize, PType Type,
		      const string& PartedStart, unsigned Id, bool Boot ) : 
        Volume( d, PNr, SizeK )
    {
    cyl_start = Start;
    cyl_size = CSize;
    boot = Boot;
    id = Id;
    type = Type;
    parted_start = PartedStart;
    y2milestone( "constructed partition %s on disk %s", dev.c_str(),
                 cont->Name().c_str() );
    }

Partition::Partition( const Disk& d, const string& Data ) : 
        Volume( d, 0, 0 )
    {
    string ts, rs;
    istringstream i( Data );
    i >> nr >> dev >> size_k >> major >> minor >> cyl_start >> cyl_size >>
	 hex >> id >> dec >> ts >> rs;
    name = dec_string(nr);
    if( ts == "extended" )
	type = EXTENDED;
    else if( ts == "logical" )
	type = LOGICAL;
    else 
	type = PRIMARY;
    if( rs == "boot" )
	boot = true;
    else
	boot = false;
    y2milestone( "constructed partition %s on disk %s", dev.c_str(),
                 cont->Name().c_str() );
    }

ostream& Partition::LogData( ostream& file ) const
    {
    file << nr << " " << dev << " " << size_k << " " <<  major << " " 
         << minor << " ";
    file << cyl_start << " " << cyl_size << " " << hex << id << dec;
    if( type == LOGICAL )
	file << " logical";
    else if( type == EXTENDED )
	file << " extended";
    else 
	file << " primary";
    if( boot )
	file << " boot";
    return( file );
    }

Partition::~Partition()
    {
    y2milestone( "destructed partition %s", dev.c_str() );
    }
