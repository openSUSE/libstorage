#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/Container.h"
#include "y2storage/Md.h"

Container::Container( const string& Name, CType t ) : name(Name)
    {
    deleted = false;
    device = "/dev/" + name;
    type = t;
    if( name == "md" )
	{
	Vols.push_back( new Md( *this, 0, Md::RAID0 ));
        Vols.push_back( new Md( *this, 2, Md::RAID5 ));
	}
    y2milestone( "constructed cont %s", name.c_str() );
    }

Container::~Container()
    {
    y2milestone( "destructed cont %s", device.c_str() );
    }


