#!/usr/bin/python

import Storage

c = Storage.createStorageInterface (1, 0, 1)

disks = Storage.dequestring()
c.getDisks (disks)

for disk in disks:

    print disk
