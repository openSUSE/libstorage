
#include <iostream>

#include "common.h"


using namespace storage;
using namespace std;


void
test(const string& name)
{
    setup_system(name);

    StorageInterface* s = createStorageInterface(TestEnvironment());

    cout << s->defaultDiskLabel("/dev/sda") << endl;

    delete s;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    test("thalassa");
    test("scandium");
}
