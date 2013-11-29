
#include <stdlib.h>
#include <iostream>

#include <storage/EtcFstab.h>

#include "common.h"


using namespace std;
using namespace storage;


void
test1()
{
    cout << "test1" << endl;

    setup_system("empty");

    write_fstab({ "/dev/sdb1  /test1  ext4  defaults  0 0" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "/test1");

    fstab.removeEntry(key);
    fstab.flush();

    print_fstab();

    cout << endl;
}


void
test2()
{
    cout << "test2" << endl;

    setup_system("empty");

    write_fstab({ "/dev/sdb1  swap  swap  defaults  0 0" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "swap");

    fstab.removeEntry(key);
    fstab.flush();

    print_fstab();

    cout << endl;
}


void
test3()
{
    cout << "test3" << endl;

    setup_system("empty");

    write_fstab({ "/dev/mapper/cr_sdb1  /test1  ext4  nofail  0 0" });
    write_crypttab({ "cr_sdb1  /dev/sdb1  none  none" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "/test1");

    fstab.removeEntry(key);
    fstab.flush();

    print_fstab();
    print_crypttab();

    cout << endl;
}


void
test4()
{
    cout << "test4" << endl;

    setup_system("empty");

    write_fstab({ "/dev/mapper/cr_sdb1  /test1  ext4  nofail  0 0" });
    write_crypttab({ "cr_sdb1  /dev/sdb1  /dev/urandom  tmp" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "/test1");

    fstab.removeEntry(key);
    fstab.flush();

    print_fstab();
    print_crypttab();

    cout << endl;
}


void
test5()
{
    cout << "test5" << endl;

    setup_system("empty");

    write_fstab({ "UUID=1234  /test1  ext4  defaults  0 0" });

    EtcFstab fstab("tmp/etc");
    fstab.setDevice("/dev/sdb1", {}, "1234", "", {}, "");

    FstabKey key("/dev/sdb1", "/test1");

    fstab.removeEntry(key);
    fstab.flush();

    print_fstab();

    cout << endl;
}


int
main()
{
    setup_logger();

    test1();
    test2();
    test3();
    test4();
    test5();
}
