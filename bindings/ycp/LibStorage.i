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

specialize_sequence(storage::PartitionInfo, TO_PACK, FROM_PACK, CHECK)
specialize_sequence(storage::ContainerInfo, TO_PACK, FROM_PACK, CHECK)

%include "../../src/StorageInterface.h"

