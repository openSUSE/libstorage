
#include <stdlib.h>
#include <iostream>
#include <iterator>

#include <y2storage/StorageInterface.h>

#include "common.h"


using namespace std;
using namespace storage;


StorageInterface* s = 0;


void
print_partitions (const string& disk)
{
    deque<PartitionInfo> partitioninfos;
    s->getPartitionInfo (disk, partitioninfos);
    for (deque<PartitionInfo>::iterator i = partitioninfos.begin ();
	 i != partitioninfos.end(); i++)
    {
	cout << i->v.name << ' ';
	switch (i->partitionType)
	{
	    case PRIMARY: cout << "PRIMARY "; break;
	    case EXTENDED: cout << "EXTENDED "; break;
	    case LOGICAL: cout << "LOGICAL "; break;
	    case PTYPE_ANY: cout << "ANY "; break;
	}
	cout << i->cylStart << ' ' << i->cylSize << ' ';
	switch (i->v.fs)
	{
	    case FSUNKNOWN: cout << "UNKNOWN"; break;
	    case REISERFS: cout << "REISERFS"; break;
	    case EXT2: cout << "EXT2"; break;
	    case EXT3: cout << "EXT3"; break;
	    case EXT4: cout << "EXT4"; break;
	    case VFAT: cout << "VFAT"; break;
	    case XFS: cout << "XFS"; break;
	    case JFS: cout << "JFS"; break;
	    case HFS: cout << "HFS"; break;
	    case NTFS: cout << "NTFS"; break;
	    case HFSPLUS: cout << "HFSPLUS"; break;
	    case SWAP: cout << "SWAP"; break;
	    case NFS: cout << "NFS"; break;
	    case FSNONE: cout << "NONE"; break;
	}
	cout << '\n';
    }

    cout << '\n';
}


void
test ()
{
    printf ("test\n");

    s = createStorageInterface(TestEnvironment());

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
    setenv("LIBSTORAGE_TESTDIR", "tmp", 1);

    system ("cp data/disk_hda tmp/disk_hda");
    system ("rm -f tmp/volume_info");
    test ();
}
