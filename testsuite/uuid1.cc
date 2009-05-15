
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <iterator>

#include <y2storage/StorageInterface.h>

#include "common.h"


using namespace std;
using namespace storage;


StorageInterface* s = 0;


void
print_fstab ()
{
    ifstream fstab (testmode ? "tmp/fstab" : "/etc/fstab");
    string line;

    while (getline (fstab, line))
	cout << line << '\n';
}


void
run1 ()
{
    cout << "run1\n";

    s = createStorageInterface(TestEnvironment());

    s->destroyPartitionTable (disk, "msdos");

    long int S = 4 * 1000000;

    string name;
    cout << s->createPartitionKb (disk, PRIMARY, 0, S, name) << '\n';

    cout << name << '\n';

    cout << s->changeFormatVolume (name, true, REISERFS) << '\n';
    cout << s->changeMountPoint (name, "/tmp/mnt") << '\n';
    cout << s->changeMountBy (name, MOUNTBY_UUID) << '\n';

    cout << s->commit () << '\n';

    delete s;
}


void
run2 ()
{
    cout << "run2\n";

    s = createStorageInterface(TestEnvironment());

    string name = disk + "1";
    cout << name << '\n';

    cout << s->changeMountBy (name, MOUNTBY_DEVICE) << '\n';

    cout << s->commit () << '\n';

    delete s;
}


int
main (int argc, char* argv[])
{
    parse_command_line (argc, argv);

    system ("mkdir -p tmp");
    setenv ("YAST2_STORAGE_TDIR", "tmp", 1);

    if (testmode)
    {
	system ("rm -f tmp/fstab tmp/volume_info");
	system ("cp data/disk_hdb tmp/disk_hdb");
    }

    run1 ();
    print_fstab ();

    run2 ();
    print_fstab ();
}
