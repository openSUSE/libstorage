
#include <iostream>
#include <sstream>

#include "common.h"


using namespace storage;
using namespace std;


void createLvs(const string& vg, int n, const deque<string>& pvs)
{
    cout << "createLvs" << endl;

    StorageInterface* s = createStorageInterface(TestEnvironment());

    /* create volume group with the above pvs */
    cout << s->createLvmVg( vg, 4, false, pvs ) << endl;

    /* create n logical volumes */
    int ret = 0;
    for ( int i = 0; i < n; i++ )
    {
	ostringstream name;
	name << "volume";
	name << i;

	string dev;
	ret = s->createLvmLv( vg, name.str(), 100, 1, dev );
    }
    cout << ret << endl;

    deque<LvmLvInfo> plist;
    s->getLvmLvInfo( vg, plist );
    cout << plist.size() << endl;
    
    delete s;
}


void createExtendedLv(const string& vg, const string& dev)
{
    cout << "createExtendedLv" << endl;

    StorageInterface* s = createStorageInterface(TestEnvironment());

    deque<string> devs;
    devs.push_back(dev); // add extended partition

    /* create volume group with the extended partition */
    cout << s->createLvmVg( vg, 4, false, devs ) << endl; // FAILS

    delete s;
}


int
main()
{
    setup_logger();

    /*
     * Check that we can create a volume group from primary and logical
     * partitons and 50 logical volumes.
     */
    setup_system("thalassa");

    deque<string> pvs;
    pvs.push_back("/dev/sdc1");
    pvs.push_back("/dev/sdc2");
    pvs.push_back("/dev/sdc5");
    pvs.push_back("/dev/sdc6");
    createLvs("test", 50, pvs);

    /*
     * Check that we cannot create a volume group out of an extended
     * partition.
     */
    setup_system("thalassa");

    createExtendedLv("test", "/dev/sdc4");
}
