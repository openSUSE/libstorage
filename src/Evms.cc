#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/Evms.h"
#include "y2storage/EvmsVol.h"

Evms::Evms( const Storage * const s, const string& Name ) : 
    Container(s,Name,StaticType())
    {
    dev = "/dev/evms";
    if( name.length()>0 )
	{
	dev += "/" + name;
	is_container = true;
	}
    else
	is_container = false;
    if( name == "" )
	{
	pe_size = 0;
	num_pv = 1;
	AddToList( new EvmsVol( *this, "sda1" ));
	AddToList( new EvmsVol( *this, "sda2" ));
	AddToList( new EvmsVol( *this, "sda3" ));
	}
    else if( name == "vg1" )
	{
	pe_size = 16*1024*1024;
	num_pv = 10;
	AddToList( new EvmsVol( *this, "lv1" ));
	AddToList( new EvmsVol( *this, "lv2", 2 ));
	AddToList( new EvmsVol( *this, "lv3", 3 ));
	AddToList( new EvmsVol( *this, "lv4", 4 ));
	AddToList( new EvmsVol( *this, "lv5", 5 ));
	AddToList( new EvmsVol( *this, "lv6", 6 ));
	}
    else if( name == "vg2" )
	{
	num_pv = 5;
	pe_size = 256*1024*1024;
	AddToList( new EvmsVol( *this, "lv_va2", 2 ));
	}
    else
	{
	num_pv = 1;
	pe_size = 4*1024*1024;
	}
    y2milestone( "constructed evms co %s", dev.c_str() );
    }

Evms::~Evms()
    {
    y2milestone( "destructed evmc co %s", dev.c_str() );
    }


