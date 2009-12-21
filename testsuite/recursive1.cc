
#include <iostream>

#include "common.h"


using namespace storage;
using namespace std;


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    setup_system("thalassa");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    /* try to remove partition used by lvm */
    cout << s->removePartition("/dev/sda2") << endl; // FAILS

    /* remove partition used by lvm with recursive removal on */
    s->setRecursiveRemoval(true);
    cout << s->removePartition("/dev/sda2") << endl;

    delete s;
}
