
#include <stdlib.h>

#include "common.h"

#include "storage/AppUtil.h"


extern char* program_invocation_short_name;


namespace storage
{

    void
    setup_logger()
    {
	string name = program_invocation_short_name;
	string::size_type pos = name.rfind(".");
	if (pos != string::npos)
	{
	    string path = name.substr(pos + 1) + ".out/out";
	    string file = name.substr(0, pos) + ".log";
	    system(string("rm -f " + path + "/" + file).c_str());
	    createLogger("default", path, file);
	}
    }


    void
    setup_system(const string& name)
    {
	system("mkdir -p tmp");
	system("rm -rf tmp/*");
	system(string("cp data/" + name + "/*.info tmp").c_str());
    }

}
