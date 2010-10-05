
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

    cout << s->changeFormatVolume("/dev/sdc1", true, REISERFS) << endl;
    cout << s->changeFormatVolume("/dev/sdc2", true, EXT2) << endl;
    cout << s->changeFormatVolume("/dev/sdc3", true, EXT3) << endl;
    cout << s->changeFormatVolume("/dev/sdc4", true, REISERFS) << endl; // FAILS
    cout << s->changeFormatVolume("/dev/sdc5", true, EXT2) << endl;
    cout << s->changeFormatVolume("/dev/sdc6", true, EXT3) << endl;

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
