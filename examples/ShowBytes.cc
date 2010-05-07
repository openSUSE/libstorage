
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

    locale::global(locale(""));

    unsigned long long bytes = 0;
    if (!humanStringToByte(argv[1], false, bytes))
    {
	cerr << "parsing value failed" << endl;
	return 1;
    }

    cout << bytes << " " << getSuffix(0, false) << endl;

    cout << byteToHumanString(bytes, false, 2, true) << endl;

    return 0;
}
