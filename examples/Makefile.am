#
# Makefile.am for libstorage/testsuite
#

INCLUDES = -I$(top_srcdir)/libstorage/src -I/usr/include/YaST2
LDADD = ../src/liby2storage.la

AM_CPPFLAGS = -DY2LOG=\"y2storage\"
AM_CXXFLAGS =

noinst_PROGRAMS = TestDisk TestDiskLog TestStorage

TestDisk_SOURCES = TestDisk.cc

TestDiskLog_SOURCES = TestDiskLog.cc

TestStorage_SOURCES = TestStorage.cc

