
#include <iostream>

#include "common.h"


using namespace std;
using namespace storage;


int
main()
{
    setup_logger();

    setup_system("thalassa");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    cout << s->removePartition("/dev/sdc5") << endl;

    print_partitions(s, "/dev/sdc");

    delete s;
}
