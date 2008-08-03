#
# Makefile.am for libstorage/examples
#

PACKAGE = libstorage

INCLUDES = -I$(top_srcdir)/libstorage/src -I/usr/include/YaST2
LDADD = ../src/liby2storage.la

AM_CXXFLAGS =

noinst_PROGRAMS = TestDisk TestDiskLog TestStorage TestProbing TestLvm TestMd TestLoop TestLock

TestDisk_SOURCES = TestDisk.cc

TestDiskLog_SOURCES = TestDiskLog.cc

TestStorage_SOURCES = TestStorage.cc

TestProbing_SOURCES = TestProbing.cc

TestLvm_SOURCES = TestLvm.cc

TestMd_SOURCES = TestMd.cc

TestLoop_SOURCES = TestLoop.cc

TestLock_SOURCES = TestLock.cc

exampledir = $(docdir)/examples/libstorage

example_DATA = TestDisk.cc TestDiskLog.cc TestProbing.cc TestLvm.cc TestMd.cc TestLoop.cc 

EXTRA_DIST = $(example_DATA) Makefile.example

install-data-local:
	/usr/bin/install -d -p -m 755 $(DESTDIR)$(exampledir)
	/usr/bin/install -p -m 644 Makefile.example $(DESTDIR)$(exampledir)/Makefile
