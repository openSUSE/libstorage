
/**
 *  This program does some basic probing in read-only mode.  As a result
 *  the disk_* and volume_* files in /var/log/YaST2/ will be generated.
 */

#include <iostream>

#include <y2storage/StorageInterface.h>

using namespace storage;
using namespace std;

int
main (int argc, char** argv)
{
    StorageInterface* s = createStorageInterface (true, false, true);

    list<string> disks;
    if (!s->getDisks (disks))
    {
	cerr << "getDisks failed\n";
	exit (EXIT_FAILURE);
    }

    for (list<string>::iterator i1 = disks.begin (); i1 != disks.end(); i1++)
    {
	cout << "Found Disk " << *i1 << '\n';

	list<PartitionInfo> partitions;
	if (!s->getPartitions (*i1, partitions))
	{
	    cerr << "getPartitions failed\n";
	    exit (EXIT_FAILURE);
	}

	for (list<PartitionInfo>::iterator i2 = partitions.begin ();
	     i2 != partitions.end(); i2++)
	{
	    cout << i2->name << ' ';
	    switch (i2->partitionType)
	    {
		case PRIMARY: cout << "PRIMARY "; break;
		case EXTENDED: cout << "EXTENDED "; break;
		case LOGICAL: cout << "LOGICAL "; break;
	    }
	    switch (i2->fsType)
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

    delete s;

    exit (EXIT_SUCCESS);
}
