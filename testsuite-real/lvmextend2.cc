
#include <stdlib.h>
#include <iostream>

#include <storage/StorageInterface.h>

using namespace storage;
using namespace std;


void
doit ()
{
    StorageInterface* s = createStorageInterface(Environment(false));


    cout << s->destroyPartitionTable ("hdb", s->defaultDiskLabel ()) << '\n';


    int size = 1000000;

    string name1;
    cout << s->createPartitionKb ("hdb", PRIMARY, 0*size, size, name1) << '\n';
    cout << name1 << '\n';

    string name2;
    cout << s->createPartitionKb ("hdb", PRIMARY, 1*size, size, name2) << '\n';
    cout << name2 << '\n';

    string name3;
    cout << s->createPartitionKb ("hdb", PRIMARY, 2*size, size, name3) << '\n';
    cout << name3 << '\n';

    string name4;
    cout << s->createPartitionKb ("hdb", PRIMARY, 3*size, size, name4) << '\n';
    cout << name4 << '\n';


    deque <string> pds12;
    pds12.push_back (name1);
    pds12.push_back (name2);
    cout << s->createLvmVg ("trash", 4, false, pds12) << '\n';


    cout << s->commit () << '\n';


    delete s;


    s = createStorageInterface(Environment(false));


    deque <string> pds34;
    pds34.push_back (name3);
    pds34.push_back (name4);
    cout << s->extendLvmVg ("trash", pds34) << '\n';


    string device;
    cout << s->createLvmLv ("trash", "junk", 3500, 1, device) << '\n';


    cout << s->changeFormatVolume (device, true, EXT3) << '\n';
    cout << s->changeMountPoint (device, "/mnt") << '\n';


    cout << s->commit () << '\n';


    delete s;
}


int
main (int argc, char** argv)
{
    initDefaultLogger ();

    cout << "first run\n";
    doit ();

    cout << "second run\n";
    doit ();

    exit (EXIT_SUCCESS);
}
