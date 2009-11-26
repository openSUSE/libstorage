
#include <stdio.h>
#include <stdlib.h>

#include "common.h"


namespace storage
{

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

	string cmd = "cp " + string("data/" + name + "/*") + " tmp/";
	system(cmd.c_str());
    }

}
