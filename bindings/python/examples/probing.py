#!/usr/bin/python

import libstorage


env = libstorage.Environment(True)

c = libstorage.createStorageInterface(env)


containers = libstorage.DequeContainerInfo()
c.getContainers(containers)

for container in containers:

    print "Device:", container.device

    if container.type == libstorage.DISK:

        diskinfo = libstorage.DiskInfo()
        c.getDiskInfo(container.name, diskinfo)
        print "  Size:", libstorage.byteToHumanString(1024 * diskinfo.sizeK, True, 2, False)
        print "  Cylinder Size:", libstorage.byteToHumanString(diskinfo.cylSize, True, 2, False)

        partitioninfos = libstorage.DequePartitionInfo()
        c.getPartitionInfo(container.name, partitioninfos)

        for partitioninfo in partitioninfos:
            print "  Device:", partitioninfo.v.device
            print "    Size:", libstorage.byteToHumanString(1024 * partitioninfo.v.sizeK, True, 2, False)

    if container.type == libstorage.MD:

        mdinfos = libstorage.dequemdinfo()
        c.getMdInfo(mdinfos)

        for mdinfo in mdinfos:
            print "  Device:", mdinfo.v.device
            print "    Size:", libstorage.byteToHumanString(1024 * mdinfo.v.sizeK, True, 2, False)
            print "    Chunk Size:", libstorage.byteToHumanString(1024 * mdinfo.chunkSizeK, True, 2, True)

    if container.type == libstorage.LVM:

        lvmvginfo = libstorage.LvmVgInfo()
        c.getLvmVgInfo(container.name, lvmvginfo)
        print "  Size:", libstorage.byteToHumanString(1024 * lvmvginfo.sizeK, True, 2, False)
        print "  PE Size:", libstorage.byteToHumanString(1024 * lvmvginfo.peSizeK, True, 2, True)

        lvmlvinfos = libstorage.DequeLvmLvInfo()
        c.getLvmLvInfo(container.name, lvmlvinfos)

        for lvmlvinfo in lvmlvinfos:
            print "  Device:", lvmlvinfo.v.device
            print "    Size:", libstorage.byteToHumanString(1024 * lvmlvinfo.v.sizeK, True, 2, False)
            print "    Stripes:", lvmlvinfo.stripes
            if lvmlvinfo.stripes > 1:
                print "    Stripe Size:", libstorage.byteToHumanString(1024 * lvmlvinfo.stripeSizeK, True, 2, True)

    print


libstorage.destroyStorageInterface(c)
