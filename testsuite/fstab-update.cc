
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

    FstabChange entry;
    entry.device = entry.dentry = "/dev/sdb1";
    entry.mount = "/test2";
    entry.fs = "ext4";
    entry.opts = { ("defaults") };

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

    write_fstab({ "/dev/sdb1  swap  swap  defaults  0 0" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "swap");

    FstabChange entry;
    entry.device = entry.dentry = "/dev/sdb1";
    entry.mount = "swap";
    entry.fs = "swap";
    entry.opts = { ("noauto") };

    fstab.updateEntry(key, entry);
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

    FstabChange entry;
    entry.device = "/dev/sdb1";
    entry.dentry = "/dev/mapper/cr_sdb1";
    entry.mount = "/test2";
    entry.fs = "ext4";
    entry.opts = { ("defaults") };
    entry.encr = storage::ENC_LUKS;

    fstab.updateEntry(key, entry);
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
    write_crypttab({ "cr_sdb1  /dev/sdb1  /dev/urandom tmp" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "/test1");

    FstabChange entry;
    entry.device = "/dev/sdb1";
    entry.dentry = "/dev/mapper/cr_sdb1";
    entry.mount = "/test2";
    entry.fs = "ext4";
    entry.opts = { ("defaults") };
    entry.encr = storage::ENC_LUKS;
    entry.tmpcrypt = true;

    fstab.updateEntry(key, entry);
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

    FstabChange entry;
    entry.device = "/dev/sdb1";
    entry.dentry = "UUID=5678";
    entry.mount = "/test2";
    entry.fs = "ext4";
    entry.opts = { ("defaults") };

    fstab.updateEntry(key, entry);
    fstab.flush();

    print_fstab();

    cout << endl;
}


void
test6()
{
    cout << "test6" << endl;

    setup_system("empty");

    write_fstab({ "/dev/mapper/cr_sdb1  /test1  ext4  nofail  0 0" });
    write_crypttab({ "cr_sdb1  /dev/sdb1  /dev/urandom  tmp" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "/test1");

    FstabChange entry;
    entry.device = entry.dentry = "/dev/sdb1";
    entry.mount = "/test2";
    entry.fs = "ext4";
    entry.opts = { ("defaults") };

    fstab.updateEntry(key, entry);
    fstab.flush();

    print_fstab();
    print_crypttab();

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
    test6();
}
