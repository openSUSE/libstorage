
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#include "storage/AppUtil.h"


extern char *program_invocation_short_name;


namespace storage
{

    void
    setup_logger()
    {
	string name = program_invocation_short_name;
	string::size_type pos = name.rfind(".");
	if (pos != string::npos)
	    createLogger("default", name.substr(pos + 1) + ".out/out",
			 name.substr(0, pos) + ".log");
    }


    void
    setup_system()
    {
	system("mkdir -p tmp");
	system("rm -rf tmp/*");
    }


    void
    setup_system(const string& name)
    {
	setup_system();

	string cmd = "cp " + string("data/" + name + "/*.info") + " tmp";
	system(cmd.c_str());
    }

}
