
export LD_LIBRARY_PATH=../..

all: Storage.pyc Storage.pyo

Storage.pyc: Storage_wrap.cxx
	python -c "import Storage"

Storage.pyo: Storage_wrap.cxx
	python -O -c "import Storage"

Storage_wrap.cxx: Storage.i ../../src/StorageInterface.h
	swig -Wall -c++ -python Storage.i
	g++ -c -I/usr/include/python -O2 -fPIC -pipe Storage_wrap.cxx
	g++ -shared Storage_wrap.o -o _Storage.so -ly2storage

