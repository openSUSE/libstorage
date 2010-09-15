
#include <iostream>

#include "common.h"

using namespace storage;
using namespace std;


void
doit(const string& disk)
{
    StorageInterface* s = createStorageInterface(Environment(false));

    cout << "disk:" << disk << endl;

    check(s->destroyPartitionTable(disk, s->defaultDiskLabel(disk)) == 0);

    string volume;
    check(s->createPartitionKb(disk, PRIMARY, 0, 1024*1024, volume) == 0);
    cout << "volume:" << volume << endl;

    print_commitinfos(s);
    check(s->commit() == 0);

    static const FsType elem[] = { EXT2, EXT3, EXT4, REISERFS, BTRFS, XFS, JFS, VFAT, SWAP };
    const list<FsType> fstypes(elem, elem + lengthof(elem));
    for (list<FsType>::const_iterator it = fstypes.begin(); it != fstypes.end(); ++it)
    {
	FsCapabilities fscaps;
	check(s->getFsCapabilities(*it, fscaps) == true);

	check(s->changeFormatVolume(volume, true, *it) == 0);

	if (fscaps.supportsLabel)
	    check(s->changeLabelVolume(volume, "test") == 0);

	print_commitinfos(s);
	check(s->commit() == 0);
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
