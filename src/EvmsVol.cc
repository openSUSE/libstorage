#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/Evms.h"
#include "y2storage/EvmsVol.h"

EvmsVol::EvmsVol( const Evms& d, const string& name, unsigned Stripes ) : Volume( d, name )
    {
    stripes = Stripes;
    dev = d.Device() + "/" + name;
    y2milestone( "constructed evms vol %s on co %s", dev.c_str(),
                 cont->Name().c_str() );
    }

EvmsVol::~EvmsVol()
    {
    y2milestone( "destructed evms vol %s", dev.c_str() );
    }

