#include <iostream> 

#include <ycp/y2log.h>

#include "y2storage/LvmVg.h"
#include "y2storage/LvmLv.h"

LvmVg::LvmVg( Storage * const s, const string& Name ) :
    Container(s,Name,staticType())
    {
    if( nm == "system" )
	{
	pe_size = 4*1024*1024;
	num_pv = 1;
	addToList( new LvmLv( *this, "usr" ));
	addToList( new LvmLv( *this, "var" ));
	addToList( new LvmLv( *this, "scratch" ));
	}
    else if( nm == "vg1" )
	{
	pe_size = 16*1024*1024;
	num_pv = 10;
	addToList( new LvmLv( *this, "lv1" ));
	addToList( new LvmLv( *this, "lv2", 2 ));
	addToList( new LvmLv( *this, "lv3", 3 ));
	addToList( new LvmLv( *this, "lv4", 4 ));
	addToList( new LvmLv( *this, "lv5", 5 ));
	addToList( new LvmLv( *this, "lv6", 6 ));
	}
    else if( nm == "vg2" )
	{
	num_pv = 5;
	pe_size = 256*1024*1024;
	addToList( new LvmLv( *this, "lv_va2", 2 ));
	}
    else
	{
	num_pv = 1;
	pe_size = 4*1024*1024;
	}
    y2milestone( "constructed lvm vg %s", dev.c_str() );
    }

LvmVg::~LvmVg()
    {
    y2milestone( "destructed lvm vg %s", dev.c_str() );
    }


