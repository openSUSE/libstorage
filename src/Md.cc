#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/Md.h"
#include "y2storage/Container.h"

Md::Md( const Container& d, unsigned PNr, MdType Type ) : Volume( d, PNr, 0 )
    {
    y2milestone( "constructed md %s on container %s", dev.c_str(),
                 cont->name().c_str() );
    md_type = Type;
    if( d.type() != Container::MD )
	y2error( "constructed md with wrong container" );
    }

Md::~Md()
    {
    y2milestone( "destructed md %s", dev.c_str() );
    }

string Md::md_names[] = { "raid0", "raid1", "raid5", "multipath" };

