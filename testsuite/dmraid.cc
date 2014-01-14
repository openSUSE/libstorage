
#include <iostream>

#include "common.h"

#include "storage/DmraidCo.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"isw_bfhghbahji_foo:16767744:128:mirror:ok:0:2:0",
	"/dev/sda:isw:isw_bfhghbahji_foo:mirror:ok:16767744:0",
	"/dev/sdb:isw:isw_bfhghbahji_foo:mirror:ok:16767744:0"
    };

    CmdDmraid cmddmraid(false);
    cmddmraid.parse(lines);

    cout << cmddmraid << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
