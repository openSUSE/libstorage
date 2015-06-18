
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/CmdLsattr.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"---------------C /var/lib/mariadb"
    };

    CmdLsattr cmdlsattr("/", "var/lib/mariadb", false);
    cmdlsattr.parse(lines);

    cout << cmdlsattr << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
