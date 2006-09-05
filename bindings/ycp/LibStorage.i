//
// YCP interface definition for libstorage
//

%module LibStorage

%include "LiMaL.i"

%{
#include "../../src/StorageInterface.h"
%}

using namespace std;

%include "std_string.i"
%include "std_deque.i"

specialize_sequence(storage::ContainerInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::DiskInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::LvmVgInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::EvmsCoInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::VolumeInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::PartitionInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::PartitionAddInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::LvmLvInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::EvmsInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::MdInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::LoopInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::DmInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::DmPartCoInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::DmraidCoInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::DmPartInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::DmraidInfo, TO_PACK, FROM_PACK, CHECK)

%include "../../src/StorageInterface.h"

