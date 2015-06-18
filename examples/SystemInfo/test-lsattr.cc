
#include <iostream>

#include <storage/SystemInfo/SystemInfo.h>

using namespace std;
using namespace storage;


void
test_lsattr(SystemInfo& systeminfo)
{
    try
    {
	const CmdLsattr& cmdlsattr = systeminfo.getCmdLsattr("/dev/system/btrfs", "/btrfs",
							     "/var/lib/mariadb");
	cout << "CmdLsattr success" << endl;
	cout << cmdlsattr << endl;
    }
    catch (const exception& e)
    {
	cerr << "CmdLsattr failed" << endl;
    }
}


int
main()
{
    createLogger("/var/log/YaST2", "libstorage");

    SystemInfo systeminfo;

    test_lsattr(systeminfo);
}
