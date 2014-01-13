
#include <iostream>

#include "common.h"

#include "storage/BtrfsCo.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"Label: none  uuid: ea108250-d02c-41dd-b4d8-d4a707a5c649",
	"        Total devices 1 FS bytes used 28.00KiB",
	"        devid    1 size 1.00GiB used 138.38MiB path /dev/mapper/system-test",
	"",
	"Label: none  uuid: d82229f2-f9e4-40fd-b15f-84e2d42e6d0d",
	"        Total devices 1 FS bytes used 420.00KiB",
	"        devid    1 size 2.00GiB used 240.75MiB path /dev/mapper/system-testsuite",
	"",
	"Label: none  uuid: 653764e0-7ea2-4dbe-9fa1-866f3f7783c9",
	"        Total devices 1 FS bytes used 316.00KiB",
	"        devid    1 size 5.00GiB used 548.00MiB path /dev/mapper/system-btrfs",
	"",
	"Btrfs v3.12+20131125"
    };

    CmdBtrfsShow cmdbtrfsshow(false);
    cmdbtrfsshow.parse(lines);

    cout << cmdbtrfsshow << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
