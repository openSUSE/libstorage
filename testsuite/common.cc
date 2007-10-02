
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#include "common.h"


bool testmode = true;

string disk = "/dev/hdb";


void usage () __attribute__ ((__noreturn__));

void
usage ()
{
    fprintf (stderr, "usage: %s [--real] [--disk string]\n",
	     program_invocation_short_name);
    exit (EXIT_FAILURE);
}


void
parse_command_line (int argc, char* argv[])
{
    struct option long_options[] = {
	{ "real", 0, 0, 'r' },
	{ "disk", 1, 0, 'd' },
	{ 0, 0, 0, 0 }
    };

    while (true)
    {
	int c = getopt_long (argc, argv, "+rd:", long_options, 0);
	if (c == -1)
	    break;

	switch (c)
	{
	    case 'r':
		testmode = false;
		break;

	    case 'd':
		disk = optarg;
		break;

	    default:
		usage ();
	}
    }

    if (optind != argc)
	usage ();
}

