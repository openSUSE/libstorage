
#include <iostream>

#include "common.h"


using namespace std;
using namespace storage;


StorageInterface* s = 0;


void
test()
{
    cout << "test" << endl;

    s = createStorageInterface(TestEnvironment());

    cout << s->removePartition("/dev/sdc5") << endl;

    print_partitions(s, "/dev/sdc");

    delete s;
}


int
main()
{
    setup_logger();

    setup_system("thalassa");

    test();
}
