//
// Ruby SWIG interface definition for libstorage
//

%{
#include "storage/Utils/SystemCmd.h"
%}

%allowexception;

%exception {
  static VALUE storage_namespace = rb_define_module("Storage");
  static VALUE cmd_error = rb_define_class_under(storage_namespace, "SystemCmdException", rb_eRuntimeError);
  static VALUE not_found_error = rb_define_class_under(storage_namespace, "CommandNotFoundException", rb_eRuntimeError);
  try {
    $action
  } catch (storage::CommandNotFoundException e) {
    VALUE exception = rb_exc_new2(not_found_error, e.msg().c_str());
    rb_exc_raise(exception);
  } catch (storage::SystemCmdException e) {
    VALUE exception = rb_exc_new2(cmd_error, e.msg().c_str());
    rb_exc_raise(exception);
  }
}

%include "enum_ref.i"

OUTPUT_TYPEMAP(storage::MountByType, UINT2NUM, (unsigned int));

%include "../storage.i"

