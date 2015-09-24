
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/CmdParted.h"
#include "storage/Utils/SystemCmd.h"


using namespace std;
using namespace storage;


void
parse_msdos_disk_label_good()
{
    TRACE();

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
    parted.parse(lines, {});

    cout << parted << endl;
}


void
parse_gpt_good()
{
    TRACE();

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
    parted.parse(lines, {});

    cout << parted << endl;
}


void
parse_gpt_enlarge_good()
{
    TRACE();

    vector<string> stdout = {
	"Model: Maxtor 6 Y080L0 (scsi)",
	"Disk /dev/sdb: 9964cyl",
	"Sector size (logical/physical): 512B/512B",
	"BIOS cylinder,head,sector geometry: 9964,255,63.  Each cylinder is 8225kB.",
	"Partition Table: gpt",
	"Disk Flags: ",
	"",
	"Number  Start  End  Size  File system  Name  Flags",
	"",
	"Model: Maxtor 6 Y080L0 (scsi)",
	"Disk /dev/sdb: 160086528s",
	"Sector size (logical/physical): 512B/512B",
	"Partition Table: gpt",
	"Disk Flags: ",
	"",
	"Number  Start  End  Size  File system  Name  Flags",
	""
    };

    vector<string> stderr = {
	"Warning: Not all of the space available to /dev/sdb appears to be used, you can fix the GPT to use all of the space (an extra 78164480 blocks) or continue with the current setting? ",
	"Warning: Not all of the space available to /dev/sdb appears to be used, you can fix the GPT to use all of the space (an extra 78164480 blocks) or continue with the current setting? "
    };

    Parted parted("/dev/sdb", false);
    parted.parse(stdout, stderr);

    cout << parted << endl;
}


void
parse_gpt_fix_backup_good()
{
    TRACE();

    vector<string> stdout = {
	"Model: Maxtor 6 Y080L0 (scsi)",
	"Disk /dev/sdb: 9964cyl",
	"Sector size (logical/physical): 512B/512B",
	"BIOS cylinder,head,sector geometry: 9964,255,63.  Each cylinder is 8225kB.",
	"Partition Table: gpt",
	"Disk Flags: ",
	"",
	"Number  Start  End  Size  File system  Name  Flags",
	"",
	"Model: Maxtor 6 Y080L0 (scsi)",
	"Disk /dev/sdb: 160086528s",
	"Sector size (logical/physical): 512B/512B",
	"Partition Table: gpt",
	"Disk Flags: ",
	"",
	"Number  Start  End  Size  File system  Name  Flags",
	""
    };

    vector<string> stderr = {
	"Error: The backup GPT table is corrupt, but the primary appears OK, so that will be used.",
	"Error: The backup GPT table is corrupt, but the primary appears OK, so that will be used."
    };

    Parted parted("/dev/sdb", false);
    parted.parse(stdout, stderr);

    cout << parted << endl;
}


void
parse_dasd_good()
{
    TRACE();

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
    parted.parse(lines, {});

    cout << parted << endl;
}


void
parse_loop_good()
{
    TRACE();

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
    parted.parse(lines, {});

    cout << parted << endl;
}


void
parse_dasd_implicit_good()
{
    TRACE();

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
    parted.parse(lines, {});

    cout << parted << endl;
}


void
parse_wiped_disk_good()
{
    TRACE();

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
    parted.parse(lines, {});

    cout << parted << endl;
}


void
parse_bad_device()
{
    TRACE();

    try
    {
	Parted parted( "/dev/wrglbrmpf", true );
	EXCEPTION_EXPECTED();
    }
    catch ( const SystemCmdException &e )
    {
	ST_CAUGHT( e );
	// Expecting a SystemCmdException:
	// parted complains "cannot stat device" on stderr
	// or SystemCmd could not find the parted binary
	// (avoiding to add it to libstorage BuildRequires)
	// -> intentionally not printing exception here
	cout << "CAUGHT EXCEPTION (expected)" << endl << endl;
    }
}


