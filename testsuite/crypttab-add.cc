
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

    FstabChange entry;
    entry.device = "/dev/sdb1";
    entry.dentry = "/dev/mapper/cr_sdb1";
    entry.encr = storage::ENC_LUKS;

    fstab.addEntry(entry);
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
}
