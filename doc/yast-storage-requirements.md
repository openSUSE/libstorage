# YaST Storage Features and Requirements #

This is about _libstorage_ and the _yast-storage_ Ruby code.



## Disks and Partitions

### Simple case:

- Disk
  - Partition table
  - Partition
      - File system
          - File
          - File
          - File
          - ...
  - Partition
      - File system
          - File
          - ...
  - Partition
      - ...



## Disk Labels

(ways to create partition tables)

- MS-DOS (PC classic/legacy)
 
    - can be used on all supported architectures: i586, x86_64, ppc, s/390
    - 4 partition entries in MBR (Master Boot Record)
    - one partition can be an "extended partition": A container for _logical partitions_
    - each logical partition links to the next one
    - the first logical partition is always /dev/sd..5 (no matter if /dev/sd..4 etc. exist)

- GPT  (GUID Partition Table; new PCs with UEFI boot support)
    - can be used on all supported architectures: i586, x86_64, ppc, s/390
    - large number of partitions (128 or more)
    - no longer limited by available space in MBR
    - partitions keep their numbers when other partitions are removed or added
    - parted will use the smallest available number when creating a new partition

- Other:
    - DASD (s/390 specific)
 
- Not supported or very limited support (which may be dropped in the future) in YaST storage:
    - BSD
        - Fixed number of 8 or 16 partition slots
        - Fixed role (by convention) for some slots:
            - slot a: root
            - slot b: swap
            - slot c: complete disk
    - Sun
    - AIX
    - Mac
    - Amiga



## File Systems

Supported:

- ext2
- ext3
- ext4
- Btrfs
- XFS
- JFS
- ReiserFS
- FAT/VFAT
- NTFS
- tmpFS
- HFS
- HFS+
- NFS

Supported actions:

- create
- resize (for some types)
- /etc/fstab -handling
- mount
- unmount



## RAID

(Redundant Array of Independent Disks)

- Hardware RAID: mostly automatic and transparent
- Software RAID
- BIOS RAID: type dependent on BIOS:
    - mdadm RAID
    - dmraid (Device Mapper RAID)


RAID levels:

- **RAID 0**: striping; performance improvement due to parallel disk I/O; less fault tolerance than one disk
- **RAID 1**: mirroring; improved fault tolerance, same performance as one disk
- **RAID 5**: 3 or more disks with redundancy (for parity blocks): Good fault tolerance, improved performance (parallel I/O)
- **RAID 6**: 4 or more disks; more redundancy than RAID 5, thus even better fault tolerance
- **RAID 10**: striping + mirroring; combination of RAID 1 and RAID 0

Supported actions:

- create RAID 0, 1, 5, 6, 10
- create partitions on RAID (if there already is a partition table; cannot create a partition table if there isn't one already)

Not supported by YaST storage - use dedicated tools instead:

- add disk
- remove disk
- create BIOS RAID (use BIOS or mdadm/dmraid)



## Volume Managers

- LVM
- Btrfs (volume manager features not supported by YaST storage)
- EVMS: legacy; no longer supported

Supported actions (LVM only):

- create LV (Logical Volume)
- delete LV
- create VG (Volume Group)
- add VG
- remove VG
- add PV (Physical Volume) to VG
- remove PV from VG
- thin provisioning (overcommited disk space)

Not supported (use low-level tools instead):

- force LV to specific PV
- merge VGs
- split VG
- resize PV
- move content between PVs
- ...



## Encryption

- full disk encryption (LVM only: encrypted PVs)
- per partition (not root file system) with device mapper
- using LUKS (Linux Unified Key Setup)
- limited support for legacy method: loop devices and crypto kernel modules

Actions:

- create
- mount
- unmount
- /etc/crypttab and /etc/fstab handling

Not supported (use LUKS commands instead):

- LUKS multiple password slots handling
- change password
- set cipher (encryption method)



## Multiple Dimensions

- LVM on RAID
- Btrfs (which has its own volume manager) on LVM
- Btrfs on LVM on RAID
- Btrfs on encrypted LVM on RAID
- ... (use your imagination)



## Boot Partitions

_**very architecture/system/bootloader specific**_

- when is a boot partition needed?
- on what disk should it be?
- where on the disk should it be? (Boot sequence!)
- size constraints? (too small vs. too large; ppc for example loads the _PReP_ partition into the RAM completely, so it should be as small as possible)
- dependent on boot loader type
- dependent on encryption (BIOS can't load files from encrypted partitions)
- dependent on presence of LVM



## Bootloader Constraints

_**Caution: Voodoo inside**_


- MS-DOS (PC legacy) disk label:
    - BIOS loads the first sector of the disk (the MBR (Master Boot Record)) and executes it
    - only 512 bytes - sizeof( partition_table ) = 446 bytes for this first stage of the boot loader, i.e., the next stage (Grub2, Grub, LILO) has to be chain-loaded
    - the next stage has to be accessible by the BIOS
    - limitations depending on BIOS:
        - legacy: 1024 cylinders in CHS (Cylinder/Head/Sector) mode (obsolete today)
        - LBA (Logical Block Address):
            - 32 bit LBA (obsolete today): 2 TB limit
            - latest (7/2015): 48 bit LBA; 144 PB (144000 TB) limit
        - there have always been limits, they always seemed way out of reach, and we always reached them before we thought it possible.


- UEFI (Unified Extensible Firmware Interface): 
    - typically used with GPT disk label
    - requires EFI system partition (with FAT file system)

- PPC (Power PC)
    - can use MS-DOS or GPT disk label
    - requires PReP partition

         _**TO DO: Get more information from the PPC arch maintainers**_

- s/390
    - uses a z/IPL (Initial Boot Loader for z Series) initial boot loader
    - uses a /boot/zipl partition (typically with ext2 file system)



## Automated Storage Proposal

_**Caution: Voodoo inside**_

- a lot of code that tries to make a reasonable suggestion for many different storage setups
- trying to be everybody's darling (well, at least most people's)

Supported scenarios:

- simple partition-based proposal
- with or without LVM
- with or without Btrfs
- with or without LVM full disk encryption
- with or without automated file system snapshots (if using Btrfs)

Not supported:

- RAID



## Multipath

- network storage devices (typically fibre channel) where multiple low-level kernel devices for the same storage device exist that have redundant connections to the machine
- improved performance (parallel I/O over multiple channels)
- fail safe against some I/O channels being blocked
- special hardware that needs to be set up with special tools (outside the scope of YaST storage)
- for most operations, it is important to use only the one device mapper device for that storage device, not the multiple low-level devices

Supported actions:
- start
- stop



## DASD Configuration

_**s/390 specific**_

- DASDs (Direct Access Storage Devices) need to be activated for use with Linux with the _dasd_configure_ command
- DASDs are created/managed by the s/390 host system



## iSCSI
(Internet SCSI)

- networked storage using SCSI commands over TCP/IP
- using a separate set of commands and tools
- currently out of the scope of YaST storage, using a separate YaST module
- might need to be integrated into YaST storage some day



## FCoE
(Fibre Channel Over Ethernet)

- networked storage using fibre channel over ethernet
- using a separate set of commands and tools
- currently out of the scope of YaST storage, using a separate YaST module
- might need to be integrated into YaST storage some day


