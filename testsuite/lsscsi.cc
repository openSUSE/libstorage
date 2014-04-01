
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/CmdLsscsi.h"


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


void
parse2()
{
    cout << "parse2" << endl;

    vector<string> lines = {
	"[0:0:0:0]    disk    iqn.2014-03.com.example:34878ae1-e5a3-4cd2-b554-1e35aaf544a6,t,0x1  /dev/sda "
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
    parse2();
}
