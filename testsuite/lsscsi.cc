
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
	"[0:0:0:0]    disk                                    /dev/sda "
    };

    Lsscsi lsscsi(false);
    lsscsi.parse(lines);

    cout << lsscsi << endl;
}


void
parse3()
{
    cout << "parse3" << endl;

    vector<string> lines = {
	"[0:0:0:0]    disk    iqn.2014-03.com.example:34878ae1-e5a3-4cd2-b554-1e35aaf544a6,t,0x1  /dev/sda "
    };

    Lsscsi lsscsi(false);
    lsscsi.parse(lines);

    cout << lsscsi << endl;
}


void
parse4()
{
    cout << "parse4" << endl;

    vector<string> lines = {
	"[1:0:2:0]    disk    fc:0x200300a09827e27e0x650501   /dev/sdb ",
	"[1:0:2:1]    disk    fc:0x200300a09827e27e0x650501   /dev/sdc "
    };

    Lsscsi lsscsi(false);
    lsscsi.parse(lines);

    cout << lsscsi << endl;
}


void
parse5()
{
    cout << "parse5" << endl;

    vector<string> lines = {
	"[1:0:0:0]    storage fcoe:0x50001fe1501e6d7b0x660c00  -        ",
	"[1:0:0:1]    disk    fcoe:0x50001fe1501e6d7b0x660c00  /dev/sdb ",
	"[1:0:1:0]    storage fcoe:0x50001fe1501e6d7f0x660b00  -        ",
	"[1:0:1:1]    disk    fcoe:0x50001fe1501e6d7f0x660b00  /dev/sdc "
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
    parse3();
    parse4();
    parse5();
}
