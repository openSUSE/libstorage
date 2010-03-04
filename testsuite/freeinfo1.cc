
#include <iostream>

#include "common.h"


using namespace storage;
using namespace std;


StorageInterface* s = NULL;


void
test(const string& device)
{
    ResizeInfo resize_info;
    ContentInfo content_info;

    if (s->getFreeInfo(device, true, resize_info, true, content_info, true))
    {
	cout << device << " true" << endl;
	cout << "    " << resize_info.resize_freeK << " " << resize_info.df_freeK << " "
	     << resize_info.usedK << " " << resize_info.resize_ok << endl;
	cout << "    " << content_info.windows << " " << content_info.efi << " "
	     << content_info.homes << endl;
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

    setup_logger();

    setup_system("thalassa");

    s = createStorageInterface(TestEnvironment());

    test("/dev/system/arvin");
    test("/dev/system/root");

    delete s;
}
