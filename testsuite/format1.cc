
#include <iostream>
#include <iterator>

#include <y2storage/StorageInterface.h>

using namespace std;
using namespace storage;


StorageInterface* s = 0;


void
print_partitions (const string& disk)
{
    list<PartitionInfo> partitioninfos;
    s->getPartitions (disk, partitioninfos);
    for (list<PartitionInfo>::iterator i = partitioninfos.begin ();
	 i != partitioninfos.end(); i++)
    {
	cout << i->name << ' ';
	switch (i->partitionType)
	{
	    case PRIMARY: cout << "PRIMARY "; break;
	    case EXTENDED: cout << "EXTENDED "; break;
	    case LOGICAL: cout << "LOGICAL "; break;
	    default: cout << "UNKNOWN ";
	}
	cout << i->cylStart << ' ' << i->cylSize << ' ';
	cout << i->fsType << '\n';
    }

    cout << '\n';
}


void
test ()
{
    printf ("test\n");

    cout << s->changeFormatVolume ("/dev/hda1", true, REISERFS) << '\n';
    cout << s->changeFormatVolume ("/dev/hda2", true, EXT2) << '\n';
    cout << s->changeFormatVolume ("/dev/hda3", true, EXT3) << '\n';
    cout << s->changeFormatVolume ("/dev/hda4", true, REISERFS) << '\n'; // FAILS
    cout << s->changeFormatVolume ("/dev/hda5", true, EXT2) << '\n';
    cout << s->changeFormatVolume ("/dev/hda6", true, EXT3) << '\n';

    print_partitions ("/dev/hda");
}


int
main ()
{
    setenv ("YAST2_STORAGE_TDIR", ".", 1);

    s = createStorageInterface (true, true, true);

    test ();

    delete s;
}
