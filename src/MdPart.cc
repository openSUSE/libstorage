#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/MdPart.h"
#include "y2storage/Softraid.h"

MdPart::MdPart( const Softraid& d, unsigned PNr, MdType Type ) : Volume( d, PNr )
    {
    md_type = Type;
    y2milestone( "constructed md %s on disk %s", dev.c_str(),
                 cont->Name().c_str() );
    }

MdPart::~MdPart()
    {
    y2milestone( "destructed md %s", dev.c_str() );
    }

string MdPart::md_names[] = { "raid0", "raid1", "raid5", "multipath" };

