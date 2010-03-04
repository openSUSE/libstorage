
#include <stdlib.h>
#include <iostream>
#include <locale>

#include <storage/HumanString.h>


using namespace std;
using namespace storage;


void
test(const char* loc, unsigned long long size, bool classic,
     int precision, bool omit_zeroes)
{
    locale::global(locale(loc));
    cout << byteToHumanString(size, classic, precision, omit_zeroes) << endl;
}


int
main()
{
    const char* localedir = getenv("LOCALEDIR");
    if (localedir)
	bindtextdomain("libstorage", localedir);

    cout << "-- 1 --" << endl;
    test("en_GB.UTF-8", 1024, true, 2, true);
    test("en_GB.UTF-8", 1024, true, 2, false);

    cout << "-- 2 --" << endl;
    test("de_DE.UTF-8", 123456789, true, 4, false);
    test("de_DE.UTF-8", 123456789, false, 4, false);

    cout << "-- 3 --" << endl;
    test("fr_FR.UTF-8", 123456789, true, 4, false);
    test("fr_FR.UTF-8", 123456789, false, 4, false);

    cout << "-- 4 --" << endl;
    test("en_GB.UTF-8", 1000*1024, false, 2, false);
    test("de_DE.UTF-8", 1000*1024, false, 2, false);
    test("fr_FR.UTF-8", 1000*1024, false, 2, false);
}
