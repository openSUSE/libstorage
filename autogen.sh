#!/bin/sh -e

aclocal
libtoolize --force --automake --copy
autoheader
automake --add-missing --copy
autoconf
