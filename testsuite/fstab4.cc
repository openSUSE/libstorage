
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

    cout << s->setCryptPassword(name, "12345678") << endl;
    cout << s->setCrypt(name, true) << endl;

    deque<string> pvs;
    pvs.push_back(name);
    cout << s->createLvmVg("test", 1024, false, pvs) << endl;

    cout << s->commit() << endl;	// TODO: fails, but acceptable for testcase

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
