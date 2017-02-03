//
// Ruby SWIG interface definition for libstorage
//

%{
#include "storage/Utils/SystemCmd.h"

VALUE ruby_cmd_error_class(VALUE namesp){
  VALUE klass = rb_define_class_under(namesp, "SystemCmdException", rb_eRuntimeError);
  rb_define_attr(klass, "cmd", 1, 1);
  rb_define_attr(klass, "ret", 1, 1);
  rb_define_attr(klass, "stderr", 1, 1);
  return klass;
}

VALUE ruby_not_found_class(VALUE namesp, VALUE parent){
  return rb_define_class_under(namesp, "CommandNotFoundException", parent);
}

VALUE raise_system_cmd_exception(VALUE cmd_error, const storage::SystemCmdException& e)
{
  VALUE exception = rb_exc_new2(cmd_error, e.msg().c_str());
  rb_funcall(exception, rb_intern("cmd="), 1, rb_str_new2(e.cmd().c_str()));
  rb_funcall(exception, rb_intern("ret="), 1, INT2FIX(e.cmdRet()));
  VALUE stderr = rb_ary_new2(e.stderr().size());
  for (auto line: e.stderr()){
    rb_ary_push(stderr, rb_str_new2(line.c_str()));
  }
  rb_funcall(exception, rb_intern("stderr="), 1, stderr);
  rb_exc_raise(exception);
  return Qnil;
}

%}



%allowexception;

%exception {
  VALUE storage_namespace = rb_define_module("Storage");
  VALUE cmd_error = ruby_cmd_error_class(storage_namespace);
  VALUE not_found_error = ruby_not_found_class(storage_namespace, cmd_error);
  try {
    $action
  } catch (storage::CommandNotFoundException e) {
    raise_system_cmd_exception(not_found_error, e);
  } catch (storage::SystemCmdException e) {
    raise_system_cmd_exception(cmd_error, e);
  }
}

%include "enum_ref.i"

OUTPUT_TYPEMAP(storage::MountByType, UINT2NUM, (unsigned int));

%include "../storage.i"

