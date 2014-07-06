
/* Creates a partition on the provided disks and creates a lvm vg with a lv
   from the partition. */

#include <stdlib.h>
#include <iostream>

#include "common.h"

using namespace storage;
using namespace std;


void
doit(const string& disk)
{
    StorageInterface* s = createStorageInterface(Environment(false));

    cout << "disk:" << disk << endl;

    check_zero(s->destroyPartitionTable(disk, s->defaultDiskLabel(disk)));
    s->removeLvmVg("test");
    print_commitinfos(s);
    check_zero(s->commit());

    string part;
    check_zero(s->createPartitionKb(disk, PRIMARY, RegionInfo(0, 1024*1024), part));
    cout << "part:" << part << endl;

    print_commitinfos(s);
    check_zero(s->commit());

    deque<string> pvs;
    pvs.push_back(part);
    check_zero(s->createLvmVg("test", 1024, false, pvs));

    string lv;
    check_zero(s->createLvmLv("test", "a", 1024*1024, 1, lv));
    cout << "lv:" << lv << endl;

    check_zero(s->changeFormatVolume(lv, true, EXT4));
    check_zero(s->changeMountPoint(lv, "/test1"));

    print_commitinfos(s);
    check_zero(s->commit());

    delete s;
}


int
main(int argc, char** argv)
{
    list<string> disks(argv + 1, argv + argc);

    if (disks.empty())
    {
	cerr << "no disks provided" << endl;
	exit(EXIT_FAILURE);
    }

    for (auto const &it : disks)
    {
	doit(it);
    }

    exit(EXIT_SUCCESS);
}
