
#include <iostream>

#include "common.h"

#include "storage/StorageTmpl.h"


using namespace storage;
using namespace std;


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    setup_system("thalassa");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    s->setUserdata("/dev/sda1", { { "foo", "hello" } });

    map<string, string> userdata;
    cout << s->getUserdata("/dev/sda1", userdata) << endl;
    cout << userdata << endl;

    s->createBackupState("test");

    s->setUserdata("/dev/sda1", { { "foo", "world" } });

    cout << s->getUserdata("/dev/sda1", userdata) << endl;
    cout << userdata << endl;

    s->restoreBackupState("test");

    cout << s->getUserdata("/dev/sda1", userdata) << endl;
    cout << userdata << endl;

    delete s;
}
