
#include <iostream>

#include "common.h"

#include "storage/ProcMdstat.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"Personalities : [raid0] [raid1] [raid10] [raid6] [raid5] [raid4] ",
	"md0 : active raid1 sdb1[1] sda1[0]",
	"      8387520 blocks super 1.0 [2/2] [UU]",
	"      [====>................]  resync = 23.4% (1963200/8387520) finish=4.6min speed=23178K/sec",
	"      bitmap: 1/1 pages [4KB], 65536KB chunk",
	"",
	"unused devices: <none>"
    };

    ProcMdstat procmdstat(false);
    procmdstat.parse(lines);

    cout << procmdstat << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
