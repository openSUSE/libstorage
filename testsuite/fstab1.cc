
#include <stdlib.h>
#include <iostream>

#include "common.h"


using namespace std;
using namespace storage;


StorageInterface* s = 0;


void
mountpoint1()
{
    cout << "mountpoint1\n";

    s = createStorageInterface(TestEnvironment());

    string mountpoint;

    cout << s->changeMountPoint("/dev/sda1", "") << '\n';
    cout << s->getMountPoint("/dev/sda1", mountpoint) << '\n';
    cout << mountpoint << '\n';

    cout << s->changeMountPoint("/dev/sda1", "/mnt") << '\n';
    cout << s->getMountPoint("/dev/sda1", mountpoint) << '\n';
    cout << mountpoint << '\n';

    delete s;
}


void
mountby1()
{
    cout << "mountby1\n";

    s = createStorageInterface(TestEnvironment());

    MountByType mountby;

    cout << s->changeMountBy("/dev/sda1", MOUNTBY_DEVICE) << '\n';
    cout << s->getMountBy("/dev/sda1", mountby) << '\n';
    cout << mountby << '\n';

    cout << s->changeMountBy("/dev/sda1", MOUNTBY_UUID) << '\n';
    cout << s->getMountBy("/dev/sda1", mountby) << '\n';
    cout << mountby << '\n';

    cout << s->changeMountBy("/dev/sda1", MOUNTBY_LABEL) << '\n';
    cout << s->getMountBy("/dev/sda1", mountby) << '\n';
    cout << mountby << '\n';

    delete s;
}


void
options1()
{
    cout << "options1\n";

    s = createStorageInterface(TestEnvironment());

    string options;

    cout << s->changeFstabOptions("/dev/sda1", "acl,user_xattr") << '\n';
    cout << s->getFstabOptions("/dev/sda1", options) << '\n';
    cout << options << '\n';

    cout << s->removeFstabOptions("/dev/sda1", "acl") << '\n';
    cout << s->getFstabOptions("/dev/sda1", options) << '\n';
    cout << options << '\n';

    cout << s->addFstabOptions("/dev/sda1", "noauto") << '\n';
    cout << s->getFstabOptions("/dev/sda1", options) << '\n';
    cout << options << '\n';

    cout << s->addFstabOptions("/dev/sda1", "iocharset=utf8") << '\n';
    cout << s->getFstabOptions("/dev/sda1", options) << '\n';
    cout << options << '\n';

    cout << s->removeFstabOptions("/dev/sda1", "iocharset=*") << '\n';
    cout << s->getFstabOptions("/dev/sda1", options) << '\n';
    cout << options << '\n';

    cout << s->removeFstabOptions("/dev/sda1", "not_here") << '\n';
    cout << s->getFstabOptions("/dev/sda1", options) << '\n';
    cout << options << '\n';

    delete s;
}


void
crypt1()
{
    cout << "crypt1\n";

    s = createStorageInterface(TestEnvironment());

    bool val = false;

    cout << s->changeFormatVolume("/dev/sda1", true, EXT3 ) << '\n';
    cout << s->setCryptPassword("/dev/sda1", "test") << '\n';	// FAILS
    cout << s->setCrypt("/dev/sda1", true) << '\n'; // FAILS

    cout << s->changeFormatVolume("/dev/sda1", false, EXT3 ) << '\n';
    cout << s->setCryptPassword("/dev/sda1", "test") << '\n';
    cout << s->setCrypt("/dev/sda1", true) << '\n';

    cout << s->changeFormatVolume("/dev/sda1", true, EXT3 ) << '\n';
    cout << s->setCryptPassword("/dev/sda1", "hello-world") << '\n';

    cout << s->setCrypt("/dev/sda1", true) << '\n';
    cout << s->getCrypt("/dev/sda1", val) << '\n';
    cout << val << '\n';

    cout << s->setCrypt("/dev/sda1", false) << '\n';
    cout << s->getCrypt("/dev/sda1", val) << '\n';
    cout << val << '\n';

    delete s;
}


void
combined1()
{
    cout << "combined1\n";

    s = createStorageInterface(TestEnvironment());

    string mountpoint;

    cout << s->changeMountPoint("/dev/sda1", "/mnt") << '\n';
    cout << s->changeMountBy("/dev/sda1", MOUNTBY_DEVICE) << '\n';

    cout << s->changeMountPoint("/dev/sda1", "") << '\n';
    cout << s->changeMountBy("/dev/sda1", MOUNTBY_DEVICE) << '\n'; // FAILS

    delete s;
}


int
main()
{
    setup_logger();

    setup_system("thalassa");

    mountpoint1();
    mountby1();
    options1();
    crypt1();
    combined1();
}
