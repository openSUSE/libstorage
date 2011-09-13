%define %RefOutputTypemapEnum(Type, Name)

%typemap(in,numinputs=0) Type& Name ($*1_ltype temp)  "$1 = &temp;"
%typemap(argout) Type& Name
    {
    PyObject* obj = SWIG_From(int)(*$1);
    $result = t_output_helper( $result, obj ); 
    }

%enddef


%define %RefOutputEnum( type, name )

%fragment("t_output_helper");
%RefOutputTypemapEnum( %arg(type), name );

%enddef
