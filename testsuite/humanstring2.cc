
#include <stdlib.h>
#include <iostream>
#include <locale>

#include <storage/HumanString.h>


using namespace std;
using namespace storage;


// During package build translations are installed into buildroot so are not
// available during testsuite run.


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
    test("fr_FR.UTF-8", "123 456 ko", false);

    test("en_GB.UTF-8", "123,456.789kB", false);
    test("de_DE.UTF-8", "123.456,789kB", false);
    test("fr_FR.UTF-8", "123 456,789ko", false);

    test("en_GB.UTF-8", "123,456.789 kB", false);
    test("de_DE.UTF-8", "123.456,789 kB", false);
    test("fr_FR.UTF-8", "123 456,789 ko", false);

    test("fr_FR.UTF-8", "5Go", false);
    test("fr_FR.UTF-8", "5 Go", false);

    test("en_US.UTF-8", "5 G B", false);		// FAILS
    test("de_DE.UTF-8", "12.34 kB", false);		// FAILS
    test("fr_FR.UTF-8", "12 34 Go", false);		// FAILS

    test("en_GB.UTF-8", "3.14 G", false);
    test("en_GB.UTF-8", "3.14 GB", false);
    test("en_GB.UTF-8", "3.14 GiB", false);
}
