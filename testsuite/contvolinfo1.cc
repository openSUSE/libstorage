
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <iterator>

#include <storage/StorageInterface.h>

#include "common.h"


using namespace std;
using namespace storage;


StorageInterface* s = 0;


void
test(const string& device)
{
    cout << "device:" << device << endl;

    ContVolInfo info;

    int ret = s->getContVolInfo(device, info);
    cout << "ret:" << ret << endl;

    if (ret == 0)
    {
	cout << "type:" << info.type << endl;
	cout << "cname:" << info.cname << endl;
	cout << "vname:" << info.vname << endl;
	cout << "numeric:" << info.numeric << endl;
	cout << "nr:" << info.nr << endl;
    }
}


int
main(int argc, char* argv[])
{
    system("mkdir -p tmp");

    system("cp data/disk_sda tmp/disk_sda");
    system("cp data/volume_info tmp/volume_info");

    s = createStorageInterface(TestEnvironment());

    test("/dev/sda2");

    test("/dev/disk/by-label/BOOT");
    test("LABEL=BOOT");

    test("/dev/disk/by-uuid/14875716-b8e3-4c83-ac86-48c20682b63a");

    test("/dev/disk/by-uuid/unknown");

    delete s;
}
