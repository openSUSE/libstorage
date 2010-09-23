#
# Makefile.am for libstorage/testsuite-real
#

INCLUDES = -I$(top_srcdir)

LDADD = ../storage/libstorage.la

testsuite_realdir = /usr/lib/libstorage/testsuite-real

testsuite_real_PROGRAMS = align1 align2 format1 resize1 lvmcreate1 lvmresize1 format2 resize2

align1_SOURCES = align1.cc common.h common.cc
align2_SOURCES = align2.cc common.h common.cc
format1_SOURCES = format1.cc common.h common.cc
resize1_SOURCES = resize1.cc common.h common.cc

lvmcreate1_SOURCES = lvmcreate1.cc common.h common.cc
lvmresize1_SOURCES = lvmresize1.cc common.h common.cc
format2_SOURCES = format2.cc common.h common.cc
resize2_SOURCES = resize2.cc common.h common.cc

