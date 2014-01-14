
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
	"MD_LEVEL=raid1",
	"MD_DEVICES=2",
	"MD_METADATA=1.0",
	"MD_UUID=35dd06d4:b4e9e248:9262c3ad:02b61654",
	"MD_NAME=linux:0",
	"MD_DEVICE_sda1_ROLE=0",
	"MD_DEVICE_sda1_DEV=/dev/sda1",
	"MD_DEVICE_sdb1_ROLE=1",
	"MD_DEVICE_sdb1_DEV=/dev/sdb1",
    };

    MdadmDetails mdadmdetails("/dev/md0", false);
    mdadmdetails.parse(lines);

    cout << mdadmdetails << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
