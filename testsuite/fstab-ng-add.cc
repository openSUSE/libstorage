
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

    FstabChange c1;
    c1.device = c1.dentry = "/dev/sdb1";
    c1.mount = "/test1";
    c1.fs = "btrfs";
    c1.opts = { ("defaults") };

    FstabChange c2;
    c2.device = c2.dentry = "/dev/sdb1";
    c2.mount = "/test1/sub";
    c2.fs = "btrfs";
    c2.opts = { ("subvol=sub") };

    fstab.addEntry(c1);
    fstab.addEntry(c2);
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

    FstabChange c1;
    c1.device = "/dev/sdb1";
    c1.dentry = "UUID=1234";
    c1.mount = "/test1";
    c1.fs = "btrfs";
    c1.opts = { ("defaults") };

    FstabChange c2;
    c2.device = "/dev/sdb1";
    c2.dentry = "UUID=1234";
    c2.mount = "/test1/sub";
    c2.fs = "btrfs";
    c2.opts = { ("subvol=sub") };

    fstab.addEntry(c1);
    fstab.addEntry(c2);
    fstab.flush();

    print_fstab();

    cout << endl;
}


void
test3()
{
    cout << "test3" << endl;

    setup_system("empty");

    write_fstab({ "/dev/sdb1  /test1  btrfs  defaults  0 0" });

    EtcFstab fstab("tmp/etc");

    FstabChange c;
    c.device = c.dentry = "/dev/sdb1";
    c.mount = "/test1/sub";
    c.fs = "btrfs";
    c.opts = { ("subvol=sub") };

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
}
