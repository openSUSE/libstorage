#!/usr/bin/python

import storage

storage.initDefaultLogger()
env = storage.Environment(True)
storageInterface = storage.createStorageInterface(env)

rootdev = "/dev/system/root"
swapdev = "/dev/system/swap"
bootdev = "/dev/sda1"
nodev = "/dev/system/xyz123"
disk = "/dev/sdb"

print storageInterface.getIgnoreFstab(rootdev)
print storageInterface.getCrypt(rootdev)
print storageInterface.getMountPoint(rootdev)
print storageInterface.getMountPoint(swapdev)
print storageInterface.getMountPoint("/dev/system/space")
print storageInterface.getMountPoint(nodev)
print storageInterface.nextFreePartition(disk, storage.PRIMARY)
print storageInterface.nextFreeMd()
print "MOUNTBY DEVICE:", storage.MOUNTBY_DEVICE, " UUID:", storage.MOUNTBY_UUID, " LABEL:", storage.MOUNTBY_LABEL, " ID:", storage.MOUNTBY_ID, " PATH:", storage.MOUNTBY_PATH
print storageInterface.getMountBy(rootdev)
print storageInterface.getMountBy(bootdev)
print storageInterface.getDefaultMountBy()

storage.destroyStorageInterface(storageInterface)
