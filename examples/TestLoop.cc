
#include <iostream>

#include <y2storage/StorageInterface.h>

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

    printCommitActions( s );
    int ret = s->commit();
    if( ret ) cerr << "retcode:" << ret << endl;
    if( ret==0 )
	{
	cout << "after commit " << cnt << endl;
	printCommitActions( s );
	cout << "after commit " << cnt << endl;
	}
    cnt++;
    return( ret );
    }

int
main( int argc_iv, char** argv_ppcv )
    {
    int ret = 0;
    initDefaultLogger();
    StorageInterface* s = createStorageInterface(Environment(false));
    s->setCallbackProgressBar( progressbarCb );
    s->setCallbackShowInstallInfo( installInfoCb );
    string disk = "/dev/hdb";
    string device;
    ret = s->createFileLoop( "/tmp/floop1", true, 60*1024, "/tmp/m3", 
                             "asdfghjkl", device );
    if( ret ) cerr << "retcode:" << ret << endl;
    cout << "device:" << device << endl;
    if( ret==0 )
	{
	ret = s->createFileLoop( "/tmp/floop2", true, 60*1024, "/tmp/m4", 
				 "123456789", device );
	if( ret ) cerr << "retcode:" << ret << endl;
	cout << "device:" << device << endl;
	}
    if( ret==0 )
	{
	ret = s->addFstabOptions( device, "noauto" );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = s->removeFileLoop( "/tmp/floop2", false );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = s->removeFileLoop( "/tmp/floop1", false );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    delete(s);
    }
