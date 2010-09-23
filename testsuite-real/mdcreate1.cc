
/* Creates four partitions on the provided disks and creates md raids with
   different md types from the partition. */

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

    deque<string> parts;

    int size = 1024*1024;

    for (int i = 0; i < 4; ++i)
    {
	string part;
	check_zero(s->createPartitionKb(disk, PRIMARY, i*size, size, part));
	cout << "part[" << i << "]:" << part << endl;
	parts.push_back(part);
    }

    print_commitinfos(s);
    check_zero(s->commit());

    static const MdType elem[] = { RAID0, RAID1, RAID5, RAID6, RAID10 };
    const list<MdType> fstypes(elem, elem + lengthof(elem));
    for (list<MdType>::const_iterator it = fstypes.begin(); it != fstypes.end(); ++it)
    {
	string md;
	check_zero(s->createMdAny(*it, parts, md));

	check_zero(s->changeFormatVolume(md, true, EXT4));
	check_zero(s->changeMountPoint(md, "/test1"));

	print_commitinfos(s);
	check_zero(s->commit());

	check_zero(s->removeMd(md, true));

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
