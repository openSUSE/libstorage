#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <iostream>
#include <ext/stdio_filebuf.h>

#include "EvmsAccess.h"
#include "y2storage/AppUtil.h"
#include "y2storage/StorageInterface.h"
#include "y2storage/StorageTmpl.h"

using namespace std;
using namespace storage;

#define OPT_LOG_PATH 200
#define OPT_LOG_NAME 201
#define OPT_LOG_FILE 202
#define OPT_SOCKET   203
#define OPT_TIMEOUT  204

static struct option LongOpt_arm[] =
    {
	{ "log_path",   required_argument, NULL, OPT_LOG_PATH   },
	{ "log_name",   required_argument, NULL, OPT_LOG_NAME   },
	{ "log_file",   required_argument, NULL, OPT_LOG_FILE   },
	{ "socket",     required_argument, NULL, OPT_SOCKET     },
	{ "timeout",    required_argument, NULL, OPT_TIMEOUT   }
    };

int EvmsCreateCoCmd( EvmsAccess& evms, const string& params )
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
    return( ret );
    }

int EvmsExtendCoCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string name = extractNthWord( 0, params );
    string device = extractNthWord( 1, params );
    cerr << "EvmsExtendCoCmd name:" << name << " device:" << device << endl; 
    ret = evms.extendCo( name, device );
    return( ret );
    }

int EvmsShrinkCoCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string name = extractNthWord( 0, params );
    string device = extractNthWord( 1, params );
    cerr << "EvmsShrinkCoCmd name:" << name << " device:" << device << endl; 
    ret = evms.shrinkCo( name, device );
    return( ret );
    }

int EvmsDeleteCoCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string name = extractNthWord( 0, params );
    cerr << "EvmsShrinkCoCmd name:" << name << endl; 
    ret = evms.deleteCo( name );
    return( ret );
    }

int EvmsCreateLvCmd( EvmsAccess& evms, const string& params )
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
    return( ret );
    }

int EvmsDeleteLvCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string coname = extractNthWord( 0, params );
    string lvname = extractNthWord( 1, params );
    cerr << "EvmsRemoveLvCmd coName:" << coname << " lvname:" << lvname << endl;
    ret = evms.deleteLv( lvname, coname );
    return( ret );
    }

int EvmsResizeLvCmd( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string coname = extractNthWord( 0, params );
    string lvname = extractNthWord( 1, params );
    unsigned long long sizeK = 0;
    extractNthWord( 2, params ) >> sizeK;
    cerr << "EvmsResizeLvCmd coName:" << coname << " lvname:" << lvname 
         << " newSizeK:" << sizeK << endl;
    ret = evms.changeLvSize( lvname, coname, sizeK );
    return( ret );
    }

int EvmsCreateCompatVolume( EvmsAccess& evms, const string& params )
    {
    int ret = 0;
    string name = extractNthWord( 0, params );
    cerr << "EvmsCreateCompatVolume name:" << name << endl; 
    ret = evms.createCompatVol( name );
    return( ret );
    }

struct CmdEntry
    {
    char *cmd;
    int (*fnc)( EvmsAccess& evms, const string& params );
    };

static CmdEntry cmds[] = {
    { "create_co", EvmsCreateCoCmd },
    { "extend_co", EvmsExtendCoCmd },
    { "shrink_co", EvmsShrinkCoCmd },
    { "delete_co", EvmsDeleteCoCmd },
    { "create_lv", EvmsCreateLvCmd },
    { "delete_lv", EvmsDeleteLvCmd },
    { "resize_lv", EvmsResizeLvCmd },
    { "cc_vol", EvmsCreateCompatVolume }
    };

void searchExecCmd( const string& cmd, EvmsAccess& evms, const string& params,
                    ostream& output )
    {
    int ret = 0;
    unsigned i=0;
    while( i<lengthof(cmds) && cmds[i].cmd!=cmd )
	i++;
    if( i<lengthof(cmds) )
	{
	ret = cmds[i].fnc( evms, params );
	output << ret << endl;
	}
    else if( cmd=="list" )
	{
	output << evms;
	}
    else
	{
	ret = EVMS_HELPER_UNKNOWN_CMD;
	output << ret << endl;
	}
    }

void loop_cin()
    {
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
	    searchExecCmd( cmd, evms, extractNthWord( 1, cmdline, true ), cout );
	}
    while( !end_program );
    }

