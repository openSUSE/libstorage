
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

    saveDeviceGraph(s, "device.gv");
    saveMountGraph(s, "mount.gv");

    destroyStorageInterface(s);

    cout << "run \"dot -T png -o device.png device.gv\" and "
	"\"dot -T png -o mount.png mount.gv\" to generate pngs" << endl;

    exit(EXIT_SUCCESS);
}
