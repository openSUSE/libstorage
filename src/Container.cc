#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/Container.h"
#include "y2storage/Md.h"
#include "y2storage/Loop.h"

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
    else if( name == "loop" )
	{
	Vols.push_back( new Loop( *this, 0, "loop_file" ));
        Vols.push_back( new Loop( *this, 1, "another-file" ));
        Vols.push_back( new Loop( *this, 2, "last_file" ));
	}
    y2milestone( "constructed cont %s", name.c_str() );
    }

Container::~Container()
    {
    y2milestone( "destructed cont %s", device.c_str() );
    }