static string spath;
static int semid = -1;
static int lsock = -1;

void cleanup()
    {
    if( lsock>=0 )
	close(lsock);
    if( !spath.empty() )
	unlink( spath.c_str() );
    if( semid>0 )
	semctl( semid, 1, IPC_RMID );
    }

void sigterm_handler(int sig)
    {
    y2warning("Received signal %d; terminating.", sig);
    cleanup();
    exit( 255 );
    }

typedef __gnu_cxx::stdio_filebuf<char> my_strbuf;
class mstream : public std::iostream
  {
  public:
    mstream( int fd, boolean input ) : std::iostream(NULL) 
      {
#if __GNUC__ >= 4
      my_strbuf *strbuf = new my_strbuf(fd, input?std::ios::in:std::ios::out);
#else
      my_strbuf *strbuf = new my_strbuf(fd, input?std::ios::in:std::ios::out,
					true, static_cast<size_t>(BUFSIZ));
#endif
      rdbuf(strbuf);
      }
  };

void
loop_socket( const string& spath, int timeout )
    {
    bool ok = true;
    fd_set fdset;
    struct sockaddr_un saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sun_family = AF_UNIX;
    strncpy( saddr.sun_path, spath.c_str(), sizeof(saddr.sun_path)-1 );

    int lsock = socket(AF_UNIX, SOCK_STREAM, 0);
    if( lsock<0 )
	{
	y2error( "error creating socket %s", strerror(errno));
	ok = false;
	}
    if( ok &&
	bind( lsock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
	{
	y2error( "error bind to socket %s", strerror(errno));
	shutdown(lsock, SHUT_RDWR);
	close(lsock);
	}
    if( ok && listen( lsock, 20 )<0 )
	{
	y2error( "error listen on socket %s", strerror(errno));
	shutdown(lsock, SHUT_RDWR);
	close(lsock);
	}
    EvmsAccess* evms = NULL;
    if( ok )
	{
	signal(SIGTERM, sigterm_handler);
	signal(SIGQUIT, sigterm_handler);
	signal(SIGINT, sigterm_handler);
	key_t k = ftok( spath.c_str(), IPC_PROJ_ID );
	semid = semget( k, 1, 0 );
	y2milestone( "ipc key:%x semid:%d", k, semid );
	evms = new EvmsAccess;
	}
    bool end_program = false;
    while( ok && !end_program )
	{
	struct timeval tv = { timeout/1000, (timeout%1000)*1000 };
	FD_ZERO( &fdset );
	FD_SET( lsock, &fdset );
	int ret = select( lsock+1, &fdset, NULL, NULL, &tv );
	if( ret<0 || (ret>0&&!FD_ISSET( lsock, &fdset )) )
	    {
	    if( errno != EINTR )
		y2warning( "select: %s", strerror(errno));
	    }
	else if( ret>0 )
	    {
	    socklen_t aux = sizeof(saddr);
	    int newsock = accept( lsock, (struct sockaddr *)&saddr, &aux );
	    if( newsock<0 )
		{
		if( errno != EINTR )
		    y2warning( "accept: %s", strerror(errno));
		}
	    else
		{
		mstream input( newsock, true );
		mstream output( newsock, false );
		string line;
		getline( input, line );
		y2milestone( "got line:\"%s\"", line.c_str() );
		string cmd = extractNthWord( 0, line );
		if( cmd == "exit" )
		    end_program = true;
		else
		    searchExecCmd( cmd, *evms, extractNthWord( 1, line, true ), 
		                   output );
		}
	    }
	else
	    {
	    }
	}
    delete evms;
    }

int
main( int argc_iv, char** argv_ppcv )
    {
    int OptionChar_ii;
    int timeout = 1000;
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
	    case OPT_SOCKET:
		spath = optarg;
		break;
	    case OPT_TIMEOUT:
		string(optarg)>>timeout;
		break;
	    default: break;
	    }
	}
    createLogger( "y2storage", lname, lpath, lfile );
    y2milestone( "evms_helper (pid:%d) started socket:%s timout:%d", 
                 getpid(), spath.c_str(), timeout );
    if( spath.empty() )
	loop_cin();
    else
	loop_socket( spath, timeout );
    cleanup();
    }
