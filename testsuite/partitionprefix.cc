
#include <iostream>

#include "common.h"


using namespace std;
using namespace storage;


int
main()
{
    setup_logger();

    StorageInterface* s = createStorageInterface(TestEnvironment());

    cout << s->getPartitionPrefix("/dev/sda") << endl;
    cout << s->getPartitionPrefix("/dev/sda1") << endl;

    cout << s->getPartitionPrefix("/dev/cciss/c0d1") << endl;
    cout << s->getPartitionPrefix("/dev/cciss/c0d1p2") << endl;

    cout << s->getPartitionPrefix("/dev/mmcblk0") << endl;
    cout << s->getPartitionPrefix("/dev/mmcblk0p3") << endl;

    cout << s->getPartitionPrefix("/dev/mapper/isw_cfcjajfdfh_test") << endl;
    cout << s->getPartitionPrefix("/dev/mapper/isw_cfcjajfdfh_test-part4") << endl;

    cout << s->getPartitionPrefix("/dev/rsxx0") << endl;
    cout << s->getPartitionPrefix("/dev/rsxx0p5") << endl;

    cout << s->getPartitionPrefix("/dev/nvme0n1") << endl;
    cout << s->getPartitionPrefix("/dev/nvme0n1p6") << endl;

    cout << s->getPartitionPrefix("/dev/pmem0") << endl;
    cout << s->getPartitionPrefix("/dev/pmem0p1") << endl;

    cout << s->getPartitionPrefix("/dev/pmem0s") << endl;
    cout << s->getPartitionPrefix("/dev/pmem0s1") << endl;

    cout << s->getPartitionPrefix("/dev/pmem0.1") << endl;
    cout << s->getPartitionPrefix("/dev/pmem0.1p1") << endl;

    cout << s->getPartitionPrefix("/dev/pmem0.1s") << endl;
    cout << s->getPartitionPrefix("/dev/pmem0.1s1") << endl;

    delete s;
}
