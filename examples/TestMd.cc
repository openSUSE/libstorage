
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
    if( ret==0 )
	{
	ret = s->destroyPartitionTable( disk, s->defaultDiskLabel() );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = s->createPartitionKb( disk, PTYPE_ANY, 0, 3500*1024, device );
	cout << "device:" << device << endl;
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = s->createPartitionMax( disk, EXTENDED, device );
	cout << "device:" << device << endl;
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    unsigned int num = 0;
    while( ret==0 && num++<15 )
	{
	ret = s->createPartitionAny( disk, 1024*1024, device );
	cout << "device:" << device << endl;
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	deque<string> ds;
	ds.push_back( "/dev/hdb5" );
	ds.push_back( "/dev/hdb6" );
	ds.push_back( "/dev/hdb7" );
	ret = s->createMd( "md0", RAID0, ds );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	deque<string> ds;
	ds.push_back( "/dev/hdb8" );
	ds.push_back( "/dev/hdb9" );
	ret = s->createMdAny( RAID1, ds, device );
	cout << "device:" << device << endl;
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	deque<string> ds;
	ds.push_back( "/dev/hdb10" );
	ds.push_back( "/dev/hdb11" );
	ds.push_back( "/dev/hdb12" );
	ret = s->createMd( "/dev/md2", RAID5, ds );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	deque<string> ds;
	ds.push_back( "/dev/hdb13" );
	ds.push_back( "/dev/hdb14" );
	ds.push_back( "/dev/hdb15" );
	ds.push_back( "/dev/hdb16" );
	ret = s->createMd( "/dev/md3", RAID6, ds );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	deque<string> ds;
	ds.push_back( "/dev/hdb17" );
	ds.push_back( "/dev/hdb18" );
	ds.push_back( "/dev/hdb19" );
	ds.push_back( "/dev/hdb1" );
	ret = s->createMd( "/dev/md4", RAID10, ds );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    /*
    if( ret==0 )
	{
	ret = s->removeVolume( "/dev/md1" );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    if( ret==0 )
	{
	ret = s->removeMd( "md0", false );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    */
    delete(s);
    }
