
#include <stdlib.h>
#include <iostream>
#include <boost/algorithm/string.hpp>

#include "common.h"


using namespace std;
using namespace storage;


void
run()
{
    // check order of entries in fstab (bnc #584553)

    StorageInterface* s = createStorageInterface(TestEnvironment());

    string disk = "/dev/sda";

    s->destroyPartitionTable(disk, "msdos");

    string name1;
    cout << s->createPartitionKb(disk, PRIMARY, 0, 999999, name1) << endl;
    cout << name1 << endl;

    string name2;
    cout << s->createPartitionKb(disk, PRIMARY, 1000000, 1999999, name2) << endl;
    cout << name2 << endl;

    cout << s->changeFormatVolume(name1, true, EXT4) << endl;
    cout << s->changeMountPoint(name1, "/usr/local") << endl;
    cout << s->changeLabelVolume(name1, "first") << endl;
    cout << s->changeMountBy(name1, MOUNTBY_LABEL) << endl;

    cout << s->changeFormatVolume(name2, true, EXT4) << endl;
    cout << s->changeMountPoint(name2, "/usr") << endl;
    cout << s->changeLabelVolume(name2, "second") << endl;
    cout << s->changeMountBy(name2, MOUNTBY_LABEL) << endl;

    cout << s->commit() << endl;

    delete s;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    setup_system("empty");

    run();
    print_fstab();
}
