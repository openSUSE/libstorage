//
// Ruby SWIG interface definition for libstorage
//

%include "enum_ref.i"

OUTPUT_TYPEMAP(storage::MountByType, UINT2NUM, (unsigned int));

%include "../storage.i"

