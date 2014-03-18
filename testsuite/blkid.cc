
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/CmdBlkid.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"/dev/sda1: LABEL=\"BOOT\" UUID=\"14875716-b8e3-4c83-ac86-48c20682b63a\" TYPE=\"ext3\" PTTYPE=\"dos\" ",
	"/dev/sda2: UUID=\"qquP1O-WWoh-Ofas-Rbx0-y72T-0sNe-Wnyc33\" TYPE=\"LVM2_member\" ",
	"/dev/mapper/system-abuild: LABEL=\"ABUILD\" UUID=\"16337c60-fc2a-4b87-8199-4f511fa06c65\" TYPE=\"ext4\" ",
	"/dev/mapper/system-btrfs: LABEL=\"BTRFS\" UUID=\"946de1e3-ab5a-49d2-8c9d-057f1613d395\" UUID_SUB=\"8fd5c226-d060-4049-90e6-1df5c865fdf4\" TYPE=\"btrfs\" ",
	"/dev/mapper/system-giant: LABEL=\"GIANT\" UUID=\"0857a01f-c58e-464a-b74b-cd46992873e7\" TYPE=\"xfs\" ",
	"/dev/mapper/system-root: LABEL=\"ROOT\" UUID=\"31e381c9-5b35-4045-8d01-9274a30e1298\" TYPE=\"ext3\" ",
	"/dev/mapper/system-swap: LABEL=\"SWAP\" UUID=\"fd39c3f9-2990-435d-8eed-e56b6dc2e592\" TYPE=\"swap\" "
    };

    Blkid blkid(false);
    blkid.parse(lines);

    cout << blkid << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
