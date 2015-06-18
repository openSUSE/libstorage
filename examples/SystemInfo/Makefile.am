#
# Makefile.am for libstorage/examples/SystemInfo
#

AM_CPPFLAGS = -I$(top_srcdir)

LDADD = ../../storage/libstorage.la

noinst_PROGRAMS = test-blkid test-cmdbtrfsshow test-cmdbtrfssubvolumes		\
	test-cmddmsetupinfo test-cmdvgdisplay test-cmdvgs test-dir		\
	test-lsattr test-lsscsi test-majorminor					\
	test-mdadmdetail test-mdadmexamine test-mdlinks test-parted		\
	test-procmdstat test-procmounts test-procparts test-udevmap

test_blkid_SOURCES = test-blkid.cc
test_cmdbtrfsshow_SOURCES = test-cmdbtrfsshow.cc
test_cmdbtrfssubvolumes_SOURCES = test-cmdbtrfssubvolumes.cc
test_cmddmsetupinfo_SOURCES = test-cmddmsetupinfo.cc
test_cmdvgdisplay_SOURCES = test-cmdvgdisplay.cc
test_cmdvgs_SOURCES = test-cmdvgs.cc
test_dir_SOURCES = test-dir.cc
test_lsattr_SOURCES = test-lsattr.cc
test_lsscsi_SOURCES = test-lsscsi.cc
test_majorminor_SOURCES = test-majorminor.cc
test_mdadmdetail_SOURCES = test-mdadmdetail.cc
test_mdadmexamine_SOURCES = test-mdadmexamine.cc
test_mdlinks_SOURCES = test-mdlinks.cc
test_parted_SOURCES = test-parted.cc
test_procmdstat_SOURCES = test-procmdstat.cc
test_procmounts_SOURCES = test-procmounts.cc
test_procparts_SOURCES = test-procparts.cc
test_udevmap_SOURCES = test-udevmap.cc

