#
# Makefile.am for libstorage/storage
#

SUBDIRS = SystemInfo

lib_LTLIBRARIES = libstorage.la

libstorage_la_SOURCES =					\
	StorageInterface.h				\
	Storage.cc		Storage.h		\
	Device.cc		Device.h		\
	Container.cc		Container.h		\
	  Disk.cc		Disk.h			\
	    Dasd.cc		Dasd.h			\
	  PeContainer.cc	PeContainer.h		\
	    LvmVg.cc		LvmVg.h			\
	    DmPartCo.cc		DmPartCo.h		\
	      DmraidCo.cc	DmraidCo.h		\
	      DmmultipathCo.cc	DmmultipathCo.h		\
	    DmCo.cc		DmCo.h			\
	  MdCo.cc		MdCo.h			\
	  MdPartCo.cc		MdPartCo.h		\
	  BtrfsCo.cc		BtrfsCo.h		\
	  TmpfsCo.cc		TmpfsCo.h		\
	  LoopCo.cc		LoopCo.h		\
	  NfsCo.cc		NfsCo.h			\
	Volume.cc		Volume.h		\
	  Partition.cc		Partition.h		\
	  Dm.cc			Dm.h			\
	    LvmLv.cc		LvmLv.h			\
	    DmPart.cc		DmPart.h		\
	      Dmraid.cc		Dmraid.h		\
	      Dmmultipath.cc	Dmmultipath.h		\
	  Md.cc			Md.h			\
	  MdPart.cc		MdPart.h		\
	  Btrfs.cc		Btrfs.h			\
	  Tmpfs.cc		Tmpfs.h			\
	  Loop.cc		Loop.h			\
	  Nfs.cc		Nfs.h			\
	HumanString.cc		HumanString.h		\
	Graph.cc		Graph.h			\
	Utils.cc		Utils.h			\
	SystemCmd.cc		SystemCmd.h		\
	OutputProcessor.cc	OutputProcessor.h	\
	Regex.cc		Regex.h			\
	FreeInfo.cc		FreeInfo.h		\
	ArchInfo.cc		ArchInfo.h		\
	AsciiFile.cc		AsciiFile.h		\
	XmlFile.cc		XmlFile.h		\
	EtcFstab.cc		EtcFstab.h		\
	EtcMdadm.cc		EtcMdadm.h		\
	Geometry.cc		Geometry.h		\
	AppUtil.cc		AppUtil.h		\
	Enum.cc			Enum.h			\
	Lock.cc			Lock.h			\
	StorageTypes.cc		StorageTypes.h		\
	FilterIterator.h	ListListIterator.h	\
	DerefIterator.h		IterPair.h		\
	Region.cc		Region.h		\
	StorageTmpl.h		StorageDefines.h        \
	gen_md5sum.cc           gen_md5sum.h

libstorage_la_LDFLAGS = -version-info @LIBVERSION_INFO@
libstorage_la_LIBADD = SystemInfo/libsysteminfo.la -lxml2

$(srcdir)/gen_pathlist.cc: StorageDefines.h
	grep BIN $< | sed -e "/PARTEDCMD/d" -e "s/^[^ ]*[ ]\+//" -e "s/ .*/,/" >$@

$(srcdir)/gen_md5sum.cc: $(filter-out gen_md5sum.cc,$(libstorage_la_SOURCES))
	./create_md5sum_date_file $(filter-out gen_md5sum.cc,$(libstorage_la_SOURCES))

BUILT_SOURCES = gen_pathlist.cc

noinst_SCRIPTS = create_md5sum_date_file generate_md5_sources

EXTRA_DIST = $(noinst_SCRIPTS)

pkgincludedir = $(includedir)/storage

pkginclude_HEADERS =			\
	StorageVersion.h		\
	StorageInterface.h		\
	StorageSwig.h		        \
	HumanString.h			\
	Graph.h				\
	Utils.h
