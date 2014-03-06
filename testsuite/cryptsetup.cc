
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/CmdCryptsetup.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"/dev/mapper/cr_test is active and is in use.",
	"  type:    LUKS1",
	"  cipher:  aes-cbc-essiv:sha256",
	"  keysize: 256 bits",
	"  device:  /dev/sda2",
	"  offset:  4096 sectors",
	"  size:    487348224 sectors",
	"  mode:    read/write",
    };

    CmdCryptsetup cmdcryptsetup("cr_test", false);
    cmdcryptsetup.parse(lines);

    cout << cmdcryptsetup << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
