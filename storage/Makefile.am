#
# Makefile.am for libstorage/storage
#

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
	  Loop.cc		Loop.h			\
	  Nfs.cc		Nfs.h			\
	HumanString.cc		HumanString.h		\
	Graph.cc		Graph.h			\
	SystemCmd.cc		SystemCmd.h		\
	OutputProcessor.cc	OutputProcessor.h	\
	Regex.cc		Regex.h			\
	FreeInfo.cc		FreeInfo.h		\
	AsciiFile.cc		AsciiFile.h		\
	XmlFile.cc		XmlFile.h		\
	SystemInfo.cc		SystemInfo.h		\
	ProcParts.cc		ProcParts.h		\
	Blkid.cc		Blkid.h			\
	EtcRaidtab.cc		EtcRaidtab.h		\
	ProcMounts.cc		ProcMounts.h		\
	EtcFstab.cc		EtcFstab.h		\
	AppUtil.cc		AppUtil.h		\
	Lock.cc			Lock.h			\
	StorageTypes.cc		StorageTypes.h		\
	FilterIterator.h	ListListIterator.h	\
	DerefIterator.h		IterPair.h		\
	Region.cc		Region.h		\
	StorageTmpl.h		StorageDefines.h

libstorage_la_LDFLAGS = -version-info 2:0
libstorage_la_LIBADD = -lblocxx -lxml2

pkgincludedir = $(includedir)/storage

pkginclude_HEADERS =			\
	StorageInterface.h		\
	HumanString.h			\
	Graph.h
