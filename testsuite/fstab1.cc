
#include <stdlib.h>
#include <iostream>
#include <iterator>

#include "common.h"


using namespace std;
using namespace storage;


StorageInterface* s = 0;


void
mountpoint1 ()
{
    cout << "mountpoint1\n";

    s = createStorageInterface(TestEnvironment());

    string mountpoint;

    cout << s->changeMountPoint ("/dev/hda1", "") << '\n';
    cout << s->getMountPoint ("/dev/hda1", mountpoint) << '\n';
    cout << mountpoint << '\n';

    cout << s->changeMountPoint ("/dev/hda1", "/mnt") << '\n';
    cout << s->getMountPoint ("/dev/hda1", mountpoint) << '\n';
    cout << mountpoint << '\n';

    delete s;
}


void
mountby1 ()
{
    cout << "mountby1\n";

    s = createStorageInterface(TestEnvironment());

    MountByType mountby;

    cout << s->changeMountBy ("/dev/hda1", MOUNTBY_DEVICE) << '\n';
    cout << s->getMountBy ("/dev/hda1", mountby) << '\n';
    cout << mountby << '\n';

    cout << s->changeMountBy ("/dev/hda1", MOUNTBY_UUID) << '\n';
    cout << s->getMountBy ("/dev/hda1", mountby) << '\n';
    cout << mountby << '\n';

    cout << s->changeMountBy ("/dev/hda1", MOUNTBY_LABEL) << '\n';
    cout << s->getMountBy ("/dev/hda1", mountby) << '\n';
    cout << mountby << '\n';

    delete s;
}


void
options1 ()
{
    cout << "options1\n";

    s = createStorageInterface(TestEnvironment());

    string options;

    cout << s->changeFstabOptions ("/dev/hda1", "acl,user_xattr") << '\n';
    cout << s->getFstabOptions ("/dev/hda1", options) << '\n';
    cout << options << '\n';

    cout << s->removeFstabOptions ("/dev/hda1", "acl") << '\n';
    cout << s->getFstabOptions ("/dev/hda1", options) << '\n';
    cout << options << '\n';

    cout << s->addFstabOptions ("/dev/hda1", "noauto") << '\n';
    cout << s->getFstabOptions ("/dev/hda1", options) << '\n';
    cout << options << '\n';

    cout << s->addFstabOptions ("/dev/hda1", "iocharset=utf8") << '\n';
    cout << s->getFstabOptions ("/dev/hda1", options) << '\n';
    cout << options << '\n';

    cout << s->removeFstabOptions ("/dev/hda1", "iocharset=*") << '\n';
    cout << s->getFstabOptions ("/dev/hda1", options) << '\n';
    cout << options << '\n';

    cout << s->removeFstabOptions ("/dev/hda1", "not_here") << '\n';
    cout << s->getFstabOptions ("/dev/hda1", options) << '\n';
    cout << options << '\n';

    delete s;
}


void
crypt1 ()
{
    cout << "crypt1\n";

    s = createStorageInterface(TestEnvironment());

    bool val = false;

    cout << s->setCryptPassword ("/dev/hda1", "test") << '\n';	// FAILS

    cout << s->setCrypt ("/dev/hda1", true) << '\n'; // FAILS

    cout << s->setCryptPassword ("/dev/hda1", "hello-world") << '\n';

    cout << s->setCrypt ("/dev/hda1", true) << '\n';
    cout << s->getCrypt ("/dev/hda1", val) << '\n';
    cout << val << '\n';

    cout << s->setCrypt ("/dev/hda1", false) << '\n';
    cout << s->getCrypt ("/dev/hda1", val) << '\n';
    cout << val << '\n';

    delete s;
}


void
combined1 ()
{
    cout << "combined1\n";

    s = createStorageInterface(TestEnvironment());

    string mountpoint;

    cout << s->changeMountPoint ("/dev/hda1", "/mnt") << '\n';
    cout << s->changeMountBy ("/dev/hda1", MOUNTBY_DEVICE) << '\n';

    cout << s->changeMountPoint ("/dev/hda1", "") << '\n';
    cout << s->changeMountBy ("/dev/hda1", MOUNTBY_DEVICE) << '\n'; // FAILS

    delete s;
}


int
main ()
{
    system ("mkdir -p tmp");
    setenv("LIBSTORAGE_TESTDIR", "tmp", 1);

    system ("cp data/volume_info tmp");
    system ("cp data/disk_hda tmp");
    mountpoint1 ();
    mountby1 ();
    options1 ();
    crypt1 ();
    combined1 ();
}
