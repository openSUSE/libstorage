
/* Creates a partition on provided disks and formats it with different
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

    string volume;
    check_zero(s->createPartitionKb(disk, PRIMARY, 0, 1024*1024, volume));
    cout << "volume:" << volume << endl;

    print_commitinfos(s);
    check_zero(s->commit());

    static const FsType elem[] = { EXT2, EXT3, EXT4, REISERFS, BTRFS, XFS, JFS, VFAT, SWAP };
    const list<FsType> fstypes(elem, elem + lengthof(elem));
    for (list<FsType>::const_iterator it = fstypes.begin(); it != fstypes.end(); ++it)
    {
	FsCapabilities fscaps;
	check_true(s->getFsCapabilities(*it, fscaps));

	check_zero(s->changeFormatVolume(volume, true, *it));

	if (fscaps.supportsLabel)
	    check_zero(s->changeLabelVolume(volume, "test"));

	print_commitinfos(s);
	check_zero(s->commit());
    }

    delete s;
}


int
main(int argc, char** argv)
{
    initDefaultLogger();

    list<string> disks(argv + 1, argv + argc);

    if (disks.empty())
    {
	cerr << "no disks provided" << endl;
	exit(EXIT_FAILURE);
    }

    for (list<string>::const_iterator it = disks.begin(); it != disks.end(); ++it)
    {
	doit(*it);
    }

    exit(EXIT_SUCCESS);
}
