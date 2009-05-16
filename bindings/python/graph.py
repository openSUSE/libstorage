#!/usr/bin/python

from LibStorage import Environment, createStorageInterface, saveGraph, destroyStorageInterface


env = Environment(True)

c = createStorageInterface(env)

saveGraph(c, "storage.gv")

destroyStorageInterface(c)
