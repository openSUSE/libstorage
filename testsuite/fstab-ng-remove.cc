
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
		  "/dev/sdb1  /test1/sub1  btrfs  subvol=sub1  0 0",
		  "/dev/sdb1  /test1/sub2  btrfs  subvol=sub2  0 0" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "/test1/sub1");

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

    write_fstab({ "/dev/sdb1  /test1  btrfs  defaults  0 0",
		  "/dev/sdb1  /test1/sub1  btrfs  subvol=sub1  0 0",
		  "/dev/sdb1  /test1/sub2  btrfs  subvol=sub2  0 0" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "/test1/sub2");

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

    write_fstab({ "LABEL=test  /test1  btrfs  defaults  0 0",
		  "LABEL=test  /test1/sub1  btrfs  defaults  0 0",
		  "LABEL=test  /test1/sub2  btrfs  defaults  0 0" });
    EtcFstab fstab("tmp/etc");

    fstab.setDevice("/dev/sdb1", {}, "", "test", {}, "");

    FstabKey key("/dev/sdb1", "/test1/sub1");

    fstab.removeEntry(key);
    fstab.flush();

    print_fstab();

    cout << endl;
}


void
test4()
{
    cout << "test4" << endl;

    setup_system("empty");

    write_fstab({ "LABEL=test  /test1  btrfs  defaults  0 0",
		  "LABEL=test  /test1/sub1  btrfs  defaults  0 0",
		  "LABEL=test  /test1/sub2  btrfs  defaults  0 0",
		  "LABEL=test  /test1/sub3  btrfs  defaults  0 0" });
    EtcFstab fstab("tmp/etc");

    fstab.setDevice("/dev/sdb1", {}, "", "test", {}, "");

    FstabKey key1("/dev/sdb1", "/test1");
    FstabKey key2("/dev/sdb1", "/test1/sub1");
    FstabKey key3("/dev/sdb1", "/test1/sub2");
    FstabKey key4("/dev/sdb1", "/test1/sub3");

    fstab.removeEntry(key1);
    fstab.removeEntry(key2);
    fstab.removeEntry(key3);
    fstab.removeEntry(key4);
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
}
