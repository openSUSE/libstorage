#include <sstream>

#include <features.h>
#include <sys/stat.h>
#include <ycp/y2log.h>

#include "y2storage/Volume.h"
#include "y2storage/Disk.h"
#include "y2storage/Container.h"

Volume::Volume( const Container& d, unsigned PNr, unsigned long long SizeK ) 
    : cont(&d), deleted(false)
    {
    numeric = true;
    nr = PNr;
    size_k = SizeK;
    Init();
    y2milestone( "constructed volume %s on disk %s", dev.c_str(),
                 cont->Name().c_str() );
    }

Volume::Volume( const Container& c, const string& Name, unsigned long long SizeK ) : cont(&c)
    {
    numeric = false;
    name = Name;
    size_k = SizeK;
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
	Buf_Ci << cont->Device() << (Disk::NeedP(cont->Device())?"p":"") << nr;
    else
	Buf_Ci << cont->Device() << "/" << name;
    dev = Buf_Ci.str();
    major = minor = 0;
    GetMajorMinor( dev, major, minor );
    if( numeric )
	{
	Buf_Ci.str("");
	Buf_Ci << cont->Name() << nr;
	name = Buf_Ci.str();
	}
    else
	nr = 0;
    }

bool Volume::operator== ( const Volume& rhs ) const
    {
    return( (*cont)==(*rhs.cont) && 
            name == rhs.name && 
	    deleted == rhs.deleted ); 
    }

bool Volume::operator< ( const Volume& rhs ) const
    {
    if( *cont != *rhs.cont )
	return( *cont<*rhs.cont );
    else if( name != rhs.name )
	{
	if( numeric )
	    return( nr<rhs.nr );
	else
	    return( name<rhs.name );
	}
    else
	return( !deleted );
    }

bool Volume::GetMajorMinor( const string& device,
			    unsigned long& Major, unsigned long& Minor )
    {
    bool ret = false;
    string dev( device );
    if( dev.find( "/dev/" )!=0 )
	dev = "/dev/"+ dev;
    struct stat sbuf;
    if( stat( dev.c_str(), &sbuf )==0 )
	{
	Minor = gnu_dev_minor( sbuf.st_rdev );
	Major = gnu_dev_major( sbuf.st_rdev );
	ret = true;
	}
    return( ret );
    }



