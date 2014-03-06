
#include <iostream>

#include "common.h"

#include "storage/SystemInfo/Dasdview.h"


using namespace std;
using namespace storage;


void
parse1()
{
    cout << "parse1" << endl;

    vector<string> lines = {
	"",
	"--- general DASD information --------------------------------------------------",
	"device node            : /dev/dasda",
	"busid                  : 0.0.0150",
	"type                   : ECKD",
	"device type            : hex 3390  	dec 13200",
	"",
	"--- DASD geometry -------------------------------------------------------------",
	"number of cylinders    : hex 2721  	dec 10017",
	"tracks per cylinder    : hex f  	dec 15",
	"blocks per track       : hex c  	dec 12",
	"blocksize              : hex 1000  	dec 4096",
	"",
	"--- extended DASD information -------------------------------------------------",
	"real device number     : hex 0  	dec 0",
	"subchannel identifier  : hex 0  	dec 0",
	"CU type  (SenseID)     : hex 3990  	dec 14736",
	"CU model (SenseID)     : hex e9  	dec 233",
	"device type  (SenseID) : hex 3390  	dec 13200",
	"device model (SenseID) : hex c  	dec 12",
	"open count             : hex 1  	dec 1",
	"req_queue_len          : hex 0  	dec 0",
	"chanq_len              : hex 0  	dec 0",
	"status                 : hex 5  	dec 5",
	"label_block            : hex 2  	dec 2",
	"FBA_layout             : hex 0  	dec 0",
	"characteristics_size   : hex 40  	dec 64",
	"confdata_size          : hex 100  	dec 256",
	"format                 : hex 2  	dec 2      	CDL formatted",
	"features               : hex 0  	dec 0      	default",
	"",
	"characteristics        : 3990e933 900c5e8c  1ff72032 2721000f",
	"                         e000e5a2 05940222  13090674 00000000",
	"                         00000000 00000000  32321502 dfee0001",
	"                         0677080f 007f4800  1f3c0000 00002721",
	"",
	"configuration_data     : dc010100 f0f0f2f1  f0f7f9f0 f0c9c2d4",
	"                         f7f5f0f0 f0f0f0f0  f0d3f2f3 f7f10040",
	"                         40000004 00000000  00000000 00002720",
	"                         00000000 00000000  00000000 00000000",
	"                         d4020000 f0f0f2f1  f0f7f9f3 f1c9c2d4",
	"                         f7f5f0f0 f0f0f0f0  f0d3f2f3 f7f10000",
	"                         d0000000 f0f0f2f1  f0f7f9f3 f1c9c2d4",
	"                         f7f5f0f0 f0f0f0f0  f0d3f2f3 f7f00000",
	"                         f0000001 f0f0f2f1  f0f7f9f0 f0c9c2d4",
	"                         f7f5f0f0 f0f0f0f0  f0d3f2f3 f7f10000",
	"                         00000000 00000000  00000000 00000000",
	"                         00000000 00000000  00000000 00000000",
	"                         00000000 00000000  00000000 00000000",
	"                         00000000 00000000  00000000 00000000",
	"                         80000000 2d001e00  00010004 0000020b",
	"                         000cc040 4eaff602  00000000 0000a000"
    };

    Dasdview dasdview("/dev/dasda", false);
    dasdview.parse(lines);

    cout << dasdview << endl;
}


