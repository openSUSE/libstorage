
/**
 *  This program does some basic probing in read-only mode.  As a result
 *  the disk_* and volume_* files in /var/log/YaST2/ will be generated.
 */

#include <unistd.h>
#include <iostream>
#include <fstream>

#include <storage/StorageInterface.h>

using namespace storage;
using namespace std;

ofstream logstream;
unsigned pid;

void logDo( int level, const string& component, const char* file,
            int line, const char* function, const string& content )
    {
    logstream << "<" << level << "> " << component << "(" << pid << ") " 
              << file << "(" << function << "):" << line << " " 
              << content << endl;
    }

bool logQuery( int level, const string& component )
    {
    return( level>=0 );
    }

int
main (int argc, char** argv)
    {
    pid = getpid();
    logstream.open( "/tmp/emil_log", ios_base::app );
    storage::setLogDoCallback( logDo );
    storage::setLogQueryCallback( logQuery );
    StorageInterface* s = createStorageInterface(Environment(true));

    deque<ContainerInfo> containers;
    s->getContainers (containers);

    for (deque<ContainerInfo>::const_iterator i1 = containers.begin();
	 i1 != containers.end(); ++i1)
    {
	switch (i1->type)
	{
	    case DISK: {

		cout << "found container (disk) " << i1->name << '\n';

		deque<PartitionInfo> partitions;
		if (s->getPartitionInfo (i1->name, partitions) != 0)
		{
		    cerr << "getPartitionInfo failed\n";
		    exit (EXIT_FAILURE);
		}

		for (deque<PartitionInfo>::const_iterator i2 = partitions.begin();
		     i2 != partitions.end(); ++i2)
		{
		    cout << "  " << i2->v.name << ' ';
		    switch (i2->partitionType)
		    {
			case PRIMARY: cout << "PRIMARY "; break;
			case EXTENDED: cout << "EXTENDED "; break;
			case LOGICAL: cout << "LOGICAL "; break;
			case PTYPE_ANY: cout << "ANY "; break;
		    }
		    switch (i2->v.fs)
		    {
			case FSUNKNOWN: cout << "UNKNOWN"; break;
			case REISERFS: cout << "REISERFS"; break;
			case EXT2: cout << "EXT2"; break;
			case EXT3: cout << "EXT3"; break;
			case EXT4: cout << "EXT4"; break;
			case BTRFS: cout << "BTRFS"; break;
			case VFAT: cout << "VFAT"; break;
			case XFS: cout << "XFS"; break;
			case JFS: cout << "JFS"; break;
			case HFS: cout << "HFS"; break;
			case HFSPLUS: cout << "HFSPLUS"; break;
			case NTFS: cout << "NTFS"; break;
			case SWAP: cout << "SWAP"; break;
			case NFS: cout << "NFS"; break;
			case NFS4: cout << "NFS4"; break;
			case TMPFS: cout << "TMPFS"; break;
			case FSNONE: cout << "NONE"; break;
		    }
		    cout << '\n';
		}

	    } break;

	    case MD:
	    {
		cout << "found special container (md) " << i1->name << '\n';

		deque<MdInfo> mds;
		if (s->getMdInfo(mds) != 0)
		{
		    cerr << "getMdInfo failed\n";
		    exit (EXIT_FAILURE);
		}

		for (deque<MdInfo>::const_iterator i2 = mds.begin();
		     i2 != mds.end(); ++i2)
		{
		    cout << "  " << i2->v.name;
		    cout << '\n';
		}

	    } break;

	    case LVM: {

		cout << "found container (lvm) " << i1->name << '\n';

		deque<LvmLvInfo> lvmlvs;
		if (s->getLvmLvInfo (i1->name, lvmlvs) != 0)
		{
		    cerr << "getLvmLvInfo failed\n";
		    exit (EXIT_FAILURE);
		}

		for (deque<LvmLvInfo>::const_iterator i2 = lvmlvs.begin();
		     i2 != lvmlvs.end(); ++i2)
		{
		    cout << "  " << i2->v.name;
		    cout << '\n';
		}

	    } break;

	    default: {

		cout << "found container " << i1->name << '\n';

	    } break;

	}

	cout << '\n';
    }

    delete s;
    logstream.close();

    exit (EXIT_SUCCESS);
    }
