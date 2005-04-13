
#include <iostream>
#include <iterator>

#include <y2storage/StorageInterface.h>

using namespace std;
using namespace storage;


StorageInterface* s = 0;


void
print_partitions (const string& disk)
{
    deque<PartitionInfo> partitioninfos;
    s->getPartitions (disk, partitioninfos);
    for (deque<PartitionInfo>::iterator i = partitioninfos.begin ();
	 i != partitioninfos.end(); i++)
    {
	cout << i->name << ' ';
	switch (i->partitionType)
	{
	    case PRIMARY: cout << "PRIMARY "; break;
	    case EXTENDED: cout << "EXTENDED "; break;
	    case LOGICAL: cout << "LOGICAL "; break;
	}
	cout << i->cylStart << ' ' << i->cylSize << ' ';
	switch (i->fsType)
	{
	    case FSUNKNOWN: cout << "FSUNKNOWN"; break;
	    case REISERFS: cout << "REISERFS"; break;
	    case EXT2: cout << "EXT2"; break;
	    case EXT3: cout << "EXT3"; break;
	    case VFAT: cout << "VFAT"; break;
	    case XFS: cout << "XFS"; break;
	    case JFS: cout << "JFS"; break;
	    case NTFS: cout << "NTFS"; break;
	    case SWAP: cout << "SWAP"; break;
	}
	cout << '\n';
    }

    cout << '\n';
}


void
test ()
{
    printf ("test\n");

    s = createStorageInterface (false, true, false);

    cout << s->changeFormatVolume ("/dev/hda1", true, REISERFS) << '\n';
    cout << s->changeFormatVolume ("/dev/hda2", true, EXT2) << '\n';
    cout << s->changeFormatVolume ("/dev/hda3", true, EXT3) << '\n';
    cout << s->changeFormatVolume ("/dev/hda4", true, REISERFS) << '\n'; // FAILS
    cout << s->changeFormatVolume ("/dev/hda5", true, EXT2) << '\n';
    cout << s->changeFormatVolume ("/dev/hda6", true, EXT3) << '\n';

    print_partitions ("/dev/hda");

    delete s;
}


int
main ()
{
    system ("mkdir -p tmp");
    setenv ("YAST2_STORAGE_TDIR", "tmp", 1);

    system ("cp data/disk_hda tmp/disk_hda");
    system ("rm -f tmp/volume_info");
    test ();
}