void
parse2()
{
    cout << "parse2" << endl;

    vector<string> lines = {
	"",
	"--- general DASD information --------------------------------------------------",
	"device node            : /dev/dasdb",
	"busid                  : 0.0.0160",
	"type                   : ECKD",
	"device type            : hex 3390  	dec 13200",
	"",
	"--- DASD geometry -------------------------------------------------------------",
	"number of cylinders    : hex d0b  	dec 3339",
	"tracks per cylinder    : hex f  	dec 15",
	"blocks per track       : hex 0  	dec 0",
	"blocksize              : hex 200  	dec 512",
	"",
	"--- extended DASD information -------------------------------------------------",
	"real device number     : hex 0  	dec 0",
	"subchannel identifier  : hex 1  	dec 1",
	"CU type  (SenseID)     : hex 3990  	dec 14736",
	"CU model (SenseID)     : hex e9  	dec 233",
	"device type  (SenseID) : hex 3390  	dec 13200",
	"device model (SenseID) : hex a  	dec 10",
	"open count             : hex 1  	dec 1",
	"req_queue_len          : hex 0  	dec 0",
	"chanq_len              : hex 0  	dec 0",
	"status                 : hex 3  	dec 3",
	"label_block            : hex 2  	dec 2",
	"FBA_layout             : hex 1  	dec 1",
	"characteristics_size   : hex 40  	dec 64",
	"confdata_size          : hex 100  	dec 256",
	"format                 : hex 0  	dec 0      	NOT formatted",
	"features               : hex 0  	dec 0      	default",
	"",
	"characteristics        : 3990e933 900a5e8c  1ff72024 0d0b000f",
	"                         e000e5a2 05940222  13090674 00000000",
	"                         00000000 00000000  24241502 dfee0001",
	"                         0677080f 007f4800  1f3c0000 00000d0b",
	"",
	"configuration_data     : dc010100 f0f0f2f1  f0f7f9f0 f0c9c2d4",
	"                         f7f5f0f0 f0f0f0f0  f0d3f2f3 f7f10045",
	"                         40000004 00000000  00000000 00000d0a",
	"                         00000000 00000000  00000000 00000000",
	"                         d4020000 f0f0f2f1  f0f7f9f3 f1c9c2d4",
	"                         f7f5f0f0 f0f0f0f0  f0d3f2f3 f7f10000",
	"                         d0000000 f0f0f2f1  f0f7f9f3 f1c9c2d4",
	"                         f7f5f0f0 f0f0f0f0  f0d3f2f3 f7f00000",
	"                         f0000001 f0f0f2f1  f0f7f9f0 f0c9c2d4",
	"                         f7f5f0f0 f0f0f0f0  f0d3f2f3 f7f10000",
	"                         00000000 00000000  00000000 00000000",
	"                         00000000 00000000  00000000 00000000",
	"                         00000000 00000000  00000000 00000000",
	"                         00000000 00000000  00000000 00000000",
	"                         80000000 2d001e00  00010004 0000020b",
	"                         000cc045 c1360f2e  00000000 0000a000"
    };

    Dasdview dasdview("/dev/dasdb", false);
    dasdview.parse(lines);

    cout << dasdview << endl;
}


void
parse3()
{
    cout << "parse3" << endl;

    vector<string> lines = {
	"",
	"--- general DASD information --------------------------------------------------",
	"device node            : /dev/dasdc",
	"busid                  : 0.0.3333",
	"type                   : FBA ",
	"device type            : hex 9336  	dec 37686",
	"",
	"--- DASD geometry -------------------------------------------------------------",
	"number of cylinders    : hex 309  	dec 777",
	"tracks per cylinder    : hex 10  	dec 16",
	"blocks per track       : hex 80  	dec 128",
	"blocksize              : hex 200  	dec 512",
	"",
	"--- extended DASD information -------------------------------------------------",
	"real device number     : hex 0  	dec 0",
	"subchannel identifier  : hex 14  	dec 20",
	"CU type  (SenseID)     : hex 6310  	dec 25360",
	"CU model (SenseID)     : hex 80  	dec 128",
	"device type  (SenseID) : hex 9336  	dec 37686",
	"device model (SenseID) : hex 10  	dec 16",
	"open count             : hex 1  	dec 1",
	"req_queue_len          : hex 0  	dec 0",
	"chanq_len              : hex 0  	dec 0",
	"status                 : hex 5  	dec 5",
	"label_block            : hex 1  	dec 1",
	"FBA_layout             : hex 1  	dec 1",
	"characteristics_size   : hex 20  	dec 32",
	"confdata_size          : hex 0  	dec 0",
	"format                 : hex 1  	dec 1      	LDL formatted",
	"features               : hex 0  	dec 0      	default",
	"",
	"characteristics        : 30082111 02000000  006f0000 03090000",
	"                         4e200000 00000000  00000000 00000000",
	"",
	""
    };

    Dasdview dasdview("/dev/dasdc", false);
    dasdview.parse(lines);

    cout << dasdview << endl;
}


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    parse1();
    parse2();
    parse3();
}
