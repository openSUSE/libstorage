
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

    EtcFstab fstab("tmp/etc");

    FstabChange c;
    c.device = c.dentry = "/dev/sdb1";
    c.mount = "/test1";
    c.fs = "ext4";
    c.opts = { ("defaults") };

    fstab.addEntry(c);
    fstab.flush();

    print_fstab();

    cout << endl;
}


void
test2()
{
    cout << "test2" << endl;

    setup_system("empty");

    EtcFstab fstab("tmp/etc");

    FstabChange c;
    c.device = c.dentry = "/dev/sdb1";
    c.mount = "swap";
    c.fs = "swap";
    c.opts = { ("defaults") };

    fstab.addEntry(c);
    fstab.flush();

    print_fstab();

    cout << endl;
}


void
test3()
{
    cout << "test3" << endl;

    setup_system("empty");

    EtcFstab fstab("tmp/etc");

    FstabChange c;
    c.device = "/dev/sdb1";
    c.dentry = "/dev/mapper/cr_sdb1";
    c.mount = "/test1";
    c.fs = "ext4";
    c.opts = { ("defaults") };
    c.encr = storage::ENC_LUKS;

    fstab.addEntry(c);
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

    EtcFstab fstab("tmp/etc");

    FstabChange c;
    c.device = "/dev/sdb1";
    c.dentry = "/dev/mapper/cr_sdb1";
    c.mount = "/test1";
    c.fs = "ext4";
    c.opts = { ("defaults") };
    c.encr = storage::ENC_LUKS;
    c.tmpcrypt = true;

    fstab.addEntry(c);
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

    EtcFstab fstab("tmp/etc");

    FstabChange c;
    c.device = "/dev/sdb1";
    c.dentry = "UUID=1234";
    c.mount = "/test1";
    c.fs = "ext4";
    c.opts = { ("defaults") };

    fstab.addEntry(c);
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
