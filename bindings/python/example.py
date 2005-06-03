#!/usr/bin/python

import LibStorage


c = LibStorage.createStorageInterface (1, 0, 1)


disks = LibStorage.dequestring ()
c.getDisks (disks)

for disk in disks:

    print disk

    partitioninfos = LibStorage.dequepartitioninfo ()
    c.getPartitionsOfDisk (disk, partitioninfos)

    for partitioninfo in partitioninfos:

        print partitioninfo.v.name,partitioninfo.cylStart,partitioninfo.cylSize


fscapabilities = LibStorage.FsCapabilities ();
c.getFsCapabilities (LibStorage.REISERFS, fscapabilities);
print fscapabilities.isExtendable,fscapabilities.minimalFsSizeK


LibStorage.destroyStorageInterface (c)
