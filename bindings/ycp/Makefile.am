#
# Makefile.am for libstorage/bindings/ycp
#

PLUGLIB_MODULE = LibStorage
PLUGLIB_LIBRARY = ../../src/.libs/libstorage.so
PLUGLIB_DEPEND =					\
	../../src/StorageInterface.h			\
	../../src/HumanString.h				\
	../../src/Graph.h

include $(top_srcdir)/pluglib-bindings.ami

EXTRA_DIST = LibStorage.i example.ycp
