
#include <iostream>

#include "common.h"


using namespace storage;
using namespace std;


void
test(StorageInterface* s, const string& device)
{
    VolumeInfo info;
    cout << s->getVolume(device, info) << " " << device << " " << endl;
}


void
altNamesPartition()
{
    cout << "altNamesPartition" << endl;

    setup_system("thalassa");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    test(s, "/dev/sda1");
    // test(s, "/dev/disk/by-label/BOOT"); // FIXME

    delete s;
}



void
altNamesLogicalVolume()
{
    cout << "altNamesLogicalVolume" << endl;

    setup_system("thalassa");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    test(s, "/dev/system/abuild");
    // test(s, "/dev/mapper/system-abuild"); // FIXME
    // test(s, "/dev/dm-0"); // FIXME
    // test(s, "/dev/disk/by-id/dm-name-system-abuild"); // FIXME
    // test(s, "/dev/disk/by-label/ABUILD"); // FIXME

    delete s;
}


int
main()
{
    setup_logger();

    altNamesPartition();

    cout << endl;

    altNamesLogicalVolume();
}
