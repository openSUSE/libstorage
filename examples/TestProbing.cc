
/**
 *  This program does some basic probing in read-only mode.  As a result
 *  the disk_* and volume_* files in /var/log/YaST2/ will be generated.
 */

#include <iostream>

#include <blocxx/AppenderLogger.hpp>
#include <blocxx/FileAppender.hpp>

#include <y2storage/StorageInterface.h>

using namespace storage;
using namespace std;
using namespace blocxx;

void 
initLogger()
    {
    String name = "testlog";
    LoggerConfigMap configItems;
    String StrKey;
    String StrPath;
    StrKey.format("log.%s.location", name.c_str());
    StrPath = "/var/log/YaST2/y2log";
    configItems[StrKey] = StrPath;
    LogAppenderRef logApp = 
	LogAppender::createLogAppender( name, LogAppender::ALL_COMPONENTS, 
	                                LogAppender::ALL_CATEGORIES, 
					"%d %-5p %c - %m",
					LogAppender::TYPE_FILE,
					configItems );
    LoggerRef log( new AppenderLogger("libstorage", E_INFO_LEVEL, logApp));
    Logger::setDefaultLogger(log);
    }

int
main (int argc, char** argv)
{
    initLogger();
    StorageInterface* s = createStorageInterface (true, false, true);

    deque<string> disks;
    if (!s->getDisks (disks))
    {
	cerr << "getDisks failed\n";
	exit (EXIT_FAILURE);
    }

    for (deque<string>::iterator i1 = disks.begin (); i1 != disks.end(); i1++)
    {
	cout << "Found Disk " << *i1 << '\n';

	deque<PartitionInfo> partitions;
	if (!s->getPartitions (*i1, partitions))
	{
	    cerr << "getPartitions failed\n";
	    exit (EXIT_FAILURE);
	}

	for (deque<PartitionInfo>::iterator i2 = partitions.begin ();
	     i2 != partitions.end(); i2++)
	{
	    cout << i2->name << ' ';
	    switch (i2->partitionType)
	    {
		case PRIMARY: cout << "PRIMARY "; break;
		case EXTENDED: cout << "EXTENDED "; break;
		case LOGICAL: cout << "LOGICAL "; break;
		case PTYPE_ANY: cout << "ANY "; break;
	    }
	    switch (i2->fsType)
	    {
		case FSUNKNOWN: cout << "UNKNOWN"; break;
		case REISERFS: cout << "REISERFS"; break;
		case EXT2: cout << "EXT2"; break;
		case EXT3: cout << "EXT3"; break;
		case VFAT: cout << "VFAT"; break;
		case XFS: cout << "XFS"; break;
		case JFS: cout << "JFS"; break;
		case NTFS: cout << "NTFS"; break;
		case SWAP: cout << "SWAP"; break;
		case FSNONE: cout << "NONE"; break;
	    }
	    cout << '\n';
	}

	cout << '\n';
    }

    delete s;

    exit (EXIT_SUCCESS);
}
