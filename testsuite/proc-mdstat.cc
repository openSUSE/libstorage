
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


void
parse2()
{
    cout << "parse2" << endl;

    vector<string> lines = {
	"Personalities : [raid1] [raid0] ",
	"md125 : active (auto-read-only) raid1 sda[1] sdb[0]",
	"      4194304 blocks super external:/md127/0 [2/2] [UU]",
	"      ",
	"md126 : active raid0 sda[1] sdb[0]",
	"      8378790 blocks super external:/md127/1 128k chunks",
	"      ",
	"md127 : inactive sdb[1](S) sda[0](S)",
	"      5104 blocks super external:imsm",
	"       ",
	"unused devices: <none>",
    };

    ProcMdstat procmdstat(false);
    procmdstat.parse(lines);

    cout << procmdstat << endl;
}


void
parse3()
{
    cout << "parse3" << endl;

    vector<string> lines = {
	"Personalities : [raid1] [raid0] ",
	"md125 : active raid1 sda[1] sdb[0]",
	"      7355904 blocks super external:/md127/1 [2/2] [UU]",
	"      ",
	"md126 : active raid0 sda[1] sdb[0]",
	"      1999872 blocks super external:/md127/0 512k chunks",
	"      ",
	"md127 : inactive sdb[1](S) sda[0](S)",
	"      65536 blocks super external:ddf",
	"       ",
	"unused devices: <none>",
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
    parse2();
    parse3();
}
