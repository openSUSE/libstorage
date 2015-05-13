
#include <stdlib.h>
#include <iostream>
#include <boost/algorithm/string.hpp>

#include "common.h"


using namespace std;
using namespace storage;


void
run1 ()
{
    TRACE();

    StorageInterface* s = createStorageInterface(TestEnvironment());

    string disk = "/dev/sda";

    s->destroyPartitionTable (disk, "msdos");

    long int size = 4 * 1000000;

    string name;
    cout << s->createPartitionKb(disk, PRIMARY, RegionInfo(0, size), name) << endl;

    cout << name << endl;

    cout << s->changeFormatVolume(name, true, REISERFS) << endl;
    cout << s->changeMountPoint(name, "/tmp/mnt") << endl;
    cout << s->changeMountBy(name, MOUNTBY_UUID) << endl;

    cout << "Committing: " << endl;
    cout << s->commit() << endl;

    delete s;
}


void
run2 ()
{
    TRACE();

    StorageInterface* s = createStorageInterface(TestEnvironment());

    string name = "/dev/sda1";

    cout << name << endl;

    cout << s->changeMountBy(name, MOUNTBY_DEVICE) << endl;

    cout << s->commit() << endl;

    delete s;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    setup_system("empty");

    run1 ();
    print_fstab ();

    run2 ();
    print_fstab ();
}
