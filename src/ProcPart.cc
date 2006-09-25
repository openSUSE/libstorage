// Maintainer: fehr@suse.de
/*
  Textdomain    "storage"
*/

#include <sstream>

#include "y2storage/AppUtil.h"
#include "y2storage/Regex.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/ProcPart.h"

using namespace std;
using namespace storage;

ProcPart::ProcPart() : AsciiFile( "/proc/partitions" )
    {
    y2mil( "numLines " << numLines() );
    for( unsigned i=0; i<numLines(); i++ )
	{
	y2mil( "line " << (i+1) << " is \"" << (*this)[i] << "\"" );
	string tmp = extractNthWord( 3, (*this)[i] );
	if( !tmp.empty() && tmp!="name" )
	    {
	    co[tmp] = i;
	    }
	}
    }

bool 
ProcPart::getInfo( const string& Dev, unsigned long long& SizeK,
		   unsigned long& Major, unsigned long& Minor ) const
    {
    bool ret = false;
    map<string,int>::const_iterator i = co.find( devName(Dev) );
    if( i != co.end() )
	{
	extractNthWord( 0, (*this)[i->second] ) >> Major;
	extractNthWord( 1, (*this)[i->second] ) >> Minor;
	extractNthWord( 2, (*this)[i->second] ) >> SizeK;
	ret = true;
	}
    return( ret );
    }

bool 
ProcPart::getSize( const string& Dev, unsigned long long& SizeK ) const
    {
    bool ret = false;
    map<string,int>::const_iterator i = co.find( devName(Dev) );
    if( i != co.end() )
	{
	extractNthWord( 2, (*this)[i->second] ) >> SizeK;
	ret = true;
	}
    y2mil( "dev:" << Dev << " ret:" << ret << " Size:" << (ret?SizeK:0) );
    return( ret );
    }

string 
ProcPart::devName( const string& Dev )
    {
    return( undevDevice( Dev ));
    }

list<string>  
ProcPart::getMatchingEntries( const string& regexp ) const
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

