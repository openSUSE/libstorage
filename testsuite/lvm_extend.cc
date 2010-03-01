
#include <iostream>

#include "common.h"


using namespace storage;
using namespace std;


void
extendVg(const string& vg, const deque<string>& pvs)
{
    cout << "extendVg" << endl;

    StorageInterface* s = createStorageInterface(TestEnvironment());

    LvmVgInfo info;
    cout << s->getLvmVgInfo(vg, info) << endl;
    cout << info.devices << endl;

    /* extend it by devs_extend given pvs */
    cout << s->extendLvmVg(vg, pvs) << endl;

    cout << s->getLvmVgInfo(vg, info) << endl;
    cout << info.devices << endl;
    cout << info.devices_add << endl;

    delete s;
}


int
main()
{
    setup_logger();

    setup_system("thalassa");

    deque<string> devs;

    /*
     * Check that we can extend a volume group.
     */
    devs.push_back("/dev/sdb1");
    extendVg("system", devs);
}
