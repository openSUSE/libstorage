
#include <stdlib.h>
#include <iostream>

#include <storage/EtcFstab.h>

#include "common.h"


using namespace std;
using namespace storage;


void
test1()
{
    cout << "test1" << endl;

    setup_system("empty");

    write_fstab({ "/dev/sdb1  /test1  btrfs  defaults  0 0",
		  "/dev/sdb1  /test1/sub  btrfs  subvol=sub  0 0" });

    EtcFstab fstab("tmp/etc");

    FstabEntry entry;
    cout << fstab.findDevice("/dev/sdb1", entry) << endl;

    cout << entry.mount << endl;

    cout << endl;
}


int
main()
{
    setup_logger();

    test1();
}
