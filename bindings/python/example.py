#!/usr/bin/python

import LibStorage


env = LibStorage.Environment(1)

c = LibStorage.createStorageInterface(env)


containers = LibStorage.dequecontainerinfo()
c.getContainers(containers)

for container in containers:

    print container.device

    if container.type == LibStorage.DISK:

        diskinfo = LibStorage.DiskInfo()
        c.getDiskInfo(container.name, diskinfo)
        print "  ", diskinfo.sizeK

        partitioninfos = LibStorage.dequepartitioninfo()
        c.getPartitionInfo(container.name, partitioninfos)

        for partitioninfo in partitioninfos:
            print "  ", partitioninfo.v.device, partitioninfo.v.sizeK, \
            partitioninfo.cylStart, partitioninfo.cylSize

    if container.type == LibStorage.LVM:

        lvmvginfo = LibStorage.LvmVgInfo()
        c.getLvmVgInfo(container.name, lvmvginfo)
        print "  ", lvmvginfo.sizeK

        lvmlvinfos = LibStorage.dequelvmlvinfo()
        c.getLvmLvInfo(container.name, lvmlvinfos)

        for lvmlvinfo in lvmlvinfos:
            print "  ", lvmlvinfo.v.device, lvmlvinfo.v.sizeK


fscapabilities = LibStorage.FsCapabilities()
c.getFsCapabilities(LibStorage.REISERFS, fscapabilities)
print fscapabilities.isExtendable, fscapabilities.minimalFsSizeK


LibStorage.destroyStorageInterface(c)
