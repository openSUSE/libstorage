
#include <iostream>

#include "common.h"

#include "storage/ProcParts.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"major minor  #blocks  name",
	"",
	"   8        0  976762584 sda",
	"   8        1    1051648 sda1",
	"   8        2  943721472 sda2",
	" 253        0   31457280 dm-0",
	" 253        1   52428800 dm-1",
	" 253        2   16777216 dm-2",
	" 253        3    2097152 dm-3",
	" 253        4  314572800 dm-4",
	" 253        5    5242880 dm-5",
	" 253        6    2097152 dm-6",
	" 253        7    1048576 dm-7",
	"  11        0    1048575 sr0",
    };

    ProcParts procparts(false);
    procparts.parse(lines);

    cout << procparts << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
