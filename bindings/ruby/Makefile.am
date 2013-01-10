#
# Makefile.am for libstorage/bindings/ruby
#

SUBDIRS = examples

EXTRA_DIST = storage.i enum_ref.i

MAINTAINERCLEANFILES = storage_wrap.cxx

BUILT_SOURCES = storage_wrap.cxx

rubyextensiondirdir = $(RUBY_EXTENSION_DIR)

AM_CXXFLAGS = -fPIC -DPIC

AM_CPPFLAGS = -include $(top_builddir)/config.h -I${top_srcdir} -I$(RUBY_INCLUDE_DIR) -I$(RUBY_INCLUDE_DIR)/$(RUBY_ARCH_NAME)

DEPENDENCIES =						\
	${top_srcdir}/storage/StorageInterface.h	\
	${top_srcdir}/storage/HumanString.h		\
	${top_srcdir}/storage/Graph.h                   \
	./enum_ref.i

# Build Ruby module as shared library
rubyextensiondir_LTLIBRARIES = storage.la
nodist_storage_la_SOURCES = storage_wrap.cxx
storage_la_LIBADD =  $(top_builddir)/storage/libstorage.la -l$(RUBY_SO_NAME)
storage_la_LDFLAGS = -no-undefined -module -shared -avoid-version -export-dynamic -L$(RUBY_EXTENSION_DIR)

storage_la_DEPENDENCIES =				\
	$(DEPENDENCIES)					\
	${top_builddir}/storage/libstorage.la

storage_wrap.cxx: storage.i $(DEPENDENCIES)
	swig -o storage_wrap.cxx -c++ -ruby ${INCLUDES} storage.i

CLEANFILES = storage.so storage_wrap.cxx
