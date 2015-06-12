//
// Python SWIG interface definition for libstorage
//

%include "enum_ref.i"

%typemap(out) std::string* {
    $result = PyString_FromString($1->c_str());
}

%RefOutputEnum(storage::MountByType, OUTPUT);

%include "../storage.i"

