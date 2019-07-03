
#include <iostream>

#include "common.h"


using namespace std;
using namespace storage;


int
main()
{
    setup_logger();

    StorageInterface* s = createStorageInterface(TestEnvironment());

    cout << s->getPartitionName("/dev/sda", 1) << endl;

    cout << s->getPartitionName("/dev/cciss/c0d1", 2) << endl;

    cout << s->getPartitionName("/dev/mmcblk0", 3) << endl;

    cout << s->getPartitionName("/dev/mapper/isw_cfcjajfdfh_test", 4) << endl;

    cout << s->getPartitionName("/dev/md2", 5) << endl;

    cout << s->getPartitionName("/dev/rsxx0", 6) << endl;

    cout << s->getPartitionName("/dev/nvme0n1", 7) << endl;

    cout << s->getPartitionName("/dev/pmem0", 1) << endl;
    cout << s->getPartitionName("/dev/pmem0s", 1) << endl;
    cout << s->getPartitionName("/dev/pmem0.1", 1) << endl;
    cout << s->getPartitionName("/dev/pmem0.1s", 1) << endl;

    delete s;
}
