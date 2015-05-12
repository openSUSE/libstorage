
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/CmdParted.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"Model: ATA WDC WD10EADS-00M (scsi)",
	"Disk /dev/sda: 121601cyl",
	"Sector size (logical/physical): 512B/4096B",
	"BIOS cylinder,head,sector geometry: 121601,255,63.  Each cylinder is 8225kB.",
	"Partition Table: msdos",
	"",
	"Number  Start   End        Size       Type     File system  Flags",
	" 1      0cyl    131cyl     130cyl     primary  ext3         boot, type=83",
	" 2      131cyl  117618cyl  117487cyl  primary               lvm, type=8e",
	"",
	"Model: ATA WDC WD10EADS-00M (scsi)",
	"Disk /dev/sda: 1953525168s",
	"Sector size (logical/physical): 512B/4096B",
	"Partition Table: msdos",
	"",
	"Number  Start     End          Size         Type     File system  Flags",
	" 1      2048s     2105343s     2103296s     primary  ext3         boot, type=83",
	" 2      2105344s  1889548287s  1887442944s  primary               lvm, type=8e",
	""
    };

    Parted parted("/dev/sda", false);
    parted.parse(lines);

    cout << parted << endl;
}


void
parse2()
{
    cout << "parse2" << endl;

    vector<string> lines = {
	"Model: ATA ST3500320NS (scsi)",
	"Disk /dev/sda: 60801cyl",
	"Sector size (logical/physical): 512B/512B",
	"BIOS cylinder,head,sector geometry: 60801,255,63.  Each cylinder is 8225kB.",
	"Partition Table: gpt_sync_mbr",
	"",
	"Number  Start     End       Size      File system     Name     Flags",
	" 1      0cyl      63cyl     63cyl     fat16           primary",
	" 2      63cyl     127cyl    63cyl     ext3            primary  boot, legacy_boot",
	" 3      127cyl    18369cyl  18241cyl  ext3            primary",
	" 4      18369cyl  36610cyl  18241cyl                  primary",
	" 5      36610cyl  36872cyl  261cyl    linux-swap(v1)  primary",
	" 6      36872cyl  60801cyl  23929cyl  ext3            primary",
	"",
	"Model: ATA ST3500320NS (scsi)",
	"Disk /dev/sda: 976773168s",
	"Sector size (logical/physical): 512B/512B",
	"Partition Table: gpt_sync_mbr",
	"",
	"Number  Start       End         Size        File system     Name     Flags",
	" 1      2048s       1028095s    1026048s    fat16           primary",
	" 2      1028096s    2056191s    1028096s    ext3            primary  boot, legacy_boot",
	" 3      2056192s    295098367s  293042176s  ext3            primary",
	" 4      295098368s  588140543s  293042176s                  primary",
	" 5      588140544s  592349183s  4208640s    linux-swap(v1)  primary",
	" 6      592349184s  976773119s  384423936s  ext3            primary",
	""
    };

    Parted parted("/dev/sdb", false);
    parted.parse(lines);

    cout << parted << endl;
}


void
parse3()
{
    cout << "parse3" << endl;

    vector<string> lines = {
	"Model: IBM S390 DASD drive (dasd)",
	"Disk /dev/dasda: 178079cyl",
	"Sector size (logical/physical): 512B/4096B",
	"BIOS cylinder,head,sector geometry: 178080,15,12.  Each cylinder is 92.2kB.",
	"Partition Table: dasd",
	"Disk Flags: ",
	"",
	"Number  Start     End        Size       File system     Flags",
	" 1      1cyl      23303cyl   23302cyl   linux-swap(v1)",
	" 2      23304cyl  25583cyl   2280cyl    ext2",
	" 3      25584cyl  178079cyl  152496cyl  ext4",
	"",
	"Model: IBM S390 DASD drive (dasd)",
	"Disk /dev/dasda: 32054400s",
	"Sector size (logical/physical): 512B/4096B",
	"Partition Table: dasd",
	"Disk Flags: ",
	"",
	"Number  Start     End        Size       File system     Flags",
	" 1      192s      4194719s   4194528s   linux-swap(v1)",
	" 2      4194720s  4605119s   410400s    ext2",
	" 3      4605120s  32054399s  27449280s  ext4",
	""
    };

    Parted parted("/dev/dasda", false);
    parted.parse(lines);

    cout << parted << endl;
}


void
parse4()
{
    cout << "parse4" << endl;

    vector<string> lines = {
	"Model: Maxtor 6 Y080L0 (scsi)",
	"Disk /dev/sdb: 9964cyl",
	"Sector size (logical/physical): 512B/512B",
	"BIOS cylinder,head,sector geometry: 9964,255,63.  Each cylinder is 8225kB.",
	"Partition Table: loop",
	"",
	"Number  Start  End      Size     File system  Flags",
	" 1      0cyl   9964cyl  9964cyl  xfs",
	"",
	"Model: Maxtor 6 Y080L0 (scsi)",
	"Disk /dev/sdb: 160086528s",
	"Sector size (logical/physical): 512B/512B",
	"Partition Table: loop",
	"",
	"Number  Start  End         Size        File system  Flags",
	" 1      0s     160086527s  160086528s  xfs",
	""
    };

    Parted parted("/dev/sdb", false);
    parted.parse(lines);

    cout << parted << endl;
}


void
parse5()
{
    cout << "parse5" << endl;

    vector<string> lines = {
	"Model: IBM S390 DASD drive (dasd)",
	"Disk /dev/dasdc: 244cyl",
	"Sector size (logical/physical): 512B/512B",
	"BIOS cylinder,head,sector geometry: 244,16,128.  Each cylinder is 1049kB.",
	"Partition Table: dasd",
	"Disk Flags: implicit_partition_table",
	"",
	"Number  Start  End     Size    File system  Flags",
	" 1      0cyl   244cyl  244cyl",
	"",
	"Model: IBM S390 DASD drive (dasd)",
	"Disk /dev/dasdc: 500000s",
	"Sector size (logical/physical): 512B/512B",
	"Partition Table: dasd",
	"Disk Flags: implicit_partition_table",
	"",
	"Number  Start  End      Size     File system  Flags",
	" 1      2s     499999s  499998s"
    };

    Parted parted("/dev/dasdc", false);
    parted.parse(lines);

    cout << parted << endl;
}


void
parse6()
{
    cout << "parse6" << endl;

    // Disk with no partition table at all (brand new or after 'wipefs')
    vector<string> lines = {
	"Error: /dev/sdb: unrecognised disk label",
	"Model: Maxtor 6 Y080L0 (scsi)",
	"Disk /dev/sdb: 9964cyl",
	"Sector size (logical/physical): 512B/512B",
	"BIOS cylinder,head,sector geometry: 9964,255,63.  Each cylinder is 8225kB.",
	"Partition Table: unknown",
	"Disk Flags: "
    };

    Parted parted("/dev/sdb", false);
    parted.parse(lines);

    cout << parted << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
    parse2();
    parse3();
    parse4();
    parse5();
    parse6();
}
