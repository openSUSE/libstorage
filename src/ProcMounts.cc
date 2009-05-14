// Maintainer: fehr@suse.de
/*
  Textdomain    "storage"
*/


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
	    FstabEntry entry;
	    entry.device = dev;
	    entry.mount = dir;
	    entry.fs = extractNthWord(2, *it);
	    entry.opts = splitString(extractNthWord(3, *it), ",");
	    data.insert(make_pair(dev, entry));
	}
    }

    /*
    // code seems to be obsolete
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
    */

    AsciiFile swaps("/proc/swaps");
    swaps.remove(0, 1);

    for (vector<string>::const_iterator it = swaps.lines().begin(); it != swaps.lines().end(); ++it)
    {
	string dev = extractNthWord(0, *it);
	string::size_type pos = dev.find("\\040(deleted)");
	if (pos != string::npos)
	    dev.erase(pos);

	FstabEntry entry;
	entry.device = dev;
	entry.mount = "swap";
	entry.fs = "swap";
	data.insert(make_pair(dev, entry));
    }

    for (const_iterator it = data.begin(); it != data.end(); ++it)
	y2mil("data[" << it->first << "] -> " << it->second);
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
ProcMounts::getMount(const string& device) const
    {
    string ret;
    const_iterator i = data.find(device);
    if (i != data.end())
	ret = i->second.mount;
    return ret;
    }

string
ProcMounts::getMount(const list<string>& devices) const
    {
    string ret;
    list<string>::const_iterator i = devices.begin();
    while (ret.empty() && i != devices.end())
	{
	ret = getMount( *i );
	++i;
	}
    return ret;
    }


    list<string>
    ProcMounts::getAllMounts(const string& device) const
    {
	list<string> ret;

	pair<const_iterator, const_iterator> range = data.equal_range(device);
	for (const_iterator i = range.first; i != range.second; ++i)
	    ret.push_back(i->second.mount);

	return ret;
    }


    list<string>
    ProcMounts::getAllMounts(const list<string>& devices) const
    {
	list<string> ret;

	for (list<string>::const_iterator i = devices.begin(); i != devices.end(); ++i)
	    ret.splice(ret.end(), getAllMounts(*i));

	return ret;
    }


map<string, string>
ProcMounts::allMounts() const
    {
    map<string, string> ret;
    for (const_iterator i = data.begin(); i != data.end(); ++i)
	{
	ret[i->second.mount] = i->first;
	}
    return ret;
    }


    list<FstabEntry>
    ProcMounts::getEntries() const
    {
	list<FstabEntry> ret;
	for (const_iterator i = data.begin(); i != data.end(); ++i)
	    ret.push_back(i->second);
	return ret;
    }

}
