#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/Loop.h"
#include "y2storage/Container.h"

Loop::Loop( const Container& d, unsigned PNr, const string& LoopFile ) : 
    Volume( d, PNr, 0 )
    {
    y2milestone( "constructed loop %s on container %s", dev.c_str(),
                 cont->Name().c_str() );
    loop_file = LoopFile;
    if( d.Type() != Container::LOOP )
	y2error( "constructed loop with wrong container" );
    }

Loop::~Loop()
    {
    y2milestone( "destructed loop %s", dev.c_str() );
    }

