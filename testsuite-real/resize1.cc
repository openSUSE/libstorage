
/* Creates a partition on provided disks and formats it with different
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

    static const FsType elem[] = { EXT2, EXT3, EXT4, REISERFS, XFS, VFAT };
    const list<FsType> fstypes(elem, elem + lengthof(elem));
    for (list<FsType>::const_iterator it = fstypes.begin(); it != fstypes.end(); ++it)
    {
	check(s->destroyPartitionTable(disk, s->defaultDiskLabel(disk)) == 0);

	string volume;
	check(s->createPartitionKb(disk, PRIMARY, 0, 1024*1024, volume) == 0);
	cout << "volume:" << volume << endl;

	print_commitinfos(s);
	check(s->commit() == 0);

	FsCapabilities fscaps;
	check(s->getFsCapabilities(*it, fscaps) == true);

	check(s->changeFormatVolume(volume, true, *it) == 0);

	print_commitinfos(s);
	check(s->commit() == 0);

	if (fscaps.isExtendable)
	{
	    check(s->resizeVolume(volume, 2*1024*1024) == 0);

	    print_commitinfos(s);
	    check(s->commit() == 0);
	}

	if (fscaps.isReduceable)
	{
	    check(s->resizeVolume(volume, 1024*1024) == 0);

	    print_commitinfos(s);
	    check(s->commit() == 0);
	}

	if (fscaps.isExtendableWhileMounted)
	{
	    check(s->changeMountPoint(volume, "/test") == 0);

	    print_commitinfos(s);
	    check(s->commit() == 0);

	    check(s->resizeVolume(volume, 3*1024*1024) == 0);

	    print_commitinfos(s);
	    check(s->commit() == 0);
	}
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
