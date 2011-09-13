#!/usr/bin/python

import libstorage

libstorage.initDefaultLogger()
env = libstorage.Environment(True)
storageInterface = libstorage.createStorageInterface(env)

rootdev = "/dev/system/root"
swapdev = "/dev/system/swap"
bootdev = "/dev/sda1"
nodev = "/dev/system/xyz123"
disk = "/dev/sdb"

print storageInterface.getIgnoreFstab( rootdev )
print storageInterface.getCrypt( rootdev )
print storageInterface.getMountPoint( rootdev )
print storageInterface.getMountPoint( swapdev )
print storageInterface.getMountPoint( "/dev/system/space" )
print storageInterface.getMountPoint( nodev )
print storageInterface.nextFreePartition( disk, libstorage.PRIMARY )
print storageInterface.nextFreeMd()
print "MOUNTBY DEVICE:", libstorage.MOUNTBY_DEVICE, " UUID:", libstorage.MOUNTBY_UUID, " LABEL:", libstorage.MOUNTBY_LABEL, " ID:", libstorage.MOUNTBY_ID, " PATH:", libstorage.MOUNTBY_PATH
print storageInterface.getMountBy( rootdev )
print storageInterface.getMountBy( bootdev )
print storageInterface.getDefaultMountBy()

libstorage.destroyStorageInterface(storageInterface)
