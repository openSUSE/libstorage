
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

    write_crypttab({ "cr_sdb1  /dev/sdb1  none  none" });

    EtcFstab fstab("tmp/etc");

    FstabKey key("/dev/sdb1", "");

    fstab.removeEntry(key);
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
