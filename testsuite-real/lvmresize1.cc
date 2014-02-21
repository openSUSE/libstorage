
/* Creates several partitions on the provided disks and creates, extends and
   shrinks a lvm vg the partitions. */

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

    deque<string> parts;

    int size = 1024*1024;

    for (int i = 0; i < 4; ++i)
    {
	string part;
	check_zero(s->createPartitionKb(disk, PRIMARY, RegionInfo(i*size, size), part));
	cout << "part[" << i << "]:" << part << endl;
	parts.push_back(part);
    }

    print_commitinfos(s);
    check_zero(s->commit());

    deque<string> pvs1(parts.begin(), parts.begin() + 2);
    check_zero(s->createLvmVg("test", 1024, false, pvs1));

    print_commitinfos(s);
    check_zero(s->commit());

    deque<string> pvs2(parts.begin() + 2, parts.begin() + 4);
    check_zero(s->extendLvmVg("test", pvs2));

    print_commitinfos(s);
    check_zero(s->commit());

    deque<string> pvs3(parts.begin() + 1, parts.begin() + 3);
    check_zero(s->shrinkLvmVg("test", pvs3));

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

    for (list<string>::const_iterator it = disks.begin(); it != disks.end(); ++it)
    {
	doit(*it);
    }

    exit(EXIT_SUCCESS);
}
