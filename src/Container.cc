#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/Container.h"

Container::Container( const string& Name ) : name(Name)
    {
    deleted = false;
    type = StaticType();
    /*
    if( name.find( "hdb" ) != string::npos )
	deleted = true;
    device = "/dev/" + name;
    if( name == "sda" )
	{
	Vols.push_back( Volume( *this, 1 ));
	Vols.push_back( Volume( *this, 2 ));
	Vols.push_back( Volume( *this, 3 ));
	Vols.push_back( Volume( *this, 4 ));
	}
    if( name == "hdb" )
	{
	Vols.push_back( Volume( *this, 1 ));
	Vols.push_back( Volume( *this, 5 ));
	Vols.push_back( Volume( *this, 6 ));
	Vols.push_back( Volume( *this, 7 ));
	}
    if( name == "hdc" )
	{
	Vols.push_back( Volume( *this, 1 ));
	}
    */
    y2milestone( "constructed cont %s", name.c_str() );
    }

Container::~Container()
    {
    y2milestone( "destructed cont %s", device.c_str() );
    }


