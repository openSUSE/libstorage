
#include <storage/StorageInterface.h>
#include <storage/StorageTmpl.h>


namespace storage
{

    void check_failure(const char* str, const char* file, int line, const char* func);

#define check(expr) ((expr) ? (void)(0) : check_failure(#expr, __FILE__, __LINE__, \
							__PRETTY_FUNCTION__))


    void print_commitinfos(const StorageInterface* s);

}
