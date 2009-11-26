
#include <iostream>

#include <storage/AppUtil.h>


using namespace std;
using namespace storage;


int
main()
{
    cout << udevEncode("nothing") << endl;

    cout << udevEncode("hello world") << endl;
    cout << udevEncode("hello  world") << endl;

    cout << udevEncode(" beginning") << endl;
    cout << udevEncode("ending ") << endl;

    cout << udevEncode("woody's") << endl;
}
