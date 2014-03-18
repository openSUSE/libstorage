
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


void
parse2()
{
    cout << "parse2" << endl;

    vector<string> lines = {
	"/dev/sda1: UUID=\"fc5985ee-e069-4bb4-a36c-24d8f6022f7a\" TYPE=\"ext4\" ",
	"/dev/sda2: UUID=\"f98def5a-6553-49a1-940d-df55a77d7d28\" TYPE=\"crypto_LUKS\" ",
	"/dev/mapper/tmp: UUID=\"wunWKW-nDeG-SUnC-CxDO-5UNt-BA4Y-Se7xVC\" TYPE=\"LVM2_member\" ",
	"/dev/mapper/system-home: UUID=\"7e9e18c3-b743-47d8-9f33-464f466fc517\" UUID_SUB=\"4f40002c-125e-43c1-8a38-b5bf29f5319c\" TYPE=\"btrfs\" ",
	"/dev/mapper/system-root: UUID=\"9fca85ff-4278-4f49-932e-4060726cf0d6\" UUID_SUB=\"ab28e20a-9c11-4ea6-a46d-0fe412fe0e19\" TYPE=\"btrfs\" ",
	"/dev/mapper/system-swap: UUID=\"d0f020a1-9847-4ee5-a22e-fe0cdd4aa905\" TYPE=\"swap\" "
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
    parse2();
}
