// Maintainer: fehr@suse.de

#include <features.h>
#include <sys/stat.h>
#include <ycp/y2log.h>

#include <sstream>

#include "y2storage/AppUtil.h"
#include "y2storage/Regex.h"
#include "y2storage/ProcPart.h"

ProcPart::ProcPart() : AsciiFile( "/proc/partitions" )
    {
    for( int i=0; i<NumLines(); i++ )
	{
	string tmp = ExtractNthWord( 3, (*this)[i] );
	if( tmp.size()>0 && tmp!="name" )
	    {
	    co[tmp] = i;
	    }
	}
    }

bool 
ProcPart::GetInfo( const string& Dev, unsigned long long& SizeK,
		   unsigned long& Major, unsigned long& Minor ) const
    {
    bool ret = false;
    string name( Dev );
    if( name.find( "/dev/" )==0 )
	name.erase( 0, 5 );
    map<string,int>::const_iterator i = co.find( name );
    SizeK = 0;
    if( i != co.end() )
	{
	ExtractNthWord( 0, (*this)[i->second] ) >> Major;
	ExtractNthWord( 1, (*this)[i->second] ) >> Minor;
	ExtractNthWord( 2, (*this)[i->second] ) >> SizeK;
	ret = true;
	}
    else
	{
	string dev( Dev );
	if( dev.find( "/dev/" )!=0 )
	    dev = "/dev/"+ dev;
	struct stat sbuf;
	if( stat( dev.c_str(), &sbuf )==0 )
	    {
	    Minor = gnu_dev_minor( sbuf.st_rdev );
	    Major = gnu_dev_major( sbuf.st_rdev );
	    ret = true;
	    }
	}
    return( ret );
    }

list<string>  
ProcPart::GetMatchingEntries( const string& regexp ) const
    {
    Regex reg( "^" + regexp + "$" );
    list<string> ret;
    for( map<string,int>::const_iterator i=co.begin(); i!=co.end(); i++ )
	{
	if( reg.match( i->first ))
	    {
	    ret.push_back( i->first );
	    }
	}
    return( ret );
    }