void
parse_no_geometry()
{
    TRACE();

    // Missing the geometry line completely
    vector<string> lines = {
	"Model: ATA WDC WD10EADS-00M (scsi)",
	"Disk /dev/sda: 121601cyl",
	"Sector size (logical/physical): 512B/4096B",
	// "BIOS cylinder,head,sector geometry: 121601,255,63.  Each cylinder is 8225kB.",
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

    try
    {
	parted.parse(lines, {});
	EXCEPTION_EXPECTED();
    }
    catch ( const ParseException &ex )
    {
	ST_CAUGHT( ex );
	cout << "CAUGHT EXCEPTION (expected): " << ex << endl;
    }
}


void
parse_bad_geometry()
{
    TRACE();

    vector<string> lines = {
	"Model: ATA WDC WD10EADS-00M (scsi)",
	"Disk /dev/sda: 121601cyl",
	"Sector size (logical/physical): 512B/4096B",
	"BIOS cylinder,head,sector geometry:  Each cylinder is 8225kB.", // malformed
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

    try
    {
	parted.parse(lines, {});
	EXCEPTION_EXPECTED();
    }
    catch ( const ParseException &ex )
    {
	ST_CAUGHT( ex );
	cout << "CAUGHT EXCEPTION (expected): " << ex << endl;
    }
}


void
parse_bad_sector_size_line()
{
    TRACE();

    vector<string> lines = {
	"Model: ATA WDC WD10EADS-00M (scsi)",
	"Disk /dev/sda: 121601cyl",
	"Sector size (logical/physical): 512B, 4096B",
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

    try
    {
	parted.parse(lines, {});
	EXCEPTION_EXPECTED();
    }
    catch ( const ParseException &ex )
    {
	ST_CAUGHT( ex );
	cout << "CAUGHT EXCEPTION (expected): " << ex << endl;
    }
}


void
parse_bad_msdos_part_entry_1()
{
    TRACE();

    vector<string> lines = {
	"Model: ATA WDC WD10EADS-00M (scsi)",
	"Disk /dev/sda: 121601cyl",
	"Sector size (logical/physical): 512B/4096B",
	"BIOS cylinder,head,sector geometry: 121601,255,63.  Each cylinder is 8225kB.",
	"Partition Table: msdos",
	"",
	"Number  Start   End     Size    Type     File system  Flags",
	" 1      0       131     130     primary  ext3         boot, type=83", // no "cyl" units
	" 2      131     117618  117487  primary               lvm, type=8e",  // no "cyl" units
	""
    };

    Parted parted("/dev/sda", false);

    try
    {
	parted.parse(lines, {});
	EXCEPTION_EXPECTED();
    }
    catch ( const ParseException &ex )
    {
	ST_CAUGHT( ex );
	cout << "CAUGHT EXCEPTION (expected): " << ex << endl;
    }
}


void
parse_bad_msdos_part_entry_2()
{
    TRACE();

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
	"Number  Start    End         Size        Type     File system  Flags",
	" 1      2048     2105343     2103296     primary  ext3         boot, type=83", // no "s" units
	" 2      2105344  1889548287  1887442944  primary               lvm, type=8e",  // no "s" units
	""
    };

    Parted parted("/dev/sda", false);

    try
    {
	parted.parse(lines, {});
	EXCEPTION_EXPECTED();
    }
    catch ( const ParseException &ex )
    {
	ST_CAUGHT( ex );
	cout << "CAUGHT EXCEPTION (expected): " << ex << endl;
    }
}


void
parse_bad_gpt_part_entry()
{
    TRACE();

    vector<string> lines = {
	"Model: ATA ST3500320NS (scsi)",
	"Disk /dev/sda: 60801cyl",
	"Sector size (logical/physical): 512B/512B",
	"BIOS cylinder,head,sector geometry: 60801,255,63.  Each cylinder is 8225kB.",
	"Partition Table: gpt_sync_mbr",
	"",
	"Number  Start  End    Size   File system     Name     Flags",
	" 1      0      63     63     fat16           primary",                    // no "cyl" units
	" 2      63     127    63     ext3            primary  boot, legacy_boot", // no "cyl" units
	" 3      127    18369  18241  ext3            primary",                    // no "cyl" units
	""
    };

    Parted parted("/dev/sda", false);

    try
    {
	parted.parse(lines, {});
	EXCEPTION_EXPECTED();
    }
    catch ( const ParseException &ex )
    {
	ST_CAUGHT( ex );
	cout << "CAUGHT EXCEPTION (expected): " << ex << endl;
    }
}


void
parse_inconsistent_partition_tables()
{
    TRACE();

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
	"",
	"Model: ATA ST3500320NS (scsi)",
	"Disk /dev/sda: 976773168s",
	"Sector size (logical/physical): 512B/512B",
	"Partition Table: gpt_sync_mbr",
	"",
	"Number  Start       End         Size        File system     Name     Flags",
	" 1      2048s       1028095s    1026048s    fat16           primary",
	" 2      1028096s    2056191s    1028096s    ext3            primary  boot, legacy_boot",
	// Partition #3 is not in first (cylinder-based) partition table
	" 3      2056192s    295098367s  293042176s  ext3            primary",
	""
    };

    Parted parted("/dev/sda", false);

    try
    {
	parted.parse(lines, {});
	EXCEPTION_EXPECTED();
    }
    catch ( const ParseException &ex )
    {
	ST_CAUGHT( ex );
	cout << "CAUGHT EXCEPTION (expected): " << ex << endl;
    }
}


void
parse_third_partition_table()
{
    TRACE();

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
	"",
	// One partition table too many
	"Model: ATA WDC WD10EADS-00M (scsi)",
	"Disk /dev/sda: 1953525168s",
	"Sector size (logical/physical): 512B/4096B",
	"Partition Table: msdos",
	"",
	"Number  Start       End            Size           Type     File system  Flags",
	" 1      2048foo     2105343foo     2103296foo     primary  ext3         boot, type=83",
	" 2      2105344foo  1889548287foo  1887442944foo  primary               lvm, type=8e",
	""
    };

    Parted parted("/dev/sda", false);

    try
    {
	parted.parse(lines, {});
	EXCEPTION_EXPECTED();
    }
    catch ( const ParseException &ex )
    {
	ST_CAUGHT( ex );
	cout << "CAUGHT EXCEPTION (expected): " << ex << endl;
    }
}



int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse_msdos_disk_label_good();
    parse_gpt_good();
    parse_gpt_enlarge_good();
    parse_gpt_fix_backup_good();
    parse_dasd_good();
    parse_loop_good();
    parse_dasd_implicit_good();
    parse_wiped_disk_good();

    parse_bad_device();
    parse_no_geometry();
    parse_bad_geometry();
    parse_bad_sector_size_line();

    parse_bad_msdos_part_entry_1();
    parse_bad_msdos_part_entry_2();
    parse_bad_gpt_part_entry();

    parse_inconsistent_partition_tables();
    parse_third_partition_table();
}
