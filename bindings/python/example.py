#!/usr/bin/python

import LibStorage

c = LibStorage.createStorageInterface (1, 0, 1)

disks = LibStorage.dequestring ()
c.getDisks (disks)

for disk in disks:

    print disk

    partitioninfos = LibStorage.dequepartitioninfo()
    c.getPartitionsOfDisk (disk, partitioninfos)

    for partitioninfo in partitioninfos:

        print partitioninfo.name,partitioninfo.cylStart,partitioninfo.cylSize

LibStorage.destroyStorageInterface (c)
