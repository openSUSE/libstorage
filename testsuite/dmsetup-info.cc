
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/CmdDmsetup.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"system-abuild/253/0/3/LVM-OMPzXFm3am1zIlAVdQi5WxtmyNcevmRn25elwrmng2K9f1Vnfex9VFkN4tEXD8xI",
	"system-test/253/7/1/LVM-OMPzXFm3am1zIlAVdQi5WxtmyNcevmRnAL4A6NByky35grIbJsJ8RYCQP0NqLsF7",
	"system-testsuite/253/6/1/LVM-OMPzXFm3am1zIlAVdQi5WxtmyNcevmRn47gLDXzm2P9srqwHhaiuUlNFmLlLJH9V",
	"system-btrfs/253/5/1/LVM-OMPzXFm3am1zIlAVdQi5WxtmyNcevmRnFSGfCGnRIJQ5lNx59k7JP7uK3fkyQl48",
	"system-arvin/253/1/2/LVM-OMPzXFm3am1zIlAVdQi5WxtmyNcevmRngOeeB8at0WP8Z3sAyZv5BtKT49j6TwN6",
	"system-swap/253/3/1/LVM-OMPzXFm3am1zIlAVdQi5WxtmyNcevmRnKKC5tfbWLpsF2toVKQtE0wxeQpUp8bV0",
	"system-root/253/2/1/LVM-OMPzXFm3am1zIlAVdQi5WxtmyNcevmRn89Crg8K5dO0VvjVwurvCLK4efhWCtRfN",
	"system-giant--xfs/253/4/3/LVM-OMPzXFm3am1zIlAVdQi5WxtmyNcevmRnHdH1l4B0LUptYisBQuf33vP5rGosS1e2"
    };

    CmdDmsetupInfo cmddmsetupinfo(false);
    cmddmsetupinfo.parse(lines);

    cout << cmddmsetupinfo << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
}
