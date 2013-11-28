
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

    FstabEntry c;
    c.device = c.dentry = "/dev/sdb1";
    c.mount = "/test2";
    c.fs = "ext4";
    c.opts = { ("defaults") };

    fstab.updateEntry(c);
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

    FstabEntry c;
    c.device = c.dentry = "/dev/sdb1";
    c.mount = "swap";
    c.fs = "swap";
    c.opts = { ("noauto") };

    fstab.updateEntry(c);
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

    FstabEntry c;
    c.device = "/dev/sdb1";
    c.dentry = "/dev/mapper/cr_sdb1";
    c.mount = "/test2";
    c.fs = "ext4";
    c.opts = { ("defaults") };
    c.encr = storage::ENC_LUKS;

    fstab.updateEntry(c);
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

    FstabEntry c;
    c.device = "/dev/sdb1";
    c.dentry = "/dev/mapper/cr_sdb1";
    c.mount = "/test2";
    c.fs = "ext4";
    c.opts = { ("defaults") };
    c.encr = storage::ENC_LUKS;
    c.tmpcrypt = true;

    fstab.updateEntry(c);
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

    FstabEntry entry;
    fstab.findUuidLabel("1234", "", entry);
    fstab.setDevice(entry, "/dev/sdb1");

    FstabChange c;
    c.device = "/dev/sdb1";
    c.dentry = "UUID=5678";
    c.mount = "/test2";
    c.fs = "ext4";
    c.opts = { ("defaults") };

    fstab.updateEntry(c);
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

    FstabEntry c;
    c.device = c.dentry = "/dev/sdb1";
    c.mount = "/test2";
    c.fs = "ext4";
    c.opts = { ("defaults") };

    fstab.updateEntry(c);
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
