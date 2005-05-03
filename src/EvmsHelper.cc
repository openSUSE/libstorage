#include <getopt.h>

#include <iostream>

#include "EvmsAccess.h"
#include "y2storage/AppUtil.h"
#include "y2storage/StorageInterface.h"
#include "y2storage/StorageTmpl.h"

using namespace std;
using namespace storage;

#define OPT_LOG_PATH 200
#define OPT_LOG_NAME 201
#define OPT_LOG_FILE 202

static struct option LongOpt_arm[] =
    {
	{ "log_path",   required_argument, NULL, OPT_LOG_PATH   },
	{ "log_name",   required_argument, NULL, OPT_LOG_NAME   },
	{ "log_file",   required_argument, NULL, OPT_LOG_FILE   }
    };

void EvmsListCmd( EvmsAccess& evms, const string& )
    {
    cout << evms;
    }

void EvmsCreateCoCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string name = extractNthWord( 0, params );
    unsigned long long peSizeK = 0;
    extractNthWord( 1, params ) >> peSizeK;
    boolean lvm2 = false;
    extractNthWord( 2, params ) >> lvm2;
    list <string> devs;
    string dev;
    unsigned i=3;
    while( !(dev=extractNthWord( i, params )).empty() )
	{
	devs.push_back( dev );
	++i;
	}
    cerr << "EvmsCreateCoCmd name:" << name << " PeSize:" << peSizeK 
         << " lvm2:" << lvm2 << " devs:" << devs << endl;
    ret = evms.createCo( name, peSizeK, lvm2, devs );
    cout << ret << endl;
    }

void EvmsExtendCoCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string name = extractNthWord( 0, params );
    string device = extractNthWord( 1, params );
    cerr << "EvmsExtendCoCmd name:" << name << " device:" << device << endl; 
    ret = evms.extendCo( name, device );
    cout << ret << endl;
    }

void EvmsShrinkCoCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string name = extractNthWord( 0, params );
    string device = extractNthWord( 1, params );
    cerr << "EvmsShrinkCoCmd name:" << name << " device:" << device << endl; 
    ret = evms.shrinkCo( name, device );
    cout << ret << endl;
    }

void EvmsDeleteCoCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string name = extractNthWord( 0, params );
    cerr << "EvmsShrinkCoCmd name:" << name << endl; 
    ret = evms.deleteCo( name );
    cout << ret << endl;
    }

void EvmsCreateLvCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string coname = extractNthWord( 0, params );
    string lvname = extractNthWord( 1, params );
    unsigned long long sizeK = 0;
    extractNthWord( 2, params ) >> sizeK;
    unsigned long stripe = 1;
    unsigned long long stripeSize = 0;
    string tmp;
    if( !(tmp=extractNthWord( 3, params )).empty() )
	{
	tmp >> stripe;
	if( !(tmp=extractNthWord( 4, params )).empty() )
	    tmp >> stripeSize;
	}
    cerr << "EvmsCreateLvCmd coName:" << coname << " lvname:" << lvname 
         << " sizeK:" << sizeK << " stripes:" << stripe 
         << " stripeSize:" << stripeSize << endl;
    ret = evms.createLv( lvname, coname, sizeK, stripe, stripeSize );
    cout << ret << endl;
    }

void EvmsDeleteLvCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string coname = extractNthWord( 0, params );
    string lvname = extractNthWord( 1, params );
    cerr << "EvmsRemoveLvCmd coName:" << coname << " lvname:" << lvname << endl;
    ret = evms.deleteLv( lvname, coname );
    cout << ret << endl;
    }

void EvmsResizeLvCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string coname = extractNthWord( 0, params );
    string lvname = extractNthWord( 1, params );
    unsigned long long sizeK = 0;
    extractNthWord( 2, params ) >> sizeK;
    cerr << "EvmsResizeLvCmd coName:" << coname << " lvname:" << lvname 
         << " newSizeK:" << sizeK << endl;
    ret = evms.changeLvSize( lvname, coname, sizeK );
    cout << ret << endl;
    }

void EvmsCreateCompatVolume( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string name = extractNthWord( 0, params );
    cerr << "EvmsCreateCompatVolume name:" << name << endl; 
    ret = evms.createCompatVol( name );
    cout << ret << endl;
    }

struct CmdEntry
    {
    char *cmd;
    void (*fnc)( EvmsAccess& evms, const string& params );
    };

static CmdEntry cmds[] = {
    { "list", EvmsListCmd },
    { "create_co", EvmsCreateCoCmd },
    { "extend_co", EvmsExtendCoCmd },
    { "shrink_co", EvmsShrinkCoCmd },
    { "delete_co", EvmsDeleteCoCmd },
    { "create_lv", EvmsCreateLvCmd },
    { "delete_lv", EvmsDeleteLvCmd },
    { "resize_lv", EvmsResizeLvCmd },
    { "cc_vol", EvmsCreateCompatVolume }
    };

void searchExecCmd( const string& cmd, EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    unsigned i=0;
    while( i<lengthof(cmds) && cmds[i].cmd!=cmd )
	i++;
    if( i<lengthof(cmds) )
	cmds[i].fnc( evms, params );
    else
	{
	ret = EVMS_HELPER_UNKNOWN_CMD;
	cout << ret << endl;
	}
    }

int
main( int argc_iv, char** argv_ppcv )
    {
    int OptionChar_ii;
    string lfile = "y2log";
    string lpath = "/var/log/YaST2";
    string lname = "evms_helper";

    while( (OptionChar_ii=getopt_long( argc_iv, argv_ppcv, "", LongOpt_arm,
                                       NULL )) != EOF )
	{
	switch (OptionChar_ii)
	    {
	    case OPT_LOG_FILE:
		lfile = optarg;
		break;
	    case OPT_LOG_NAME:
		lname = optarg;
		break;
	    case OPT_LOG_PATH:
		lpath = optarg;
		break;
	    default: break;
	    }
	}
    createLogger( "y2storage", lname, lpath, lfile );
    y2milestone( "evms_helper started" );
    EvmsAccess evms;
    bool end_program = false;
    do
	{
	string cmdline;
	cout << "CMD> ";
	getline( cin, cmdline );
	string cmd = extractNthWord( 0, cmdline );
	if( cmd == "exit" )
	    end_program = true;
	else
	    searchExecCmd( cmd, evms, extractNthWord( 1, cmdline, true ) );
	}
    while( !end_program );
    }
