#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/Softraid.h"
#include "y2storage/MdPart.h"

Softraid::Softraid() : Container("md")
    {
    type = StaticType();
    device = "/dev/md";
    Vols.push_back( new MdPart( *this, 0, MdPart::RAID0 ));
    Vols.push_back( new MdPart( *this, 2, MdPart::RAID5 ));
    y2milestone( "constructed softraid %s", device.c_str() );
    }

Softraid::~Softraid()
    {
    y2milestone( "destructed softraid %s", device.c_str() );
    }


