#include <sstream>

#include "y2storage/Evms.h"
#include "y2storage/AppUtil.h"
#include "y2storage/EvmsVol.h"

EvmsVol::EvmsVol( const Evms& d, const string& name, unsigned Stripes ) 
    : Volume( d, name, 0 )
    {
    stripe = Stripes;
    compat = true;
    dev = d.device() + "/" + name;
    y2milestone( "constructed evms vol %s on co %s", dev.c_str(),
                 cont->name().c_str() );
    }

EvmsVol::~EvmsVol()
    {
    y2milestone( "destructed evms vol %s", dev.c_str() );
    }

