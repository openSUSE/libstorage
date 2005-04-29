//-------------------------------------
// Interface definition for libstorage
//-------------------------------------

%module Storage
%{
#include "../../src/StorageInterface.h"
%}

using namespace std;

%include "std_string.i"
%include "std_deque.i"

%typemap(out) std::string* {
    $result = PyString_FromString ($1->c_str ());
}

%include "../../src/StorageInterface.h"

using namespace storage;

%template(dequestring) deque<string>;
%template(dequepartitioninfo) deque<PartitionInfo>;

