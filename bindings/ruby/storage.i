//
// Ruby interface definition for libstorage
//

%module storage

%{
#include <storage/StorageInterface.h>
#include <storage/HumanString.h>
#include <storage/Graph.h>
%}

using namespace std;

%include "std_string.i"
%include "std_deque.i"
%include "std_list.i"

%include "../../storage/StorageInterface.h"
%include "../../storage/HumanString.h"
%include "../../storage/Graph.h"

using namespace storage;

%template(Dequestring) deque<string>;
%template(Dequecontainerinfo) deque<ContainerInfo>;
%template(Dequepartitioninfo) deque<PartitionInfo>;
%template(Dequemdinfo) deque<MdInfo>;
%template(Dequelvmlvinfo) deque<LvmLvInfo>;

