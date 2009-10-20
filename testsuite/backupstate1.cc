
#include <stdlib.h>
#include <iostream>

#include "common.h"

using namespace storage;
using namespace std;

int
main()
{
    cout.setf(std::ios::boolalpha);

    system("mkdir -p tmp");
    system("rm -rf tmp/*");

    system("cp data/disk_sda tmp");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    s->createBackupState("test");

    cout << s->checkBackupState("test") << endl;
    cout << s->equalBackupStates("test", "", true) << endl;

    s->changeFormatVolume("/dev/sda1", true, EXT3);

    cout << s->checkBackupState("test") << endl;
    cout << s->equalBackupStates("test", "", true) << endl;

    s->changeFormatVolume("/dev/sda1", false, EXT3);

    cout << s->checkBackupState("test") << endl;
    cout << s->equalBackupStates("test", "", true) << endl;

    delete s;
}
