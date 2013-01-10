#
# Makefile.am for libstorage/testsuite
#

SUBDIRS = lib config libstorage.test single.out data

PACKAGE = libstorage

AUTOMAKE_OPTIONS = dejagnu

AM_CPPFLAGS = -include $(top_builddir)/config.h -I$(top_srcdir)

LDADD = ../storage/libstorage.la

noinst_PROGRAMS = backupstate1.single contvolinfo1.single disklabel1.single	\
	format1.single freeinfo1.single fstab1.single fstab2.single		\
	fstab3.single fstab4.single humanstring1.single humanstring2.single	\
	lvm_create.single lvm_create2.single lvm_extend.single			\
	md_create.single partition1.single partitionname.single			\
	partitionprefix.single recursive1.single recursive2.single		\
	recursive3.single renumber1.single renumber2.single renumber3.single	\
	udevdecode.single udevencode.single usedby1.single uuid1.single

backupstate1_single_SOURCES = backupstate1.cc common.h common.cc
contvolinfo1_single_SOURCES = contvolinfo1.cc common.h common.cc
disklabel1_single_SOURCES = disklabel1.cc common.h common.cc
format1_single_SOURCES = format1.cc common.h common.cc
freeinfo1_single_SOURCES = freeinfo1.cc common.h common.cc
fstab1_single_SOURCES = fstab1.cc common.h common.cc
fstab2_single_SOURCES = fstab2.cc common.h common.cc
fstab3_single_SOURCES = fstab3.cc common.h common.cc
fstab4_single_SOURCES = fstab4.cc common.h common.cc
humanstring1_single_SOURCES = humanstring1.cc
humanstring2_single_SOURCES = humanstring2.cc
lvm_create2_single_SOURCES = lvm_create2.cc common.h common.cc
lvm_create_single_SOURCES = lvm_create.cc common.h common.cc
lvm_extend_single_SOURCES = lvm_extend.cc common.h common.cc
md_create_single_SOURCES = md_create.cc common.h common.cc
partition1_single_SOURCES = partition1.cc common.h common.cc
partitionname_single_SOURCES = partitionname.cc common.cc
partitionprefix_single_SOURCES = partitionprefix.cc common.cc
recursive1_single_SOURCES = recursive1.cc common.h common.cc
recursive2_single_SOURCES = recursive2.cc common.h common.cc
recursive3_single_SOURCES = recursive3.cc common.h common.cc
renumber1_single_SOURCES = renumber1.cc common.h common.cc
renumber2_single_SOURCES = renumber2.cc common.h common.cc
renumber3_single_SOURCES = renumber3.cc common.h common.cc
udevdecode_single_SOURCES = udevdecode.cc
udevencode_single_SOURCES = udevencode.cc
usedby1_single_SOURCES = usedby1.cc common.h common.cc
uuid1_single_SOURCES = uuid1.cc common.h common.cc

clean-local:
	rm -f site.exp site.bak
