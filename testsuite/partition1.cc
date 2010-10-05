
#include <iostream>

#include "common.h"


using namespace std;
using namespace storage;


StorageInterface* s = 0;


void
msdos (const string& disk, int n)
{
    cout << "msdos " << disk << " " << n << endl;

    s = createStorageInterface(TestEnvironment());

    s->destroyPartitionTable(disk, "msdos");

    long int S = 100000;
    string name;

    cout << s->createPartitionKb(disk, PRIMARY, 0*S, S, name) << endl;
    cout << s->createPartitionKb(disk, PRIMARY, 1*S, S, name) << endl;
    cout << s->createPartitionKb(disk, PRIMARY, 2*S, S, name) << endl;

    cout << s->createPartitionKb(disk, EXTENDED, 3*S, (n+1)*S, name) << endl;

    for (int i = 0; i < n; ++i)
	cout << s->createPartitionKb(disk, LOGICAL, (3+i)*S, S, name) << endl;

    print_partitions(s, disk);

    delete s;
}


void
gpt (const string& disk, int n)
{
    cout << "gpt " << disk << " " << n << endl;

    s = createStorageInterface(TestEnvironment());

    s->destroyPartitionTable(disk, "gpt");

    long int S = 100000;
    string name;

    for (int i = 0; i < n; ++i)
	cout << s->createPartitionKb(disk, PRIMARY, i*S, S, name) << endl;

    print_partitions(s, disk);

    delete s;
}


int
main()
{
    setup_logger();

    /*
     * Check that we can create 3 primary, 1 extended and many logical
     * partitions on a disk with msdos partition table and big range.
     */
    setup_system("thalassa");
    msdos("/dev/sdc", 32);

    /*
     * Check that we can create many primary partitions on a disk with gpt
     * partition table and big range.
     */
    setup_system("thalassa");
    gpt("/dev/sdc", 32);
}
