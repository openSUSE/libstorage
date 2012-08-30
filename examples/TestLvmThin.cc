
#include <getopt.h>
#include <iostream>

#include <storage/StorageInterface.h>

using namespace storage;
using namespace std;

void progressbarCb( const string& id, unsigned cur, unsigned max )
    {
    cout << "PROGRESSBAR id:" << id << " cur:" << cur << " max:" << max << endl;
    }

void installInfoCb( const string& info )
    {
    cout << "INFO " << info << endl;
    }

void
printCommitActions(StorageInterface* s)
{
    list<CommitInfo> l;
    s->getCommitInfos(l);
    for (list<CommitInfo>::iterator i=l.begin(); i!=l.end(); ++i)
	cout << i->text << endl;
}

int doCommit( StorageInterface* s )
    {
    static int cnt = 1;

    //printCommitActions( s );
    int ret = s->commit();
    if( ret ) cerr << "retcode:" << ret << endl;
    if( ret==0 )
	{
	printCommitActions( s );
	}
    cnt++;
    return( ret );
    }

struct option long_options[] = {
    { "keep", 0, 0, 'k' },
    { 0, 0, 0, 0 }
    };

#define POOL_NAME "zzz_pool"
#define THIN_NAME "test_tvol"

int
main( int argc, char** argv )
    {
    int ret = 0;
    int c;
    bool keep = false;
    while( (c=getopt_long(argc, argv, "k", long_options, 0))!=EOF )
        {
        switch(c)
            {
            case 'k':
                keep = true;
                break;
            default:
                break;
            }
        }

    StorageInterface* s = createStorageInterface(Environment(false));
    s->setCallbackProgressBar( progressbarCb );
    s->setCallbackShowInstallInfo( installInfoCb );
    string device;
    deque<string> devs;
    devs.push_back("/dev/sdb8");
    devs.push_back("/dev/sdb9");
    if( ret==0 )
	{
	ret = s->createLvmVg( "testvg", 4*1024, false, devs );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = s->createLvmLvPool( "testvg", POOL_NAME, 4*1024*1024, device );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = s->changeLvChunkSize( "testvg", POOL_NAME, 128 );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    for( int i=0; i<5; ++i )
        {
        if( ret==0 )
            {
            string name = THIN_NAME;
            name += '0'+i+1;
            ret = s->createLvmLvThin( "testvg", name, POOL_NAME, 100*1024*1024, 
                                      device );
            if( ret ) cerr << "retcode:" << ret << endl;
            }
        }
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    if( !keep )
        {
        for( int i=0; i<5; ++i )
            {
            if( ret==0 )
                {
                string name = THIN_NAME;
                name += '0'+i+1;
                ret = s->removeLvmLv( "testvg", name );
                if( ret ) cerr << "retcode:" << ret << endl;
                }
            }
        if( ret==0 )
            {
            if( ret ) cerr << "retcode:" << ret << endl;
            }
        if( ret==0 )
            {
            ret = s->removeLvmLv( "testvg", POOL_NAME );
            if( ret ) cerr << "retcode:" << ret << endl;
            }
        if( ret==0 )
            {
            ret = doCommit( s );
            }
        if( ret==0 )
            {
            ret = s->removeLvmVg( "testvg" );
            if( ret ) cerr << "retcode:" << ret << endl;
            }
        if( ret==0 )
            {
            ret = doCommit( s );
            }
        }
    delete(s);
    }
