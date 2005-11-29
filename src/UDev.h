#ifndef _UDev_h
#define _UDev_h

#include <string>

using std::string;

namespace storage
{

void
udevinfo (const string& name, string& id, string& path);

}

#endif
