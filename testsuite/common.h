
#include <string>

#include <y2storage/StorageInterface.h>


using std::string;


extern bool testmode;

extern string disk;

void
parse_command_line (int argc, char* argv[]);


namespace storage
{

    struct TestEnvironment : public Environment
    {
	TestEnvironment() : Environment(false)
	{
	    testmode = ::testmode;
	    autodetect = false;
	}
    };

}

