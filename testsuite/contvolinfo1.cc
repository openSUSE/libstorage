
#include <iostream>

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
	cout << "ctype:" << info.ctype << endl;
	cout << "cname:" << info.cname << endl;
	cout << "vname:" << info.vname << endl;
    }

    cout << endl;
}


int
main()
{
    setup_logger();

    setup_system("thalassa");

    s = createStorageInterface(TestEnvironment());

    test("/dev/sda");
    test("/dev/sda1");

    test("/dev/system");
    test("/dev/system/swap");

    test("/dev/unknown");

    delete s;
}
