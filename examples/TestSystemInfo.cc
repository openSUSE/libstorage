

#include <iostream>

#include <storage/SystemInfo/SystemInfo.h>

using namespace std;
using namespace storage;


void
doit_procparts(SystemInfo& systeminfo)
{
    try
    {
	const ProcParts& procparts = systeminfo.getProcParts();
	cout << "ProcParts entries " << procparts.getEntries().size() << endl;
    }
    catch (const exception& e)
    {
	cerr << "ProcParts failed" << endl;
    }
}


void
doit_majorminor(SystemInfo& systeminfo, const string& device)
{
    try
    {
	const MajorMinor& majorminor = systeminfo.getMajorMinor(device);
	cout << "MajorMinor " << device << " " << majorminor.getMajor() << ":"
	     << majorminor.getMinor() << endl;
    }
    catch (const exception& e)
    {
	cerr << "MajorMinor failed for " << device << " " << e.what() << endl;
    }
}


int
main()
{
    createLogger("/var/log/YaST2", "libstorage");

    SystemInfo systeminfo;

    doit_procparts(systeminfo);
    doit_procparts(systeminfo);

    doit_majorminor(systeminfo, "/dev/sda");
    doit_majorminor(systeminfo, "/dev/sda");

    doit_majorminor(systeminfo, "/dev/disk/by-label/BOOT");
    doit_majorminor(systeminfo, "/dev/disk/by-label/BOOT");

    doit_majorminor(systeminfo, "/dev/does-not-exist");
    doit_majorminor(systeminfo, "/dev/does-not-exist");

    doit_majorminor(systeminfo, "/dev/char/5:0");
    doit_majorminor(systeminfo, "/dev/char/5:0");
}
