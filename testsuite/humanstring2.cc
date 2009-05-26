
#include <iostream>
#include <locale>

#include <storage/HumanString.h>

using namespace std;
using namespace storage;


// Don't bother setting up locale for gettext since we don't have translations
// during package build.


void
test(const char* loc, const char* str, bool classic)
{
    locale::global(locale(loc));

    unsigned long long size = 0;

    if (humanStringToByte(str, classic, size))
	cout << "parse ok " << size << endl;
    else
	cout << "parse error" << endl;
}


int
main()
{
    test("en_GB.UTF-8", "42", true);			// FAILS: classic=true needs a suffix
    test("en_GB.UTF-8", "42B", true);
    test("en_GB.UTF-8", "42 b", true);

    test("en_GB.UTF-8", "42", false);
    test("en_GB.UTF-8", "42b", false);
    test("en_GB.UTF-8", "42 B", false);

    test("en_GB.UTF-8", "12.4GB", true);
    test("en_GB.UTF-8", "12.4 GB", true);

    test("en_GB.UTF-8", "12.4GB", false);
    test("en_GB.UTF-8", "12.4 gb", false);
    test("en_GB.UTF-8", "12.4g", false);
    test("en_GB.UTF-8", "12.4 G", false);

    test("en_GB.UTF-8", "123,456 kB", false);
    test("de_DE.UTF-8", "123.456 kB", false);
    test("fr_FR.UTF-8", "123 456 kB", false);

    test("en_GB.UTF-8", "123,456.789kB", false);
    test("de_DE.UTF-8", "123.456,789kB", false);
    test("fr_FR.UTF-8", "123 456,789kB", false);

    test("en_GB.UTF-8", "123,456.789 kB", false);
    test("de_DE.UTF-8", "123.456,789 kB", false);
    test("fr_FR.UTF-8", "123 456,789 kB", false);

    test("fr_FR.UTF-8", "5GB", false);
    test("fr_FR.UTF-8", "5 GB", false);

    test("en_US.UTF-8", "5 G B", false);		// FAILS
    test("de_DE.UTF-8", "12.34 kB", false);		// FAILS
    test("fr_FR.UTF-8", "12 34 GB", false);		// FAILS

    test("en_GB.UTF-8", "3.14 G", false);
    test("en_GB.UTF-8", "3.14 GB", false);
    test("en_GB.UTF-8", "3.14 GiB", false);
}
