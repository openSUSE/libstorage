#
# Makefile.am for libstorage/testsuite-real
#

INCLUDES = -I$(top_srcdir)/libstorage/src

AM_CPPFLAGS =
AM_CXXFLAGS =

noinst_PROGRAMS = lvmcreate1 lvmcreate2 lvmextend1 lvmextend2

lvmcreate1_SOURCES = lvmcreate1.cc
lvmcreate1_LDADD = ../src/liby2storage.la

lvmcreate2_SOURCES = lvmcreate2.cc
lvmcreate2_LDADD = ../src/liby2storage.la

lvmextend1_SOURCES = lvmextend1.cc
lvmextend1_LDADD = ../src/liby2storage.la

lvmextend2_SOURCES = lvmextend2.cc
lvmextend2_LDADD = ../src/liby2storage.la
