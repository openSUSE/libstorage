#
# Makefile.am for libstorage/testsuite
#

SUBDIRS = lib config libstorage.test single.out data

PACKAGE = libstorage

AUTOMAKE_OPTIONS = dejagnu

INCLUDES = -I$(top_srcdir)

LDADD = ../storage/libstorage.la

noinst_PROGRAMS = alt-names1.single alt-names2.single				\
	backupstate1.single contvolinfo1.single disklabel1.single		\
	format1.single freeinfo1.single fstab1.single fstab2.single		\
	fstab3.single fstab4.single fstab-add.single fstab-update.single	\
	fstab-remove.single fstab-ng-add.single fstab-ng-update.single		\
	fstab-ng-remove.single fstab-ng-find.single crypttab-add.single		\
	crypttab-remove.single							\
	humanstring1.single humanstring2.single					\
	lvm_create.single lvm_create2.single lvm_extend.single			\
	md_create.single partition1.single partitionname.single			\
	partition-slots1.single							\
	partitionprefix.single recursive1.single recursive2.single		\
	recursive3.single renumber1.single renumber2.single renumber3.single	\
	udevdecode.single udevencode.single usedby1.single userdata1.single	\
	uuid1.single								\
	blkid.single btrfs.single cryptsetup.single dasdview.single		\
	dmraid.single dmsetup-info.single lsscsi.single mdadm-detail.single	\
	mdadm-examine.single parted.single proc-mdstat.single			\
	proc-mounts.single proc-parts.single vgdisplay.single vgs.single

alt_names1_single_SOURCES = alt-names1.cc common.h common.cc
alt_names2_single_SOURCES = alt-names2.cc common.h common.cc
backupstate1_single_SOURCES = backupstate1.cc common.h common.cc
contvolinfo1_single_SOURCES = contvolinfo1.cc common.h common.cc
disklabel1_single_SOURCES = disklabel1.cc common.h common.cc
format1_single_SOURCES = format1.cc common.h common.cc
freeinfo1_single_SOURCES = freeinfo1.cc common.h common.cc
fstab1_single_SOURCES = fstab1.cc common.h common.cc
fstab2_single_SOURCES = fstab2.cc common.h common.cc
fstab3_single_SOURCES = fstab3.cc common.h common.cc
fstab4_single_SOURCES = fstab4.cc common.h common.cc
fstab_add_single_SOURCES = fstab-add.cc common.h common.cc
fstab_update_single_SOURCES = fstab-update.cc common.h common.cc
fstab_remove_single_SOURCES = fstab-remove.cc common.h common.cc
fstab_ng_add_single_SOURCES = fstab-ng-add.cc common.h common.cc
fstab_ng_update_single_SOURCES = fstab-ng-update.cc common.h common.cc
fstab_ng_remove_single_SOURCES = fstab-ng-remove.cc common.h common.cc
fstab_ng_find_single_SOURCES = fstab-ng-find.cc common.h common.cc
crypttab_add_single_SOURCES = crypttab-add.cc common.h common.cc
crypttab_remove_single_SOURCES = crypttab-remove.cc common.h common.cc
humanstring1_single_SOURCES = humanstring1.cc
humanstring2_single_SOURCES = humanstring2.cc
lvm_create_single_SOURCES = lvm_create.cc common.h common.cc
lvm_create2_single_SOURCES = lvm_create2.cc common.h common.cc
lvm_extend_single_SOURCES = lvm_extend.cc common.h common.cc
md_create_single_SOURCES = md_create.cc common.h common.cc
partition1_single_SOURCES = partition1.cc common.h common.cc
partitionname_single_SOURCES = partitionname.cc common.cc
partitionprefix_single_SOURCES = partitionprefix.cc common.cc
partition_slots1_single_SOURCES = partition-slots1.cc common.cc
recursive1_single_SOURCES = recursive1.cc common.h common.cc
recursive2_single_SOURCES = recursive2.cc common.h common.cc
recursive3_single_SOURCES = recursive3.cc common.h common.cc
renumber1_single_SOURCES = renumber1.cc common.h common.cc
renumber2_single_SOURCES = renumber2.cc common.h common.cc
renumber3_single_SOURCES = renumber3.cc common.h common.cc
udevdecode_single_SOURCES = udevdecode.cc
udevencode_single_SOURCES = udevencode.cc
usedby1_single_SOURCES = usedby1.cc common.h common.cc
userdata1_single_SOURCES = userdata1.cc common.h common.cc
uuid1_single_SOURCES = uuid1.cc common.h common.cc

blkid_single_SOURCES = blkid.cc common.h common.cc
btrfs_single_SOURCES = btrfs.cc common.h common.cc
cryptsetup_single_SOURCES = cryptsetup.cc common.h common.cc
dasdview_single_SOURCES = dasdview.cc common.h common.cc
dmraid_single_SOURCES = dmraid.cc common.h common.cc
dmsetup_info_single_SOURCES = dmsetup-info.cc common.h common.cc
lsscsi_single_SOURCES = lsscsi.cc common.h common.cc
mdadm_detail_single_SOURCES = mdadm-detail.cc common.h common.cc
mdadm_examine_single_SOURCES = mdadm-examine.cc common.h common.cc
parted_single_SOURCES = parted.cc common.h common.cc
proc_mdstat_single_SOURCES = proc-mdstat.cc common.h common.cc
proc_mounts_single_SOURCES = proc-mounts.cc common.h common.cc
proc_parts_single_SOURCES = proc-parts.cc common.h common.cc
vgdisplay_single_SOURCES = vgdisplay.cc common.h common.cc
vgs_single_SOURCES = vgs.cc common.h common.cc

clean-local:
	rm -f site.exp site.bak
