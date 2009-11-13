
#include <stdlib.h>
#include <iostream>

#include <storage/StorageInterface.h>
#include <storage/Graph.h>

using namespace storage;
using namespace std;

int
main (int argc, char** argv)
{
    initDefaultLogger();

    StorageInterface* s = createStorageInterface(Environment(true));

    saveGraph(s, "storage.gv");

    destroyStorageInterface(s);

    cout << "run \"dot -T png -o storage.png storage.gv\" to generate a png" << endl;

    exit(EXIT_SUCCESS);
}
