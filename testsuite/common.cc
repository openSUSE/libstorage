
#include <stdio.h>
#include <stdlib.h>

#include "common.h"


namespace storage
{

    void
    setup_system(const string& name)
    {
	system("mkdir -p tmp");
	system("rm -rf tmp/*");

	string cmd = "cp " + string("data/" + name + "/*") + " tmp/";
	system(cmd.c_str());
    }

}
