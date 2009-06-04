//
// Python interface definition for libstorage
//

%module LibStorage

%{
#include "../../storage/StorageInterface.h"
#include "../../storage/HumanString.h"
#include "../../storage/Graph.h"
%}

using namespace std;

%include "std_string.i"
%include "std_deque.i"
%include "std_list.i"

%typemap(out) std::string* {
    $result = PyString_FromString($1->c_str());
}

%include "../../storage/StorageInterface.h"
%include "../../storage/HumanString.h"
%include "../../storage/Graph.h"

using namespace storage;

%template(dequestring) deque<string>;
%template(dequecontainerinfo) deque<ContainerInfo>;
%template(dequepartitioninfo) deque<PartitionInfo>;
%template(dequemdinfo) deque<MdInfo>;
%template(dequelvmlvinfo) deque<LvmLvInfo>;

