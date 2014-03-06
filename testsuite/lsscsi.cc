
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/Lsscsi.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"[0:0:0:0]    disk    sata:                           /dev/sda ",
	"[1:0:0:0]    cd/dvd  sata:                           /dev/sr0 ",
	"[6:0:0:0]    disk    usb: 2-1.5:1.0                  /dev/sdb "
    };

    Lsscsi lsscsi(false);
    lsscsi.parse(lines);

    cout << lsscsi << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
