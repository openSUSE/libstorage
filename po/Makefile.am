#
# Makefile.am for libstorage/po
#

XGETTEXT = xgettext
MSGFMT = msgfmt

SRCFILES = $(wildcard ../storage/*.cc)

POFILES = $(wildcard *.po)

MOFILES = $(POFILES:.po=.mo)

all: $(MOFILES)

libstorage.pot: $(SRCFILES)
	$(XGETTEXT) -s --no-wrap --add-comments --add-location --keyword=_ --keyword=_:1,2 \
	    --foreign-user --copyright-holder="SUSE Linux Products GmbH, Nuernberg" \
	    --default-domain=libstorage --output=libstorage.pot $(SRCFILES)

%.mo: %.po
	$(MSGFMT) --statistics -o $@ $<

install-data-local: $(MOFILES)
	files='$(MOFILES)'; \
	for file in $$files; do \
	    base=`basename $$file`; \
	    langdir=$(localedir)/`echo $$base | sed 's/\.mo$$//'`/LC_MESSAGES; \
	    $(mkinstalldirs) -d $(DESTDIR)$$langdir; \
	    echo " $(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$$langdir/$(PACKAGE).mo"; \
	    $(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$$langdir/$(PACKAGE).mo; \
	done

clean-local:
	rm -f libstorage.pot $(MOFILES)

EXTRA_DIST = $(POFILES)
