#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/LvmLv.h"
#include "y2storage/LvmVg.h"

LvmLv::LvmLv( const LvmVg& d, const string& name, unsigned Stripes ) : Volume( d, name, 0 )
    {
    stripes = Stripes;
    y2milestone( "constructed lvm lv %s on vg %s", dev.c_str(),
                 cont->Name().c_str() );
    }

LvmLv::~LvmLv()
    {
    y2milestone( "destructed lvm lv %s", dev.c_str() );
    }

