#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/LvmVg.h"
#include "y2storage/LvmLv.h"

LvmVg::LvmVg( const string& Name ) : Container(Name,StaticType())
    {
    if( name.find( "del" ) != string::npos )
	deleted = true;
    if( name == "system" )
	{
	pe_size = 4*1024*1024;
	num_pv = 1;
	Vols.push_back( new LvmLv( *this, "usr" ));
	Vols.push_back( new LvmLv( *this, "var" ));
	Vols.push_back( new LvmLv( *this, "scratch" ));
	}
    else if( name == "vg1" )
	{
	pe_size = 16*1024*1024;
	num_pv = 10;
	Vols.push_back( new LvmLv( *this, "lv1" ));
	Vols.push_back( new LvmLv( *this, "lv2", 2 ));
	Vols.push_back( new LvmLv( *this, "lv3", 3 ));
	Vols.push_back( new LvmLv( *this, "lv4", 4 ));
	Vols.push_back( new LvmLv( *this, "lv5", 5 ));
	Vols.push_back( new LvmLv( *this, "lv6", 6 ));
	}
    else if( name == "vg2" )
	{
	num_pv = 5;
	pe_size = 256*1024*1024;
	Vols.push_back( new LvmLv( *this, "lv_va2", 2 ));
	}
    else
	{
	num_pv = 1;
	pe_size = 4*1024*1024;
	}
    y2milestone( "constructed lvm vg %s", device.c_str() );
    }

LvmVg::~LvmVg()
    {
    y2milestone( "destructed lvm vg %s", device.c_str() );
    }


