#
# Makefile.am for libstorage/testsuite
#

SUBDIRS = lib config libstorage.test single.out data
PACKAGE = libstorage
AUTOMAKE_OPTIONS = dejagnu

INCLUDES = -I$(top_srcdir)/libstorage/src

AM_CPPFLAGS =
AM_CXXFLAGS =

noinst_PROGRAMS = partition1.single format1.single uuid1.single

partition1_single_SOURCES = partition1.cc
partition1_single_LDADD = ../src/liby2storage.la

format1_single_SOURCES = format1.cc
format1_single_LDADD = ../src/liby2storage.la

uuid1_single_SOURCES = uuid1.cc common.cc common.h
uuid1_single_LDADD = ../src/liby2storage.la

clean-local:
	rm -f tmp.err.* tmp.out.* site.exp site.bak

