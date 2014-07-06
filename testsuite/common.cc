
#include <stdlib.h>
#include <iostream>
#include <fstream>

#include "common.h"

#include "storage/AppUtil.h"
#include "storage/Region.h"


extern char* program_invocation_short_name;


namespace storage
{
    using namespace std;


    void
    setup_logger()
    {
	string name = program_invocation_short_name;
	string::size_type pos = name.rfind(".");
	if (pos != string::npos)
	{
	    string path = name.substr(pos + 1) + ".out/out";
	    string file = name.substr(0, pos) + ".log";
	    system(string("rm -f " + path + "/" + file).c_str());
	    createLogger(path, file);
	}
    }


    void
    setup_system(const string& name)
    {
	system("mkdir -p tmp");
	system("rm -rf tmp/*");
	system("mkdir tmp/etc");
	system(string("cp data/" + name + "/*.info tmp").c_str());
    }


    void
    write_fstab(const list<string>& lines)
    {
	ofstream fstab("tmp/etc/fstab");

	for (auto const &it : lines)
	    fstab << it << endl;
    }


    void
    write_crypttab(const list<string>& lines)
    {
	ofstream fstab("tmp/etc/crypttab");

	for (auto const &it : lines)
	    fstab << it << endl;
    }


    void
    print_fstab()
    {
	ifstream fstab("tmp/etc/fstab");

	cout << "begin of fstab" << endl;
	string line;
	while (getline(fstab, line))
	    cout << line << endl;
	cout << "end of fstab" << endl;
    }


    void
    print_crypttab()
    {
	ifstream fstab("tmp/etc/crypttab");

	cout << "begin of crypttab" << endl;
	string line;
	while (getline(fstab, line))
	    cout << line << endl;
	cout << "end of crypttab" << endl;
    }


    void
    print_partitions(StorageInterface* s, const string& disk)
    {
	deque<PartitionInfo> partitioninfos;
	s->getPartitionInfo(disk, partitioninfos);
	for (auto const &it : partitioninfos)
	{
	    cout << it.v.name << ' ';
	    switch (it.partitionType)
	    {
		case PRIMARY: cout << "PRIMARY "; break;
		case EXTENDED: cout << "EXTENDED "; break;
		case LOGICAL: cout << "LOGICAL "; break;
		case PTYPE_ANY: cout << "ANY "; break;
	    }
	    cout << Region(it.cylRegion) << ' ';
	    switch (it.v.fs)
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
		case NTFS: cout << "NTFS"; break;
		case HFSPLUS: cout << "HFSPLUS"; break;
		case SWAP: cout << "SWAP"; break;
		case NFS: cout << "NFS"; break;
		case NFS4: cout << "NFS4"; break;
		case TMPFS: cout << "TMPFS"; break;
		case FSNONE: cout << "NONE"; break;
	    }
	    cout << endl;
	}

	cout << endl;
    }

}
