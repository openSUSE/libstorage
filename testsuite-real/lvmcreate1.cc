
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


    deque <string> pds;
    pds.push_back ("hdb");
    cout << s->createLvmVg ("trash", 4, false, pds) << '\n';


    string device;
    cout << s->createLvmLv ("trash", "junk", 500, 1, device) << '\n';


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
