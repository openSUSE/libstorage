#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/LvmLv.h"
#include "y2storage/LvmVg.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"

LvmLv::LvmLv( const LvmVg& d, const string& name, unsigned long le,
	      const string& uuid, const string& stat, const string& alloc ) :
	Volume( d, name, 0 )
    {
    init();
    vol_uuid = uuid;
    status = stat;
    allocation = alloc;
    num_le = le;
    calcSize();
    SystemCmd c( "dmsetup table " + cont->name() + "-" + name );
    if( c.numLines()>0 )
	{
	string line = *c.getLine(0);
	if( extractNthWord( 2, line )=="striped" )
	    extractNthWord( 3, line ) >> stripe;
	}
    if( majorNumber()>0 )
	{
	alt_names.push_back( "/dev/dm-" + decString(minorNumber()) );
	}
    alt_names.push_back( "/dev/mapper/" + cont->name() + "-" + name );
    y2milestone( "constructed lvm lv %s on vg %s", dev.c_str(),
                 cont->name().c_str() );
    }

LvmLv::~LvmLv()
    {
    y2milestone( "destructed lvm lv %s", dev.c_str() );
    }

void LvmLv::init()
    {
    num_le = 0;
    stripe = 1;
    }

const LvmVg* const LvmLv::vg() const
    { 
    return(dynamic_cast<const LvmVg* const>(cont));
    }
	        
void LvmLv::calcSize()
    {
    setSize( num_le*vg()->peSize() );
    }

