
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/CmdBtrfs.h"


using namespace std;
using namespace storage;


void
parse_good()
{
    TRACE();

    vector<string> lines = {
	"ID 257 gen 55 top level 5 path @",
	"ID 258 gen 48 top level 257 path boot/grub2/i386-pc",
	"ID 259 gen 48 top level 257 path boot/grub2/x86_64-efi",
	"ID 260 gen 48 top level 257 path home",
	"ID 261 gen 31 top level 257 path opt",
	"ID 262 gen 48 top level 257 path srv",
	"ID 263 gen 55 top level 257 path tmp",
	"ID 264 gen 48 top level 257 path usr/local",
	"ID 265 gen 48 top level 257 path var/crash",
	"ID 266 gen 48 top level 257 path var/lib/mailman",
	"ID 267 gen 48 top level 257 path var/lib/named",
	"ID 268 gen 48 top level 257 path var/lib/pgsql",
	"ID 269 gen 54 top level 257 path var/log",
	"ID 270 gen 48 top level 257 path var/opt",
	"ID 271 gen 54 top level 257 path var/spool",
	"ID 272 gen 54 top level 257 path var/tmp",
	"ID 276 gen 55 top level 257 path .snapshots",
	"ID 277 gen 53 top level 276 path .snapshots/1/snapshot",
	"ID 278 gen 54 top level 276 path .snapshots/2/snapshot"
    };

    CmdBtrfsSubvolumes cmd("/btrfs", false);
    cmd.parse(lines);

    cout << cmd << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse_good();
}
