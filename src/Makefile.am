#
# Makefile.am for libstorage/src
#

AM_CXXFLAGS = -DY2LOG=\"y2storage\"
INCLUDES = -I$(includedir) -I/usr/include/evms

y2storage:
	ln -sf . y2storage

Makefile: y2storage

lib_LTLIBRARIES = liby2storage.la

liby2storage_la_SOURCES =			\
	StorageInterface.h			\
	Storage.cc	 Storage.h           \
	Container.cc     Container.h	     \
	  Disk.cc        Disk.h	     	     \
	  LvmVg.cc       LvmVg.h	     \
	  Evms.cc        Evms.h	     	     \
	Volume.cc        Volume.h            \
	  Partition.cc   Partition.h         \
	  LvmLv.cc       LvmLv.h             \
	  EvmsVol.cc     EvmsVol.h           \
	  Md.cc          Md.h                \
	  Loop.cc        Loop.h              \
	SystemCmd.cc     SystemCmd.h         \
	OutputProcessor.cc OutputProcessor.h \
	Regex.cc         Regex.h             \
	AsciiFile.cc     AsciiFile.h         \
	  ProcPart.cc    ProcPart.h 	     \
	ProcMounts.cc    ProcMounts.h        \
	EtcFstab.cc      EtcFstab.h 	     \
	AppUtil.cc       AppUtil.h           \
	FilterIterator.h ListListIterator.h  \
	DerefIterator.h  IterPair.h          \
	Region.h

liby2storage_la_LDFLAGS = -version-info 2:0
liby2storage_la_LIBADD = -ly2util

pkgincludedir = $(includedir)/y2storage

# these go to $(pkgincludedir)

pkginclude_HEADERS =							\
	StorageInterface.h Storage.h Container.h Volume.h Disk.h	\
	Partition.h Md.h Loop.h LvmVg.h LvmLv.h \
	Evms.h EvmsVol.h \
	FilterIterator.h ListListIterator.h DerefIterator.h \
	IterPair.h StorageTmpl.h Region.h

#<INSTALL-HEADER-TARGET>


