#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/Volume.h"
#include "y2storage/Container.h"

Volume::Volume( const Container& d, unsigned PNr ) : cont(&d), deleted(false)
    {
    numeric = true;
    nr = PNr;
    Init();
    y2milestone( "constructed volume %s on disk %s", dev.c_str(),
                 cont->Name().c_str() );
    }

Volume::Volume( const Container& c, const string& Name ) : cont(&c)
    {
    numeric = false;
    name = Name;
    Init();
    y2milestone( "constructed volume \"%s\" on disk %s", dev.c_str(),
                 cont->Name().c_str() );
    }

Volume::~Volume()
    {
    y2milestone( "destructed volume %s", dev.c_str() );
    }

void Volume::Init()
    {
    deleted = false;
    std::ostringstream Buf_Ci;
    if( numeric )
	Buf_Ci << "/dev/" << cont->Name() << nr;
    else
	Buf_Ci << "/dev/" << cont->Name() << "/" << name;
    dev = Buf_Ci.str();
    if( numeric )
	{
	Buf_Ci.str("");
	Buf_Ci << cont->Name() << nr;
	name = Buf_Ci.str();
	}
    else
	nr = 0;
    }


