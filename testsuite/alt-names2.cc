
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

    cout << s->destroyPartitionTable("/dev/sdc", "msdos") << endl;

    string device;
    cout << s->createPartitionKb("/dev/sdc", PRIMARY, RegionInfo(0, 1024*1024), device) << endl;
    cout << device << endl;

    // set label
    cout << s->changeFormatVolume(device, true, EXT4) << endl;
    cout << s->changeLabelVolume(device, "test-label") << endl;

    test(s, "/dev/sdc1");
    // test(s, "/dev/disk/by-label/test-label"); // FIXME

    delete s;
}



void
altNamesLogicalVolume()
{
    cout << "altNamesLogicalVolume" << endl;

    setup_system("thalassa");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    // create volume group
    cout << s->createLvmVg("test-vg", 4, false, { "/dev/sdc1" }) << endl;

    // create logical volume
    string device;
    cout << s->createLvmLv("test-vg", "test-lv", 1024*1024, 1, device) << endl;
    cout << device << endl;

    // set label
    cout << s->changeFormatVolume(device, true, EXT4) << endl;
    cout << s->changeLabelVolume(device, "test-label") << endl;

    test(s, "/dev/test-vg/test-lv");
    test(s, "/dev/mapper/test--vg-test--lv");
    // test(s, "/dev/disk/by-id/dm-name-test--vg-test--lv"); // FIXME
    // test(s, "/dev/disk/by-label/test-label"); // FIXME

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
