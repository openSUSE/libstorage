
#include <iostream>
#include <boost/algorithm/string.hpp>

#include "common.h"


using namespace storage;
using namespace std;


StorageInterface* s = NULL;


void
test(const string& device)
{
    list<string> devices;
    devices.push_back(device);

    list<string> usedby_devices;
    int ret = s->getRecursiveUsedBy(devices, false, usedby_devices);

    cout << device << ": ";
    if (ret == 0)
	cout << boost::join(usedby_devices, " ");
    else
	cout << ret;
    cout << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    setup_system("thalassa");

    s = createStorageInterface(TestEnvironment());

    test("/dev/sda");
    test("/dev/sda1");
    test("/dev/sda2");

    test("/dev/system");
    test("/dev/system/root");

    test("/dev/sdxyz");

    delete s;
}
