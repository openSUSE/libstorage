#!/usr/bin/python

import LibStorage


env = LibStorage.Environment(True)

c = LibStorage.createStorageInterface(env)


containers = LibStorage.dequecontainerinfo()
c.getContainers(containers)

for container in containers:

    print "Device:", container.device

    if container.type == LibStorage.DISK:

        diskinfo = LibStorage.DiskInfo()
        c.getDiskInfo(container.name, diskinfo)
        print "  Size:", LibStorage.byteToHumanString(1024 * diskinfo.sizeK, True, 2, False)
        print "  Cylinder Size:", LibStorage.byteToHumanString(diskinfo.cylSize, True, 2, False)

        partitioninfos = LibStorage.dequepartitioninfo()
        c.getPartitionInfo(container.name, partitioninfos)

        for partitioninfo in partitioninfos:
            print "  Device:", partitioninfo.v.device
            print "    Size:", LibStorage.byteToHumanString(1024 * partitioninfo.v.sizeK, True, 2, False)

    if container.type == LibStorage.MD:

        mdinfos = LibStorage.dequemdinfo()
        c.getMdInfo(mdinfos)

        for mdinfo in mdinfos:
            print "  Device:", mdinfo.v.device
            print "    Size:", LibStorage.byteToHumanString(1024 * mdinfo.v.sizeK, True, 2, False)
            print "    Chunk Size:", LibStorage.byteToHumanString(1024 * mdinfo.chunkSizeK, True, 2, True)

    if container.type == LibStorage.LVM:

        lvmvginfo = LibStorage.LvmVgInfo()
        c.getLvmVgInfo(container.name, lvmvginfo)
        print "  Size:", LibStorage.byteToHumanString(1024 * lvmvginfo.sizeK, True, 2, False)
        print "  PE Size:", LibStorage.byteToHumanString(1024 * lvmvginfo.peSizeK, True, 2, True)

        lvmlvinfos = LibStorage.dequelvmlvinfo()
        c.getLvmLvInfo(container.name, lvmlvinfos)

        for lvmlvinfo in lvmlvinfos:
            print "  Device:", lvmlvinfo.v.device
            print "    Size:", LibStorage.byteToHumanString(1024 * lvmlvinfo.sizeK, True, 2, False)
            print "    Stripes:", lvmlvinfo.stripes
            if lvmlvinfo.stripes > 1:
                print "    Stripe Size:", LibStorage.byteToHumanString(1024 * lvmlvinfo.stripeSizeK, True, 2, True)

    print


LibStorage.destroyStorageInterface(c)
