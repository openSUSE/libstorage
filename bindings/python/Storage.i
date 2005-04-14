//-------------------------------------
// Interface definition for libstorage
//-------------------------------------

%module Storage
%{
#include "../../src/StorageInterface.h"
%}

using namespace std;

%include "std_string.i"
%include "std_deque.i"

%include "../../src/StorageInterface.h"

using namespace storage;

%template(dequestring) deque<string>;
