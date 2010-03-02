
#include <stdlib.h>
#include <iostream>
#include <boost/algorithm/string.hpp>

#include "common.h"


using namespace std;
using namespace storage;


void
run()
{
    StorageInterface* s = createStorageInterface(TestEnvironment());

    string disk = "/dev/sda";

    s->destroyPartitionTable(disk, "msdos");

    string name;
    cout << s->createPartitionKb(disk, PRIMARY, 0, 999999, name) << endl;
    cout << name << endl;

    cout << s->changeFormatVolume(name, true, EXT4) << endl;
    cout << s->changeMountPoint(name, "/secret") << endl;
    cout << s->setCryptPassword(name, "12345678") << endl;
    cout << s->setCrypt(name, true) << endl;
    
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
    print_crypttab();
}
