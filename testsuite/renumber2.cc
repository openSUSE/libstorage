
#include <iostream>

#include "common.h"


using namespace std;
using namespace storage;


int
main()
{
    setup_logger();

    setup_system("empty");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    string device;
    cout << s->createPartition("/dev/sda", EXTENDED, 0, 1000, device) << endl;
    cout << s->createPartition("/dev/sda", LOGICAL,   0, 200, device) << endl;
    cout << s->createPartition("/dev/sda", LOGICAL, 200, 200, device) << endl;

    deque<string> lvm_devices;
    lvm_devices.push_back("/dev/sda6");
    cout << s->createLvmVg("test", 1024, false, lvm_devices) << endl;

    LvmVgInfo lvm_info;
    cout << s->getLvmVgInfo("test", lvm_info) << endl;
    cout << lvm_info.devices_add << endl;

    cout << s->removePartition("/dev/sda5") << endl;

    cout << s->getLvmVgInfo("test", lvm_info) << endl;
    cout << lvm_info.devices_add << endl;

    delete s;
}
