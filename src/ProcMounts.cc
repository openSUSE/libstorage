// Maintainer: fehr@suse.de
/*
  Textdomain    "storage"
*/

#include <fstream>

#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AsciiFile.h"
#include "y2storage/ProcMounts.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/Storage.h"
#include "y2storage/StorageDefines.h"


namespace storage
{
    using namespace std;


ProcMounts::ProcMounts( Storage * const sto )
    {
    const map<string, string> by_label = getRevUdevMap("/dev/disk/by-label");
    const map<string, string> by_uuid = getRevUdevMap("/dev/disk/by-uuid");

    SystemCmd mt(MOUNTBIN);

    AsciiFile mounts("/proc/mounts");

    for (vector<string>::const_iterator it = mounts.lines().begin(); it != mounts.lines().end(); ++it)
    {
	string dev = extractNthWord(0, *it);
	string dir = extractNthWord(1, *it);

	if (boost::contains(dev, "/by-label/"))
	{
	    dev = dev.substr( dev.rfind( "/" )+1 );
	    y2mil( "dev:" << dev );
	    map<string, string>::const_iterator it = by_label.find(dev);
	    if (it != by_label.end())
	    {
		dev = it->second;
		normalizeDevice( dev );
		y2mil( "dev:" << dev );
	    }
	}
	else if (boost::contains(dev, "/by-uuid/"))
	{
	    dev = dev.substr( dev.rfind( "/" )+1 );
	    y2mil( "dev:" << dev );
	    map<string, string>::const_iterator it = by_uuid.find(dev);
	    if (it != by_uuid.end())
	    {
		dev = it->second;
		normalizeDevice( dev );
		y2mil( "dev:" << dev );
	    }
	}

	if (dev == "rootfs" || dev == "/dev/root" || isBind(mt, dir))
	{
	    y2mil("skipping line:" << *it);
	}
	else
	{
	    co[dev].device = dev;
	    co[dev].mount = dir;
	    co[dev].fs = extractNthWord(2, *it);
	    co[dev].opts = splitString(extractNthWord(3, *it), ",");
	}
    }

    mt.select( " / " );
    if( mt.numLines()>0 )
    {
	y2mil( "root mount:" << mt.getLine(0,true) );
	string dev = extractNthWord( 0, mt.getLine(0,true));
	if( !dev.empty() && dev[0]!='/' )
	{
	    dev = sto->findNormalDevice( dev );
	}
	co[dev].device = dev;
	co[dev].mount = "/";
    }

    AsciiFile swaps("/proc/swaps");
    swaps.remove(0, 1);

    for (vector<string>::const_iterator it = swaps.lines().begin(); it != swaps.lines().end(); ++it)
    {
	string dev = extractNthWord(0, *it);
	string::size_type pos = dev.find("\\040(deleted)");
	if (pos != string::npos)
	    dev.erase(pos);

	co[dev].device = dev;
	co[dev].mount = "swap";
	co[dev].fs = "swap";
    }

    for (map<string, FstabEntry>::const_iterator it = co.begin(); it != co.end(); ++it)
	y2mil("co:[" << it->first << "]-->" << it->second);
    }


    bool
    ProcMounts::isBind(SystemCmd& mt, const string& dir) const
    {
	bool ret = false;

	mt.select( (string)" on "+dir+' ' );
	if( mt.numLines(true)>0 )
	{
	    list<string> sl = splitString( mt.getLine(0,true) );
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
		    ret = find(sl.begin(), sl.end(), "bind") != sl.end();
		}
	    }
	}

	return ret;
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

}
