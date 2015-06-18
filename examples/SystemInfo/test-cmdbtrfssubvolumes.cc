
#include <iostream>

#include <storage/SystemInfo/SystemInfo.h>

using namespace std;
using namespace storage;


void
test_cmdbtrfssubvolumes(SystemInfo& systeminfo)
{
    cout << systeminfo.isCmdBtrfsSubvolumesCached("/dev/system/btrfs") << endl;

    try
    {
	const CmdBtrfsSubvolumes& cmdbtrfssubvolumes = systeminfo.getCmdBtrfsSubvolumes("/dev/system/btrfs", "/btrfs");
	cout << "Cmdbtrfssubvolumes success" << endl;
	cout << cmdbtrfssubvolumes << endl;
    }
    catch (const exception& e)
    {
	cerr << "CmdBtrfsSubvolumes failed" << endl;
    }

    cout << systeminfo.isCmdBtrfsSubvolumesCached("/dev/system/btrfs") << endl;

    {
	const CmdBtrfsSubvolumes& cmdbtrfssubvolumes = systeminfo.getCmdBtrfsSubvolumes("/dev/system/btrfs", "does-not-matter");
	cout << "Cmdbtrfssubvolumes success" << endl;
	cout << cmdbtrfssubvolumes << endl;
    }
}


int
main()
{
    createLogger("/var/log/YaST2", "libstorage");

    SystemInfo systeminfo;

    test_cmdbtrfssubvolumes(systeminfo);
}
