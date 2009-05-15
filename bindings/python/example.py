#!/usr/bin/python

import LibStorage


env = LibStorage.Environment(True)

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
            print "  ", partitioninfo.v.device, partitioninfo.v.sizeK,  \
                partitioninfo.cylStart, partitioninfo.cylSize

    if container.type == LibStorage.LVM:

        lvmvginfo = LibStorage.LvmVgInfo()
        c.getLvmVgInfo(container.name, lvmvginfo)
        print "  ", lvmvginfo.sizeK

        lvmlvinfos = LibStorage.dequelvmlvinfo()
        c.getLvmLvInfo(container.name, lvmlvinfos)

        for lvmlvinfo in lvmlvinfos:
            print "  ", lvmlvinfo.v.device, lvmlvinfo.v.sizeK


print


fscapabilities = LibStorage.FsCapabilities()
c.getFsCapabilities(LibStorage.EXT4, fscapabilities)
print fscapabilities.isExtendable, fscapabilities.minimalFsSizeK,       \
    fscapabilities.isExtendableWhileMounted

print LibStorage.byteToHumanString(1234567890, True, 4, False)

print LibStorage.saveGraph(c, "storage.gv")


LibStorage.destroyStorageInterface(c)
