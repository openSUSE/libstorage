#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/Evms.h"
#include "y2storage/EvmsVol.h"

Evms::Evms( const string& Name ) : Container(Name,StaticType())
    {
    device = "/dev/evms";
    if( name.length()>0 )
	{
	device += "/" + name;
	}
    if( name == "" )
	{
	pe_size = 0;
	num_pv = 1;
	Vols.push_back( new EvmsVol( *this, "sda1" ));
	Vols.push_back( new EvmsVol( *this, "sda2" ));
	Vols.push_back( new EvmsVol( *this, "sda3" ));
	}
    else if( name == "vg1" )
	{
	pe_size = 16*1024*1024;
	num_pv = 10;
	Vols.push_back( new EvmsVol( *this, "lv1" ));
	Vols.push_back( new EvmsVol( *this, "lv2", 2 ));
	Vols.push_back( new EvmsVol( *this, "lv3", 3 ));
	Vols.push_back( new EvmsVol( *this, "lv4", 4 ));
	Vols.push_back( new EvmsVol( *this, "lv5", 5 ));
	Vols.push_back( new EvmsVol( *this, "lv6", 6 ));
	}
    else if( name == "vg2" )
	{
	num_pv = 5;
	pe_size = 256*1024*1024;
	Vols.push_back( new EvmsVol( *this, "lv_va2", 2 ));
	}
    else
	{
	num_pv = 1;
	pe_size = 4*1024*1024;
	}
    y2milestone( "constructed evms co %s", device.c_str() );
    }

Evms::~Evms()
    {
    y2milestone( "destructed evmc co %s", device.c_str() );
    }


