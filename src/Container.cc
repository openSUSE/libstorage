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
	vols.push_back( new Md( *this, 0, Md::RAID0 ));
        vols.push_back( new Md( *this, 2, Md::RAID5 ));
	}
    else if( name == "loop" )
	{
	vols.push_back( new Loop( *this, 0, "loop_file" ));
        vols.push_back( new Loop( *this, 1, "another-file" ));
        vols.push_back( new Loop( *this, 2, "last_file" ));
	}
    y2milestone( "constructed cont %s", name.c_str() );
    }

Container::~Container()
    {
    for( PlainIterator i=begin(); i!=end(); i++ )
	{
	delete( *i );
	}
    y2milestone( "destructed cont %s", device.c_str() );
    }

string Container::type_names[] = { "UNKNOWN", "DISK", "MD", "LOOP", "LVM", "EVMS" };

