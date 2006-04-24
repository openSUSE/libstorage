// Maintainer: fehr@suse.de
/*
  Textdomain    "storage"
*/

#include <sstream>

#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/ProcMounts.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/Storage.h"

using namespace std;
using namespace storage;

ProcMounts::ProcMounts( Storage * const sto ) 
    {
    map<string,string> by_label;
    map<string,string> by_uuid;
    getFindRevMap( "/dev/disk/by-label", by_label );
    getFindRevMap( "/dev/disk/by-uuid", by_uuid );
    ifstream mounts( "/proc/mounts" );
    string line;
    getline( mounts, line );
    while( mounts.good() )
	{
	y2mil( "line:\"" << line << "\"" );
	string dev = extractNthWord( 0, line );
	if( dev.find( "/by-label/" ) != string::npos )
	    {
	    dev = dev.substr( dev.rfind( "/" )+1 );
	    y2mil( "dev:" << dev );
	    if( !by_label[dev].empty() )
		{
		dev = by_label[dev];
		normalizeDevice( dev );
		y2mil( "dev:" << dev );
		}
	    }
	else if( dev.find( "/by-uuid/" ) != string::npos )
	    {
	    dev = dev.substr( dev.rfind( "/" )+1 );
	    y2mil( "dev:" << dev );
	    if( !by_uuid[dev].empty() )
		{
		dev = by_uuid[dev];
		normalizeDevice( dev );
		y2mil( "dev:" << dev );
		}
	    }
	if( dev!= "rootfs" && dev!="/dev/root" )
	    {
	    co[dev] = extractNthWord( 1, line );
	    }
	getline( mounts, line );
	}
    SystemCmd mt( "mount | grep \" / \"" );
    if( mt.numLines()>0 )
	{
	string dev = extractNthWord( 0, *mt.getLine(0));
	if( !dev.empty() && dev[0]!='/' )
	    {
	    dev = sto->findNormalDevice( dev );
	    }
	co[dev] = "/";
	}
    mounts.close();
    mounts.clear();
    mounts.open( "/proc/swaps" );
    getline( mounts, line );
    getline( mounts, line );
    while( mounts.good() )
	{
	y2mil( "line:\"" << line << "\"" );
	string::size_type pos;
	string dev = extractNthWord( 0, line );
	if( (pos=dev.find( "\\040(deleted)" ))!=string::npos )
	    {
	    y2mil( "dev:" << dev );
	    dev.erase( pos );
	    }
	co[dev] = "swap";
	getline( mounts, line );
	}
    y2mil( "co:" << co );
    }

string 
ProcMounts::getMount( const string& dev ) const
    {
    string ret;
    map<string,string>::const_iterator i = co.find( dev );
    if( i!=co.end() )
	ret = i->second;
    return( ret );
    }

string
ProcMounts::getMount( const list<string>& dl ) const
    {
    string ret;
    list<string>::const_iterator i = dl.begin();
    while( ret.empty() && i!=dl.end() )
	{
	ret = getMount( *i );
	++i;
	}
    return( ret );
    }

map<string,string>
ProcMounts::allMounts() const
    {
    map<string,string> ret;
    map<string,string>::const_iterator i = co.begin();
    for( map<string,string>::const_iterator i = co.begin(); i!=co.end(); ++i )
	{
	ret[i->second] = i->first;
	}
    return( ret );
    }


