#
# Makefile.am for libstorage/testsuite-real
#

INCLUDES = -I$(top_srcdir)

LDADD = ../storage/libstorage.la

testsuite_realdir = /usr/lib/libstorage/testsuite-real

testsuite_real_PROGRAMS = format1

format1_SOURCES = format1.cc common.h common.cc

noinst_PROGRAMS = lvmcreate1 lvmcreate2 lvmextend1 lvmextend2 create1 create2

lvmcreate1_SOURCES = lvmcreate1.cc

lvmcreate2_SOURCES = lvmcreate2.cc

lvmextend1_SOURCES = lvmextend1.cc

lvmextend2_SOURCES = lvmextend2.cc

create1_SOURCES = create1.cc

create2_SOURCES = create2.cc
