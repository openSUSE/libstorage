#include <iostream>

#include <ycp/y2log.h>

#include "y2storage/Container.h"
#include "y2storage/Md.h"
#include "y2storage/Loop.h"

Container::Container( const Storage * const s, const string& Name, CType t ) :
    sto(s), nm(Name)
    {
    dltd = false;
    rdonly = false;
    dev = "/dev/" + nm;
    typ = t;
    if( nm == "md" )
	{
	addToList( new Md( *this, 0, RAID0 ));
        addToList( new Md( *this, 2, RAID5 ));
	}
    else if( nm == "loop" )
	{
	addToList( new Loop( *this, 0, "loop_file" ));
        addToList( new Loop( *this, 1, "another-file" ));
        addToList( new Loop( *this, 2, "last_file" ));
	}
    y2milestone( "constructed cont %s", nm.c_str() );
    }

Container::~Container()
    {
    for( PlainIterator i=begin(); i!=end(); i++ )
	{
	delete( *i );
	}
    y2milestone( "destructed cont %s", dev.c_str() );
    }

string Container::type_names[] = { "UNKNOWN", "DISK", "MD", "LOOP", "LVM", "EVMS" };

