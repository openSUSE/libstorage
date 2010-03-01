
#include <storage/StorageInterface.h>


namespace storage
{

    struct TestEnvironment : public Environment
    {
	TestEnvironment() : Environment(false)
	{
	    testmode = true;
	    autodetect = false;
	    logdir = testdir = "tmp";
	}
    };


    void setup_logger();

    void setup_system(const string& name);

}
