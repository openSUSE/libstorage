#
# Makefile.am for libstorage/testsuite
#

SUBDIRS = lib config libstorage.test single.out data
PACKAGE = libstorage
AUTOMAKE_OPTIONS = dejagnu

INCLUDES = -I$(top_srcdir)/libstorage/src

AM_CPPFLAGS =
AM_CXXFLAGS =

noinst_PROGRAMS = partition1.single format1.single uuid1.single fstab1.single lvm_create.single lvm_extend.single md_create.single evms_create.single evms_extend.single

partition1_single_SOURCES = partition1.cc
partition1_single_LDADD = ../src/liby2storage.la

format1_single_SOURCES = format1.cc
format1_single_LDADD = ../src/liby2storage.la

uuid1_single_SOURCES = uuid1.cc common.cc common.h
uuid1_single_LDADD = ../src/liby2storage.la

fstab1_single_SOURCES = fstab1.cc
fstab1_single_LDADD = ../src/liby2storage.la

lvm_create_single_SOURCES = lvm_create.cc
lvm_create_single_LDADD = ../src/liby2storage.la

lvm_extend_single_SOURCES = lvm_extend.cc
lvm_extend_single_LDADD = ../src/liby2storage.la

md_create_single_SOURCES = md_create.cc
md_create_single_LDADD = ../src/liby2storage.la

evms_create_single_SOURCES = evms_create.cc
evms_create_single_LDADD = ../src/liby2storage.la

evms_extend_single_SOURCES = evms_extend.cc
evms_extend_single_LDADD = ../src/liby2storage.la

clean-local:
	rm -f tmp.err.* tmp.out.* site.exp site.bak

