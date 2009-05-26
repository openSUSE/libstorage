
#include <iostream>
#include <locale>

#include <storage/HumanString.h>

using namespace std;
using namespace storage;


// Don't bother setting up locale for gettext since we don't have translations
// during package build.


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
    test("en_GB.UTF-8", 1024, true, 2, true);
    test("en_GB.UTF-8", 1024, true, 2, false);

    test("de_DE.UTF-8", 123456789, true, 4, false);
    test("de_DE.UTF-8", 123456789, false, 4, false);

    test("fr_FR.UTF-8", 123456789, true, 4, false);
    test("fr_FR.UTF-8", 123456789, false, 4, false);

    test("en_GB.UTF-8", 1000*1024, false, 2, false);
    test("de_DE.UTF-8", 1000*1024, false, 2, false);
    test("fr_FR.UTF-8", 1000*1024, false, 2, false);
}
