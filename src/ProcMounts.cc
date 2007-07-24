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
    SystemCmd mt( "mount" );
    while( mounts.good() )
	{
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
	bool skip = false;
	string dir = extractNthWord( 1, line );
	mt.select( (string)" on "+dir+' ' );
	if( mt.numLines(true)>0 )
	    {
	    list<string> sl = splitString( *mt.getLine(0,true) );
	    y2mil( "sl:" << sl );
	    if( sl.size()>=6 )
		{
		list<string>::const_iterator i=sl.begin();
		++i;
		++i;
		++i;
		++i;
		if( *i == "none" )
		    {
		    ++i;
		    string opt = *i;
		    if( !opt.empty() && opt[0]=='(' )
			opt.erase( 0, 1 );
		    if( !opt.empty() && opt[opt.size()-1]==')' )
			opt.erase( opt.size()-1, 1 );
		    sl = splitString( opt, "," );
		    y2mil( "sl:" << sl );
		    skip = find( sl.begin(), sl.end(), "bind" )!=sl.end();
		    if( skip )
			y2mil( "skiping line:" << line );
		    }
		}
	    }
	if( !skip && dev!= "rootfs" && dev!="/dev/root" )
	    {
	    co[dev].device = dev;
	    co[dev].mount = dir;
	    co[dev].fs = extractNthWord( 2, line );
	    co[dev].opts = splitString( extractNthWord( 3, line ), "," );
	    }
	getline( mounts, line );
	}
    mt.select( " / " );
    if( mt.numLines()>0 )
	{
	y2mil( "root mount:" << *mt.getLine(0,true) );
	string dev = extractNthWord( 0, *mt.getLine(0,true));
	if( !dev.empty() && dev[0]!='/' )
	    {
	    dev = sto->findNormalDevice( dev );
	    }
	co[dev].device = dev;
	co[dev].mount = "/";
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
	co[dev].device = dev;
	co[dev].mount = "swap";
	co[dev].fs = "swap";
	getline( mounts, line );
	}
    map<string,FstabEntry>::const_iterator i = co.begin();
    while( i!=co.end() )
	{
	y2mil( "co:[" << i->first << "]-->" << i->second );
	++i;
	}
    }

string 
ProcMounts::getMount( const string& dev ) const
    {
    string ret;
    map<string,FstabEntry>::const_iterator i = co.find( dev );
    if( i!=co.end() )
	ret = i->second.mount;
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
    for( map<string,FstabEntry>::const_iterator i = co.begin(); i!=co.end(); ++i )
	{
	ret[i->second.mount] = i->first;
	}
    return( ret );
    }

void ProcMounts::getEntries( list<FstabEntry>& l ) const
    {
    l.clear();
    for( map<string,FstabEntry>::const_iterator i = co.begin(); i!=co.end(); ++i )
	{
	l.push_back( i->second );
	}
    }

