
/* Creates a partition on the provided disks and formats it with different
   filesystems and does resizing. */

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
    print_commitinfos(s);
    check_zero(s->commit());

    for (auto const &it : { EXT2, EXT3, EXT4, REISERFS, XFS, VFAT })
    {
	string part;
	check_zero(s->createPartitionKb(disk, PRIMARY, RegionInfo(0, 1024*1024), part));
	cout << "part:" << part << endl;

	print_commitinfos(s);
	check_zero(s->commit());

	FsCapabilities fscaps;
	check_true(s->getFsCapabilities(it, fscaps));

	check_zero(s->changeFormatVolume(part, true, it));

	print_commitinfos(s);
	check_zero(s->commit());

	if (fscaps.isExtendable)
	{
	    check_zero(s->resizeVolume(part, 2*1024*1024));

	    print_commitinfos(s);
	    check_zero(s->commit());
	}

	if (fscaps.isReduceable)
	{
	    check_zero(s->resizeVolume(part, 1024*1024));

	    print_commitinfos(s);
	    check_zero(s->commit());
	}

	if (fscaps.isExtendableWhileMounted)
	{
	    check_zero(s->changeMountPoint(part, "/test"));

	    print_commitinfos(s);
	    check_zero(s->commit());

	    check_zero(s->resizeVolume(part, 3*1024*1024));

	    print_commitinfos(s);
	    check_zero(s->commit());
	}

	check_zero(s->removePartition(part));
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
