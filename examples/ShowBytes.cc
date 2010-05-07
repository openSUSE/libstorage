
#include <iostream>

#include <storage/HumanString.h>

using namespace storage;
using namespace std;

int
main(int argc, char** argv)
{
    if (argc != 2)
    {
	cerr << "no value provided" << endl;
	return 1;
    }

    unsigned long long bytes = 0;
    if (!humanStringToByte(argv[1], true, bytes))
    {
	cerr << "parsing value failed" << endl;
	return 1;
    }

    cout << bytes << " B" << endl;

    cout << byteToHumanString(bytes, true, 2, true) << endl;

    return 0;
}
