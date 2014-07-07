
/* Creates several partitions on the provided disks with default alignment. */

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

    for (auto const &label : {"msdos", "gpt"})
    {
	check_zero(s->destroyPartitionTable(disk, label));
	print_commitinfos(s);
	check_zero(s->commit());

	string part;
	check_zero(s->createPartitionKb(disk, PRIMARY, RegionInfo(0, 1000000), part));
	check_zero(s->createPartitionKb(disk, PRIMARY, RegionInfo(1000000, 3000000), part));
	check_zero(s->createPartitionKb(disk, PRIMARY, RegionInfo(4000000, 500000), part));

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
