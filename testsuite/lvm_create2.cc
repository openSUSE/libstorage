
#include <iostream>

#include "common.h"


using namespace storage;
using namespace std;


int
main()
{
    setup_logger();

    setup_system("thalassa");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    string device;
    deque<string> devices;

    /* create volume group "system" which does already exist */
    devices.clear();
    devices.push_back("/dev/sdb1");
    cout << s->createLvmVg("system", 4, false, devices) << endl; // FAILS

    /* create volume group "data" */
    devices.clear();
    devices.push_back("/dev/sdb1");
    cout << s->createLvmVg("data", 4, false, devices) << endl;

    /* create logical volume "swap" on "system" which does already exist */
    cout << s->createLvmLv("system", "swap", 2000, 1, device) << endl; // FAILS

    /* create logical volume "swap" on "data" */
    cout << s->createLvmLv("data", "swap", 2000, 1, device) << endl;

    delete s;
}
