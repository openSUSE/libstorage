
/* Creates a lvm lv on the provided disks and formats it with different
   filesystems. */

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
    check_zero(s->createPartitionKb(disk, PRIMARY, RegionInfo(0, 4*1024*1024), part));
    cout << "part:" << part << endl;

    deque<string> pvs;
    pvs.push_back(part);
    check_zero(s->createLvmVg("test", 1024, false, pvs));

    string lv;
    check_zero(s->createLvmLv("test", "a", 1024*1024, 1, lv));
    cout << "lv:" << lv << endl;

    print_commitinfos(s);
    check_zero(s->commit());

    for (auto const &it : { EXT2, EXT3, EXT4, REISERFS, BTRFS, XFS, VFAT, SWAP })
    {
	FsCapabilities fscaps;
	check_true(s->getFsCapabilities(it, fscaps));

	check_zero(s->changeFormatVolume(lv, true, it));

	if (fscaps.supportsLabel)
	    check_zero(s->changeLabelVolume(lv, "test"));

	check_zero(s->changeMountPoint(lv, it != SWAP ? "/test" : "swap"));

	print_commitinfos(s);
	check_zero(s->commit());
    }

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
