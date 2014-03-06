
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/ProcMdstat.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"ARRAY /dev/md/0  metadata=1.0 UUID=35dd06d4:b4e9e248:9262c3ad:02b61654 name=linux:0"
    };

    MdadmExamine mdadmexamine({ "/dev/sda1", "/dev/sdb1" }, false);
    mdadmexamine.parse(lines);

    cout << mdadmexamine << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
