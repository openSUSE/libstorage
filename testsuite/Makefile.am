#
# Makefile.am for libstorage/testsuite
#

SUBDIRS = lib config libstorage.test single.out data
PACKAGE = libstorage
AUTOMAKE_OPTIONS = dejagnu

INCLUDES = -I$(top_srcdir)/libstorage/src

LDADD = ../src/liby2storage.la

noinst_PROGRAMS = partition1.single format1.single uuid1.single fstab1.single	\
	lvm_create.single lvm_extend.single md_create.single			\
	humanstring1.single humanstring2.single

partition1_single_SOURCES = partition1.cc common.cc common.h

format1_single_SOURCES = format1.cc common.cc common.h

uuid1_single_SOURCES = uuid1.cc common.cc common.h

fstab1_single_SOURCES = fstab1.cc common.cc common.h

lvm_create_single_SOURCES = lvm_create.cc common.cc common.h

lvm_extend_single_SOURCES = lvm_extend.cc common.cc common.h

md_create_single_SOURCES = md_create.cc common.cc common.h

humanstring1_single_SOURCES = humanstring1.cc

humanstring2_single_SOURCES = humanstring2.cc

clean-local:
	rm -f tmp.err.* tmp.out.* site.exp site.bak
