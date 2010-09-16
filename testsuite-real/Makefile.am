#
# Makefile.am for libstorage/testsuite-real
#

INCLUDES = -I$(top_srcdir)

LDADD = ../storage/libstorage.la

testsuite_realdir = /usr/lib/libstorage/testsuite-real

testsuite_real_PROGRAMS = align1 align2 format1 resize1

align1_SOURCES = align1.cc common.h common.cc
align2_SOURCES = align2.cc common.h common.cc
format1_SOURCES = format1.cc common.h common.cc
resize1_SOURCES = resize1.cc common.h common.cc

noinst_PROGRAMS = lvmcreate1 lvmcreate2 lvmextend1 lvmextend2

lvmcreate1_SOURCES = lvmcreate1.cc

lvmcreate2_SOURCES = lvmcreate2.cc

lvmextend1_SOURCES = lvmextend1.cc

lvmextend2_SOURCES = lvmextend2.cc

