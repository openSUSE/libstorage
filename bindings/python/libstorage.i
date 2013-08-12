//
// Python interface definition for libstorage
//

%module libstorage

%{
#include <storage/StorageInterface.h>
#include <storage/HumanString.h>
#include <storage/Graph.h>
%}

using namespace std;

%include "std_string.i"
%include "std_deque.i"
%include "std_list.i"
%include "enum_ref.i"

%typemap(out) std::string* {
    $result = PyString_FromString($1->c_str());
}

%RefOutputEnum( storage::MountByType, OUTPUT );

%include "../../storage/StorageSwig.h"
%include "../../storage/StorageInterface.h"
%include "../../storage/HumanString.h"
%include "../../storage/Graph.h"

using namespace storage;

%template(DequeString) deque<string>;
%template(ListString) list<string>;
%template(DequeContainerInfo) deque<ContainerInfo>;
%template(DequePartitionInfo) deque<PartitionInfo>;
%template(DequeMdPartInfo) deque<MdPartInfo>;
%template(DequeDmraidInfo) deque<DmraidInfo>;
%template(DequeLvmLvInfo) deque<LvmLvInfo>;
%template(DequeMdInfo) deque<MdInfo>;
%template(DequeLoopInfo) deque<LoopInfo>;
%template(DequeDmInfo) deque<DmInfo>;
%template(DequeNfsInfo) deque<NfsInfo>;
%template(DequeTmpfsInfo) deque<TmpfsInfo>;
%template(DequeBtrfsInfo) deque<BtrfsInfo>;
%template(ListPartitionSlotInfo) list<PartitionSlotInfo>;
%template(ListCommitInfo) list<CommitInfo>;
