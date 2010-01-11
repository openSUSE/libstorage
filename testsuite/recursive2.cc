
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
    int ret = s->getRecursiveUsing(device, devices);

    cout << device << " ";
    if (ret == 0)
	cout << boost::join(devices, " ");
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
