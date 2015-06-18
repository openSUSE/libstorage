#!/usr/bin/python

import storage


env = storage.Environment(True)

c = storage.createStorageInterface(env)


containers = storage.DequeContainerInfo()
c.getContainers(containers)

for container in containers:

    print "Device:", container.device

    if container.type == storage.DISK:

        diskinfo = storage.DiskInfo()
        c.getDiskInfo(container.name, diskinfo)
        print "  Size:", storage.byteToHumanString(1024 * diskinfo.sizeK, True, 2, False)
        print "  Cylinder Size:", storage.byteToHumanString(diskinfo.cylSize, True, 2, False)

        partitioninfos = storage.DequePartitionInfo()
        c.getPartitionInfo(container.name, partitioninfos)

        for partitioninfo in partitioninfos:
            print "  Device:", partitioninfo.v.device
            print "    Size:", storage.byteToHumanString(1024 * partitioninfo.v.sizeK, True, 2, False)

    if container.type == storage.MD:

        mdinfos = storage.dequemdinfo()
        c.getMdInfo(mdinfos)

        for mdinfo in mdinfos:
            print "  Device:", mdinfo.v.device
            print "    Size:", storage.byteToHumanString(1024 * mdinfo.v.sizeK, True, 2, False)
            print "    Chunk Size:", storage.byteToHumanString(1024 * mdinfo.chunkSizeK, True, 2, True)

    if container.type == storage.LVM:

        lvmvginfo = storage.LvmVgInfo()
        c.getLvmVgInfo(container.name, lvmvginfo)
        print "  Size:", storage.byteToHumanString(1024 * lvmvginfo.sizeK, True, 2, False)
        print "  PE Size:", storage.byteToHumanString(1024 * lvmvginfo.peSizeK, True, 2, True)

        lvmlvinfos = storage.DequeLvmLvInfo()
        c.getLvmLvInfo(container.name, lvmlvinfos)

        for lvmlvinfo in lvmlvinfos:
            print "  Device:", lvmlvinfo.v.device
            print "    Size:", storage.byteToHumanString(1024 * lvmlvinfo.v.sizeK, True, 2, False)
            print "    Stripes:", lvmlvinfo.stripes
            if lvmlvinfo.stripes > 1:
                print "    Stripe Size:", storage.byteToHumanString(1024 * lvmlvinfo.stripeSizeK, True, 2, True)

    print


storage.destroyStorageInterface(c)
