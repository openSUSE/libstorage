
#include <iostream>

#include "common.h"


using namespace storage;
using namespace std;


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    setup_system("thalassa");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    s->createBackupState("test");
    cout << s->checkBackupState("test") << " "
	 << s->equalBackupStates("test", "", true) << endl;

    s->changeFormatVolume("/dev/sda1", true, EXT3);
    cout << s->checkBackupState("test") << " "
	 << s->equalBackupStates("test", "", true) << endl;

    s->changeFormatVolume("/dev/sda1", false, EXT3);
    cout << s->checkBackupState("test") << " "
	 << s->equalBackupStates("test", "", true) << endl;

    s->changeMountBy("/dev/system/abuild", MOUNTBY_DEVICE);
    cout << s->checkBackupState("test") << " "
	 << s->equalBackupStates("test", "", true) << endl;

    s->changeMountBy("/dev/system/abuild", MOUNTBY_UUID);
    cout << s->checkBackupState("test") << " "
	 << s->equalBackupStates("test", "", true) << endl;

    delete s;
}
