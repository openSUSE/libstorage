#
# Makefile.am for libstorage/examples
#

INCLUDES = -I$(top_srcdir)

LDADD = ../storage/libstorage.la

noinst_PROGRAMS = TestDisk TestDiskLog TestStorage TestProbing TestLvm		\
	TestMd TestLoop TestLock SaveGraph ShowBytes

TestDisk_SOURCES = TestDisk.cc

TestDiskLog_SOURCES = TestDiskLog.cc

TestStorage_SOURCES = TestStorage.cc

TestProbing_SOURCES = TestProbing.cc

TestLvm_SOURCES = TestLvm.cc

TestMd_SOURCES = TestMd.cc

TestLoop_SOURCES = TestLoop.cc

TestLock_SOURCES = TestLock.cc

SaveGraph_SOURCES = SaveGraph.cc

ShowBytes_SOURCES = ShowBytes.cc

exampledir = $(docdir)/examples

example_DATA = TestDisk.cc TestDiskLog.cc TestProbing.cc TestLvm.cc TestMd.cc	\
	TestLoop.cc SaveGraph.cc ShowBytes.cc

EXTRA_DIST = $(example_DATA) Makefile.example

install-data-local:
	/usr/bin/install -d -p -m 755 $(DESTDIR)$(exampledir)
	/usr/bin/install -p -m 644 Makefile.example $(DESTDIR)$(exampledir)/Makefile
