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
%include "enum_ref.i"

OUTPUT_TYPEMAP(storage::MountByType, UINT2NUM, (unsigned int));

%include "../../storage/StorageSwig.h"
%include "../../storage/StorageInterface.h"
%include "../../storage/HumanString.h"
%include "../../storage/Graph.h"

using namespace storage;

%template(DequeString) deque<string>;
%template(ListString) list<string>;
%template(ListInt) list<int>;
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

