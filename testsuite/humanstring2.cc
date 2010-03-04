
#include <stdlib.h>
#include <iostream>
#include <locale>

#include <storage/HumanString.h>


using namespace std;
using namespace storage;


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
    const char* localedir = getenv("LOCALEDIR");
    if (localedir)
	bindtextdomain("libstorage", localedir);

    cout << "-- 1 --" << endl;
    test("en_GB.UTF-8", "42", true);			// FAILS: classic=true needs a suffix
    test("en_GB.UTF-8", "42B", true);
    test("en_GB.UTF-8", "42 b", true);

    cout << "-- 2 --" << endl;
    test("en_GB.UTF-8", "42", false);
    test("en_GB.UTF-8", "42b", false);
    test("en_GB.UTF-8", "42 B", false);

    cout << "-- 3 --" << endl;
    test("en_GB.UTF-8", "12.4GB", true);
    test("en_GB.UTF-8", "12.4 GB", true);

    cout << "-- 4 --" << endl;
    test("en_GB.UTF-8", "12.4GB", false);
    test("en_GB.UTF-8", "12.4 gb", false);
    test("en_GB.UTF-8", "12.4g", false);
    test("en_GB.UTF-8", "12.4 G", false);

    cout << "-- 5 --" << endl;
    test("en_GB.UTF-8", "123,456 kB", false);
    test("de_DE.UTF-8", "123.456 kB", false);
    test("fr_FR.UTF-8", "123 456 ko", false);

    cout << "-- 6 --" << endl;
    test("en_GB.UTF-8", "123,456.789kB", false);
    test("de_DE.UTF-8", "123.456,789kB", false);
    test("fr_FR.UTF-8", "123 456,789ko", false);

    cout << "-- 7 --" << endl;
    test("en_GB.UTF-8", "123,456.789 kB", false);
    test("de_DE.UTF-8", "123.456,789 kB", false);
    test("fr_FR.UTF-8", "123 456,789 ko", false);

    cout << "-- 8 --" << endl;
    test("fr_FR.UTF-8", "5Go", false);
    test("fr_FR.UTF-8", "5 Go", false);

    cout << "-- 9 --" << endl;
    test("en_US.UTF-8", "5 G B", false);		// FAILS
    test("de_DE.UTF-8", "12.34 kB", false);		// FAILS
    test("fr_FR.UTF-8", "12 34 Go", false);		// FAILS

    cout << "-- 10 --" << endl;
    test("en_GB.UTF-8", "3.14 G", false);
    test("en_GB.UTF-8", "3.14 GB", false);
    test("en_GB.UTF-8", "3.14 GiB", false);
}
