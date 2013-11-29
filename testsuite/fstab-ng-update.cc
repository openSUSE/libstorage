
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

    write_fstab({ "/dev/sdb1  /test1  btrfs  defaults  0 0",
		  "/dev/sdb1  /test1/sub  btrfs  subvol=sub  0 0" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "/test1/sub");

    FstabChange entry;
    entry.device = entry.dentry = "/dev/sdb1";
    entry.mount = "/test1/sub2";
    entry.fs = "btrfs";
    entry.opts = { ("subvol=sub2") };

    fstab.updateEntry(key, entry);
    fstab.flush();

    print_fstab();

    cout << endl;
}


void
test2()
{
    cout << "test2" << endl;

    setup_system("empty");

    write_fstab({ "UUID=1234  /test1  btrfs  defaults  0 0",
		  "UUID=1234  /test1/sub  btrfs  subvol=sub  0 0" });

    EtcFstab fstab("tmp/etc");
    fstab.setDevice("/dev/sdb1", {}, "1234", "", {}, "");

    FstabKey key("/dev/sdb1", "/test1/sub");

    FstabChange entry;
    entry.device = "/dev/sdb1";
    entry.dentry = "UUID=1234";
    entry.mount = "/test1/sub2";
    entry.fs = "btrfs";
    entry.opts = { ("subvol=sub2") };

    fstab.updateEntry(key, entry);
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
}
