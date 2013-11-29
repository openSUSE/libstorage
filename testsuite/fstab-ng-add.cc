
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

    FstabChange entry1;
    entry1.device = entry1.dentry = "/dev/sdb1";
    entry1.mount = "/test1";
    entry1.fs = "btrfs";
    entry1.opts = { ("defaults") };

    FstabChange entry2;
    entry2.device = entry2.dentry = "/dev/sdb1";
    entry2.mount = "/test1/sub";
    entry2.fs = "btrfs";
    entry2.opts = { ("subvol=sub") };

    fstab.addEntry(entry1);
    fstab.addEntry(entry2);
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

    FstabChange entry1;
    entry1.device = "/dev/sdb1";
    entry1.dentry = "UUID=1234";
    entry1.mount = "/test1";
    entry1.fs = "btrfs";
    entry1.opts = { ("defaults") };

    FstabChange entry2;
    entry2.device = "/dev/sdb1";
    entry2.dentry = "UUID=1234";
    entry2.mount = "/test1/sub";
    entry2.fs = "btrfs";
    entry2.opts = { ("subvol=sub") };

    fstab.addEntry(entry1);
    fstab.addEntry(entry2);
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

    FstabChange entry;
    entry.device = entry.dentry = "/dev/sdb1";
    entry.mount = "/test1/sub";
    entry.fs = "btrfs";
    entry.opts = { ("subvol=sub") };

    fstab.addEntry(entry);
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
