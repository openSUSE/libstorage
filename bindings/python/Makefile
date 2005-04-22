DESTDIR = /
PYTHON  = `readlink /usr/lib/python`
PACKDIR = ${DESTDIR}/usr/lib/${PYTHON}/site-packages
MODDIR  = ${PACKDIR}/Storage

export LD_LIBRARY_PATH=../../

all: Storage_wrap.cxx
	g++ -c -I/usr/include/python -O2 -fPIC -pipe Storage_wrap.cxx
	g++ -shared Storage_wrap.o -o _Storage.so -ly2storage
	python -c "import Storage"
	python -O -c "import Storage"

Storage_wrap.cxx: Storage.i ../../src/StorageInterface.h
	swig -Wall -c++ -python Storage.i

install_vendor:
	install -d -m 755      ${MODDIR}
	install -m 755 _Storage.so ${MODDIR}
	install -m 644 Storage.py  ${MODDIR}
	install -m 644 Storage.pyc ${MODDIR}
	install -m 644 Storage.pyo ${MODDIR}
	install -m 644 Storage.pth ${PACKDIR}
