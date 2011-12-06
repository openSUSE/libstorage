
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

    list<string> using_devices;
    int ret = s->getRecursiveUsing(devices, false, using_devices);

    cout << device << ": ";
    if (ret == 0)
	cout << boost::join(using_devices, " ");
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
    test("/dev/sdb");
    test("/dev/sdb1");
    test("/dev/sdb2");

    test("/dev/system");

    test("/dev/sdxyz");

    delete s;
}
