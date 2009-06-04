#
# Makefile.am for libstorage/doc/autodocs
#

htmldir = $(docdir)/autodocs

html_DATA = index.html $(wildcard *.html *.png)

index.html: $(srcdir)/../../storage/*.h
	doxygen doxygen.conf

CLEANFILES = $(html_DATA) doxygen.log installdox

EXTRA_DIST = doxygen.conf
