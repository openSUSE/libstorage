// Maintainer: fehr@suse.de

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
    map<string,int>::const_iterator i = co.find( DevName(Dev) );
    if( i != co.end() )
	{
	ExtractNthWord( 0, (*this)[i->second] ) >> Major;
	ExtractNthWord( 1, (*this)[i->second] ) >> Minor;
	ExtractNthWord( 2, (*this)[i->second] ) >> SizeK;
	ret = true;
	}
    return( ret );
    }

bool 
ProcPart::GetSize( const string& Dev, unsigned long long& SizeK ) const
    {
    bool ret = false;
    map<string,int>::const_iterator i = co.find( DevName(Dev) );
    if( i != co.end() )
	{
	ExtractNthWord( 2, (*this)[i->second] ) >> SizeK;
	ret = true;
	}
    return( ret );
    }

string 
ProcPart::DevName( const string& Dev )
    {
    string name( Dev );
    if( name.find( "/dev/" )==0 )
	name.erase( 0, 5 );
    return( name );
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

