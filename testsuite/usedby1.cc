
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

    /* try to format partition used by lvm */
    cout << s->changeFormatVolume("/dev/sda2", true, EXT3) << endl; // FAILS

    delete s;
}
