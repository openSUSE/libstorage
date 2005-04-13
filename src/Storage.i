//====================================
// Interface definition for libstorage
//------------------------------------
%module Storage
%{
#include "StorageInterface.h"
%}

%include "std_string.i"
%include "std_deque.i"

%include "StorageInterface.h"
