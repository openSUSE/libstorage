
#include <stdlib.h>

#include <y2storage/StorageInterface.h>

using namespace storage;

int
main (int argc, char** argv)
{
    initDefaultLogger();
    StorageInterface* s = createStorageInterface (true, false, true);

    s->saveGraph("storage.dot");

    exit(EXIT_SUCCESS);
}
