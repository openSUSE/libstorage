//
// Python interface definition for libstorage
//

%module LibStorage

%{
#include "../../src/StorageInterface.h"
#include "../../src/HumanString.h"
#include "../../src/Graph.h"
%}

using namespace std;

%include "std_string.i"
%include "std_deque.i"
%include "std_list.i"

%typemap(out) std::string* {
    $result = PyString_FromString($1->c_str());
}

%include "../../src/StorageInterface.h"
%include "../../src/HumanString.h"
%include "../../src/Graph.h"

using namespace storage;

%template(dequestring) deque<string>;
%template(dequecontainerinfo) deque<ContainerInfo>;
%template(dequepartitioninfo) deque<PartitionInfo>;
%template(dequelvmlvinfo) deque<LvmLvInfo>;

