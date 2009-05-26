
export LD_LIBRARY_PATH=../..

all: LibStorage.pyc LibStorage.pyo

LibStorage.pyc: LibStorage_wrap.cxx
	python -c "import LibStorage"

LibStorage.pyo: LibStorage_wrap.cxx
	python -O -c "import LibStorage"

LibStorage_wrap.cxx: LibStorage.i			\
	../../src/StorageInterface.h			\
	../../src/HumanString.h				\
	../../src/Graph.h
	swig -Wall -c++ -python LibStorage.i
	g++ -c -I/usr/include/python -O2 -fPIC -pipe LibStorage_wrap.cxx
	g++ -shared LibStorage_wrap.o -o _LibStorage.so -lstorage

