
#include <iostream>

#include "common.h"


using namespace storage;
using namespace std;


void
test(const string& name)
{
    setup_system(name);

    StorageInterface* s = createStorageInterface(TestEnvironment());

    cout << s->defaultDiskLabel() << " "
	 << s->defaultDiskLabelSize(5ULL * 1024*1024*1024) << endl;

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
