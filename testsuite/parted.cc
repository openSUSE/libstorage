
#include <iostream>

#include "common.h"

#include "storage/Parted.h"


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


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
    parse2();
}
