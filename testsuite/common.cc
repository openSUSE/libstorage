
#include <stdlib.h>
#include <iostream>
#include <fstream>

#include "common.h"

#include "storage/AppUtil.h"


extern char* program_invocation_short_name;


namespace storage
{
    using namespace std;


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


    void
    print_fstab()
    {
	ifstream fstab("tmp/etc/fstab");

	cout << "begin of fstab" << endl;
	string line;
	while (getline(fstab, line))
	    cout << line << endl;
	cout << "end of fstab" << endl;
    }


    void
    print_crypttab()
    {
	ifstream fstab("tmp/etc/crypttab");

	cout << "begin of crypttab" << endl;
	string line;
	while (getline(fstab, line))
	    cout << line << endl;
	cout << "end of crypttab" << endl;
    }

}
