#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/Partition.h"
#include "y2storage/Disk.h"

Partition::Partition( const Disk& d, unsigned PNr ) : Volume( d, PNr )
    {
    cyl_start = PNr;
    cyl_size = PNr*PNr;
    y2milestone( "constructed partition %s on disk %s", dev.c_str(),
                 cont->Name().c_str() );
    }

Partition::~Partition()
    {
    y2milestone( "destructed partition %s", dev.c_str() );
    }

