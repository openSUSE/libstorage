#
# Makefile.am for storage/libstorage/src
#

AM_CXXFLAGS = -DY2LOG=\"y2storage\"
INCLUDES = -I$(includedir) -I/usr/include/evms

lib_LTLIBRARIES = liby2storage.la

liby2storage_la_SOURCES =		     \
	Storage.cc	 Storage.h           \
	Container.cc     Container.h	     \
	  Disk.cc        Disk.h	     	     \
	Volume.cc        Volume.h            \
	  Partition.cc   Partition.h         \
	FilterIterator.h ListListIterator.h  \
	IterPair.h

liby2storage_la_LDFLAGS = -version-info 2:0
liby2storage_la_LIBADD = -ly2util

pkgincludedir = $(includedir)/y2storage

# these go to $(pkgincludedir)

pkginclude_HEADERS =                                                    \
        Storage.h Container.h Volume.h Disk.h Partition.h \
	FilterIterator.h ListListIterator.h \
	IterPair.h

#<INSTALL-HEADER-TARGET>


