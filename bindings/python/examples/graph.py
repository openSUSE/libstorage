#!/usr/bin/python

from storage import Environment, createStorageInterface, saveDeviceGraph, saveMountGraph, destroyStorageInterface


env = Environment(True)

c = createStorageInterface(env)

saveDeviceGraph(c, "device.gv")
saveMountGraph(c, "mount.gv")

destroyStorageInterface(c)
