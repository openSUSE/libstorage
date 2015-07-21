# Current YaST Storage Code: The Good, the Bad and the Ugly

## General

Some parts are written in C++ (libstorage), some other in Ruby (automatically converted from YCP).



## Data Structure

- Containers (disks, volume groups)
- Volumes (partitions, logical volumes)
    - contains file system information

Current limitations:

- A container cannot contain a file system directly without a volume in between,
i.e. there cannot be a disk with a file system directly on it without any partitions.

     This has already been requested as a mandatory feature.

     This also makes it impossible to convert from partitioned to unpartitioned RAID.

- No possibility to represent containers of BIOS RAIDs. Containers of BIOS RAID can include several RAIDs.

- The device name is used as a unique key to identify containers and volumes. This is problematic for:
    - tmpFS: Several tmpFS have the same pseudo device name, but different mount points and options.
    - Btrfs: If Btrfs is used as a volume manager, this does not map any more: One Btrfs can span several devices.

- The data structure supports only one mount point for each file system. Btrfs can have several mount points due to using subvolumes.



## libstorage (C++)

- Good:
    - Can export and import some data to XML files.
    - The SystemInfo class can separate parsing and logic but is not yet used everywhere.

- Bad:
    - Limited testability:
        - Cannot export and import all data to XML files.
        - No mockup of probing and destructive tools.
        - During probing and committing changes, the logic and parsing/executing are intermixed.
    - API not object oriented: One API class with a zillion functions;

        leftover from now obsolete requirements from the abandoned LIMAL project years ago

    - Error handling in interface: No exceptions



## yast2-storage (mostly Ruby)

- Bad:
    - clean up leftovers from YCP-killer (automated conversion YCP -> Ruby)

- Ugly:
    - monolithic code with very long functions
    - probably lots of code duplication over the years
    - largely untestable
    - relies heavily on "target map":
        - nested lists and maps containing all kinds of storage-related data
        - very difficult to debug anything
        - undocumented


