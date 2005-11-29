
#include <string>
#include <list>

#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/UDev.h"

using namespace std;

namespace storage
{

void
udevinfo (const string& name, string& id, string& path)
{
    SystemCmd cmd ("/usr/bin/udevinfo -q symlink -n " + name);
    if (cmd.numLines() == 1)
    {
	list<string> symlinks = splitString( *cmd.getLine (0), " " );
	for (list<string>::const_iterator i = symlinks.begin(); i != symlinks.end(); i++)
	{
	    string tmp = *i;
	    if (tmp.substr (0, 11) == "disk/by-id/")
		id = tmp.substr (11);
	    if (tmp.substr (0, 13) == "disk/by-path/")
		path = tmp.substr (13);
	}
    }
}

}
