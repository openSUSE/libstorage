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

LvmVg::LvmVg( Storage * const s, const string& file, bool ) :
    Container(s,"",staticType())
    {
    }

LvmVg::~LvmVg()
    {
    y2milestone( "destructed lvm vg %s", dev.c_str() );
    }

int LvmVg::doCreate( Volume* v ) { return( 0 ); }
int LvmVg::doRemove( Volume* v ) { return( 0 ); }
int LvmVg::doResize( Volume* v ) { return( 0 ); }

int
LvmVg::commitChanges( CommitStage stage )
    {
    return( 0 );
    }

void LvmVg::getCommitActions( list<commitAction*>& l ) const
    {
    }

int
LvmVg::checkResize( Volume* v, unsigned long long newSizeK ) const
    {
    return( false );
    }

void LvmVg::logData( const string& Dir ) {;}

