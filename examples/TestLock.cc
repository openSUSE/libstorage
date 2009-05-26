/*
 * Author:	Arvin Schnell <aschnell@suse.de>
 */


#include <getopt.h>
#include <iostream>

#include <storage/Lock.h>
#include <storage/Storage.h>

using namespace std;
using namespace storage;


const char* program_name;


void help() __attribute__ ((__noreturn__));

void
help()
{
    cout << "Usage: " << program_name << " [OPTION]..." << endl
	 << endl
	 << "Mandatory arguments to long options are mandatory for short options too." << endl
	 << "  -r, --rd              get read-only lock (default)" << endl
	 << "  -w, --wr              get read-write lock" << endl
	 << endl
	 << "  -s, --sleep=SECONDS   sleep for SECONDS seconds after getting lock (30 seconds default)" << endl
	 << endl
	 << "  -h, --help            show this help" << endl;

    exit(EXIT_SUCCESS);
}


void usage() __attribute__ ((__noreturn__));

void
usage()
{
    cerr << "Try " << program_name << " --help for more information." << endl;
    exit(EXIT_FAILURE);
}


int
main(int argc, char** argv)
{
    program_name = argv[0];

    bool readonly = true;
    int sleep = 30;

    struct option long_options[] = {
	{ "rd", 0, 0, 'r' },
	{ "wr", 0, 0, 'w' },
	{ "sleep", 1, 0, 's' },
	{ "help", 0, 0, 'h' },
	{ 0, 0, 0, 0 }
    };

    while (true)
    {
	int c = getopt_long(argc, argv, "rws:h", long_options, 0);
	if (c == -1)
	    break;

	switch (c)
	{
	    case 'r':
		readonly = true;
		break;

	    case 'w':
		readonly = false;
		break;

	    case 's':
		sleep = atoi(optarg);
		break;

	    case 'h':
		help();

	    default:
		usage();
	}
    }

    if (optind < argc)
    {
	cerr << program_name << ": unrecognized option '" << argv[optind] << "'" << endl;
	usage();
    }

    Storage::initDefaultLogger();

    cout << "pid is " << getpid() << endl;

    Lock* lock = NULL;

    try
    {
	lock = new Lock(readonly);
    }
    catch (LockException& e)
    {
	cerr << "lock failed" << endl;
	cerr << "locker-pid is " << e.getLockerPid() << endl;
	exit(EXIT_FAILURE);
    }

    cout << "lock (" << (readonly ? "read-only" : "read-write") << ") succeeded" << endl;

    if (sleep > 0)
    {
	cout << "(sleeping for " << sleep << " seconds)" << endl;
	::sleep(sleep);
    }

    cout << "releasing lock" << endl;

    delete lock;

    exit(EXIT_SUCCESS);
}
