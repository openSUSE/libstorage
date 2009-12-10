
#include <iostream>

#include "common.h"


using namespace storage;
using namespace std;


StorageInterface* s = NULL;


void
test(const string& device)
{
    unsigned long long resize_freeK;
    unsigned long long df_freeK;
    unsigned long long usedK;
    bool windows;
    bool efi;
    bool home;

    if (s->getFreeInfo(device, resize_freeK, df_freeK, usedK, windows, efi, home, true))
    {
	cout << device << " true" << endl;
	cout << "    " << resize_freeK << " " << df_freeK << " " << usedK << endl;
	cout << "    " << windows << " " << efi << " " << home << endl;
    }
    else
    {
	cout << device << " false" << endl;
    }
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_system("thalassa");

    s = createStorageInterface(TestEnvironment());

    test("/dev/system/arvin");
    test("/dev/system/root");

    delete s;
}
