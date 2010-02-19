
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
    cout << s->getPartitionPrefix("/dev/mapper/isw_cfcjajfdfh_test_part4") << endl;

    delete s;
}
