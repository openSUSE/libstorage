#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/Partition.h"
#include "y2storage/Disk.h"
#include "y2storage/Storage.h"

Disk::Disk( const Storage * const s, const string& Name ) : 
    Container(s,Name,StaticType())
    {
    if( name.find( "hdb" ) != string::npos )
	deleted = true;
    if( name == "sda" )
	{
	cylinders = heads = sectors = 100;
	AddToList( new Partition( *this, 1 ));
	AddToList( new Partition( *this, 2 ));
	AddToList( new Partition( *this, 3 ));
	AddToList( new Partition( *this, 4 ));
	}
    if( name == "hdb" )
	{
	cylinders = heads = sectors = 200;
	AddToList( new Partition( *this, 1 ));
	AddToList( new Partition( *this, 5 ));
	AddToList( new Partition( *this, 6 ));
	AddToList( new Partition( *this, 7 ));
	}
    if( name == "hdc" )
	{
	cylinders = heads = sectors = 300;
	AddToList( new Partition( *this, 1 ));
	}
    y2milestone( "constructed disk %s", device.c_str() );
    }

Disk::~Disk()
    {
    y2milestone( "destructed disk %s", device.c_str() );
    }


