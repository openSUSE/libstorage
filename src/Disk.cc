#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/Disk.h"
#include "y2storage/Partition.h"

Disk::Disk( const string& Name ) : Container(Name)
    {
    type = REAL_DISK;
    if( name.find( "hdb" ) != string::npos )
	deleted = true;
    if( name == "sda" )
	{
	Vols.push_back( new Partition( *this, 1 ));
	Vols.push_back( new Partition( *this, 2 ));
	Vols.push_back( new Partition( *this, 3 ));
	Vols.push_back( new Partition( *this, 4 ));
	}
    if( name == "hdb" )
	{
	Vols.push_back( new Partition( *this, 1 ));
	Vols.push_back( new Partition( *this, 5 ));
	Vols.push_back( new Partition( *this, 6 ));
	Vols.push_back( new Partition( *this, 7 ));
	}
    if( name == "hdc" )
	{
	Vols.push_back( new Partition( *this, 1 ));
	}
    y2milestone( "constructed disk %s", device.c_str() );
    }

Disk::~Disk()
    {
    y2milestone( "destructed disk %s", device.c_str() );
    }


