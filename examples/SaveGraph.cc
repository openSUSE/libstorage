
#include <stdlib.h>

#include <storage/StorageInterface.h>
#include <storage/Graph.h>

using namespace storage;

int
main (int argc, char** argv)
{
    initDefaultLogger();

    StorageInterface* s = createStorageInterface(Environment(true));

    saveGraph(s, "storage.gv");

    destroyStorageInterface(s);

    exit(EXIT_SUCCESS);
}
