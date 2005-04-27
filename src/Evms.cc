#include <iostream> 

#include "y2storage/Evms.h"
#include "y2storage/AppUtil.h"
#include "y2storage/EvmsVol.h"

Evms::Evms( Storage * const s, const string& Name ) : 
    Container(s,Name,staticType())
    {
    dev = "/dev/evms";
    if( nm.length()>0 )
	{
	dev += "/" + nm;
	is_container = true;
	}
    else
	is_container = false;
    if( nm == "" )
	{
	pe_size = 0;
	num_pv = 1;
	addToList( new EvmsVol( *this, "sda1" ));
	addToList( new EvmsVol( *this, "sda2" ));
	addToList( new EvmsVol( *this, "sda3" ));
	}
    else if( nm == "vg1" )
	{
	pe_size = 16*1024*1024;
	num_pv = 10;
	addToList( new EvmsVol( *this, "lv1" ));
	addToList( new EvmsVol( *this, "lv2", 2 ));
	addToList( new EvmsVol( *this, "lv3", 3 ));
	addToList( new EvmsVol( *this, "lv4", 4 ));
	addToList( new EvmsVol( *this, "lv5", 5 ));
	addToList( new EvmsVol( *this, "lv6", 6 ));
	}
    else if( nm == "vg2" )
	{
	num_pv = 5;
	pe_size = 256*1024*1024;
	addToList( new EvmsVol( *this, "lv_va2", 2 ));
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


