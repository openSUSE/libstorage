#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/Disk.h"
#include "y2storage/Partition.h"

Disk::Disk( const string& Name ) : Container(Name,StaticType())
    {
    if( name.find( "hdb" ) != string::npos )
	deleted = true;
    if( name == "sda" )
	{
	cylinders = heads = sectors = 100;
	vols.push_back( new Partition( *this, 1 ));
	vols.push_back( new Partition( *this, 2 ));
	vols.push_back( new Partition( *this, 3 ));
	vols.push_back( new Partition( *this, 4 ));
	}
    if( name == "hdb" )
	{
	cylinders = heads = sectors = 200;
	vols.push_back( new Partition( *this, 1 ));
	vols.push_back( new Partition( *this, 5 ));
	vols.push_back( new Partition( *this, 6 ));
	vols.push_back( new Partition( *this, 7 ));
	}
    if( name == "hdc" )
	{
	cylinders = heads = sectors = 300;
	vols.push_back( new Partition( *this, 1 ));
	}
    y2milestone( "constructed disk %s", device.c_str() );
    }

Disk::~Disk()
    {
    y2milestone( "destructed disk %s", device.c_str() );
    }


