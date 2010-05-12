
#include <stdlib.h>
#include <iostream>

#include <storage/StorageInterface.h>

using namespace storage;
using namespace std;


void
doit ()
{
    StorageInterface* s = createStorageInterface(Environment(false));


    cout << s->destroyPartitionTable("sdb", s->defaultDiskLabel("hdb")) << '\n';

    s->setPartitionAlignment( ALIGN_CYLINDER );

    string name = "xxx1";
    cout << s->createPartitionKb ("sdb", PRIMARY, 0, 1000000, name) << '\n';
    cout << name << '\n';
    cout << s->createPartitionKb ("sdb", PRIMARY, 1000000, 3000000, name) << '\n';
    cout << name << '\n';
    cout << s->createPartitionKb ("sdb", PRIMARY, 4000000, 500000, name) << '\n';
    cout << name << '\n';

    cout << s->commit () << '\n';

    delete s;
}


int
main (int argc, char** argv)
{
    initDefaultLogger ();

    doit ();

    exit (EXIT_SUCCESS);
}
