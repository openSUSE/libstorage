
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

    void write_fstab(const list<string>& lines);
    void write_crypttab(const list<string>& lines);

    void print_fstab();
    void print_crypttab();

    void print_partitions(StorageInterface* s, const string& disk);

}
