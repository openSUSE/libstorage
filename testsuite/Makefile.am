#
# Makefile.am for libstorage/testsuite
#

SUBDIRS = lib config libstorage.test single.out data

PACKAGE = libstorage

AUTOMAKE_OPTIONS = dejagnu

INCLUDES = -I$(top_srcdir)

LDADD = ../storage/libstorage.la

noinst_PROGRAMS = partition1.single format1.single uuid1.single fstab1.single	\
	lvm_create.single lvm_extend.single md_create.single			\
	humanstring1.single humanstring2.single partitionname.single		\
	partitionprefix.single udevencode.single udevdecode.single		\
	backupstate1.single contvolinfo1.single lvm_create2.single		\
	usedby1.single recursive1.single

partition1_single_SOURCES = partition1.cc common.h common.cc

format1_single_SOURCES = format1.cc common.h common.cc

uuid1_single_SOURCES = uuid1.cc common.h common.cc

fstab1_single_SOURCES = fstab1.cc common.h common.cc

lvm_create_single_SOURCES = lvm_create.cc common.h common.cc

lvm_extend_single_SOURCES = lvm_extend.cc common.h common.cc

md_create_single_SOURCES = md_create.cc common.h common.cc

humanstring1_single_SOURCES = humanstring1.cc

humanstring2_single_SOURCES = humanstring2.cc

partitionname_single_SOURCES = partitionname.cc

partitionprefix_single_SOURCES = partitionprefix.cc

udevencode_single_SOURCES = udevencode.cc

udevdecode_single_SOURCES = udevdecode.cc

backupstate1_single_SOURCES = backupstate1.cc common.h common.cc

contvolinfo1_single_SOURCES = contvolinfo1.cc common.h common.cc

lvm_create2_single_SOURCES = lvm_create2.cc common.h common.cc

usedby1_single_SOURCES = usedby1.cc common.h common.cc

recursive1_single_SOURCES = recursive1.cc common.h common.cc

clean-local:
	rm -f tmp.err.* tmp.out.* site.exp site.bak
