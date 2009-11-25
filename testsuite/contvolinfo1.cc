
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
	if (info.numeric)
	    cout << "nr:" << info.nr << endl;
    }
}


int
main()
{
    setup_system("thalassa");

    s = createStorageInterface(TestEnvironment());

    test("/dev/sda2");

    test("/dev/system/swap");

    test("/dev/disk/by-label/BOOT");
    test("LABEL=BOOT");

    test("/dev/disk/by-uuid/14875716-b8e3-4c83-ac86-48c20682b63a");

    test("/dev/disk/by-uuid/31e381c9-5b35-4045-8d01-9274a30e1298");

    test("/dev/disk/by-uuid/unknown");

    delete s;
}
