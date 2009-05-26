#
# Makefile.am for libstorage/src
#

AM_CXXFLAGS =
INCLUDES = -I$(includedir)

storage:
	ln -sf . storage
	mkdir .libs
	ln -sf . .libs/plugin

BUILT_SOURCES = storage StorageCallbacksBuiltinTable.h StorageCallbacksBuiltinCalls.h

StorageCallbacksBuiltinTable.h : StorageCallbacks.h
	y2tool generateYCPWrappers StorageCallbacks.h StorageCallbacksBuiltinCalls.h StorageCallbacksBuiltinTable.h

StorageCallbacksBuiltinCalls.h : StorageCallbacks.h
	y2tool generateYCPWrappers StorageCallbacks.h StorageCallbacksBuiltinCalls.h StorageCallbacksBuiltinTable.h


lib_LTLIBRARIES = liby2storage.la

liby2storage_la_SOURCES =				\
	StorageInterface.h				\
	Storage.cc		Storage.h		\
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
	  Loop.cc		Loop.h			\
	  Nfs.cc		Nfs.h			\
	HumanString.cc		HumanString.h		\
	Graph.cc		Graph.h			\
	SystemCmd.cc		SystemCmd.h		\
	OutputProcessor.cc	OutputProcessor.h	\
	Regex.cc		Regex.h			\
	AsciiFile.cc		AsciiFile.h		\
	ProcPart.cc		ProcPart.h		\
	EtcRaidtab.cc		EtcRaidtab.h		\
	ProcMounts.cc		ProcMounts.h		\
	EtcFstab.cc		EtcFstab.h		\
	AppUtil.cc		AppUtil.h		\
	Lock.cc			Lock.h			\
	StorageTypes.cc		StorageTypes.h		\
	FilterIterator.h	ListListIterator.h	\
	DerefIterator.h		IterPair.h		\
	Region.h		StorageTmpl.h		\
	StorageDefines.h

liby2storage_la_LDFLAGS = -version-info 2:0
liby2storage_la_LIBADD = -lblocxx

plugin_LTLIBRARIES = libpy2StorageCallbacks.la

libpy2StorageCallbacks_la_SOURCES =					\
	Y2StorageCallbacksComponent.cc Y2StorageCallbacksComponent.h	\
	Y2CCStorageCallbacks.cc Y2CCStorageCallbacks.h			\
	StorageCallbacks.cc StorageCallbacks.h

libpy2StorageCallbacks_la_LDFLAGS = -version-info 2:0
libpy2StorageCallbacks_la_LIBADD = -L$(libdir) -ly2 -lycp liby2storage.la

pkgincludedir = $(includedir)/storage

pkginclude_HEADERS =			\
	StorageInterface.h		\
	HumanString.h			\
	Graph.h

CLEANFILES = $(BUILT_SOURCES)
