
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

    list<string> labels;
    labels.push_back("msdos");
    labels.push_back("gpt");
    for (list<string>::const_iterator label = labels.begin(); label != labels.end(); ++label)
    {
	check_zero(s->destroyPartitionTable(disk, *label));
	print_commitinfos(s);
	check_zero(s->commit());

	string part;
	check_zero(s->createPartitionKb(disk, PRIMARY, 0, 1000000, part));
	check_zero(s->createPartitionKb(disk, PRIMARY, 1000000, 3000000, part));
	check_zero(s->createPartitionKb(disk, PRIMARY, 4000000, 500000, part));

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

    for (list<string>::const_iterator it = disks.begin(); it != disks.end(); ++it)
    {
	doit(*it);
    }

    exit(EXIT_SUCCESS);
}
