
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/CmdLvm.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"  system",
	"  data"
    };

    CmdVgs cmdvgs(false);
    cmdvgs.parse(lines);

    cout << cmdvgs << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
