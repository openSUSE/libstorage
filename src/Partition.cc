#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/Partition.h"
#include "y2storage/Container.h"

Partition::Partition( const Container& d, unsigned PNr ) : Volume( d, PNr )
    {
    cyl_start = 0;
    cyl_size = 1;
    y2milestone( "constructed partition %s on disk %s", dev.c_str(),
                 cont->Name().c_str() );
    }

Partition::~Partition()
    {
    y2milestone( "destructed partition %s", dev.c_str() );
    }

