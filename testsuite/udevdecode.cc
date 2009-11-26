
#include <iostream>

#include <storage/AppUtil.h>


using namespace std;
using namespace storage;


int
main()
{
    cout << udevDecode("nothing") << endl;

    cout << udevDecode("hello\\x20world") << endl;
    cout << udevDecode("hello\\x20\\x20world") << endl;

    cout << udevDecode("\\x20beginning") << endl;
    cout << udevDecode("ending\\x20") << endl;

    cout << udevDecode("woody\\x27s") << endl;
}
