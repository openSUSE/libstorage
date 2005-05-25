#include <iostream>

#include <blocxx/AppenderLogger.hpp>
#include <blocxx/FileAppender.hpp>

#include <y2storage/StorageInterface.h>

using namespace storage;
using namespace std;

void scrollbarCb( const string& id, unsigned cur, unsigned max )
    {
    cout << "SCROLLBAR id:" << id << " cur:" << cur << " max:" << max << endl;
    }

void installInfoCb( const string& info )
    {
    cout << "INFO " << info << endl;
    }

void
printCommitActions( StorageInterface* s )
    {
    deque<string> l = s->getCommitActions( false );
    for( deque<string>::iterator i=l.begin(); i!=l.end(); ++i )
	cout << *i << endl;
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

void 
initLogger()
    {
    using namespace blocxx;

    String name = "testlog";
    LoggerConfigMap configItems;
    String StrKey;
    String StrPath;
    StrKey.format("log.%s.location", name.c_str());
    StrPath = "/var/log/YaST2/y2log";
    configItems[StrKey] = StrPath;
    LogAppenderRef logApp = 
	LogAppender::createLogAppender( name, LogAppender::ALL_COMPONENTS, 
	                                LogAppender::ALL_CATEGORIES, 
					"%d %-5p %c - %m",
					LogAppender::TYPE_FILE,
					configItems );
    LoggerRef log( new AppenderLogger("libstorage", E_INFO_LEVEL, logApp));
    Logger::setDefaultLogger(log);
    }

int
main( int argc_iv, char** argv_ppcv )
    {
    int ret = 0;
    initLogger();
    //StorageInterface* s = createStorageInterface(false,true);
    StorageInterface* s = createDefaultStorageInterface();
    s->setCallbackProgressBar( scrollbarCb );
    s->setCallbackShowInstallInfo( installInfoCb );
    string disk = "/dev/hdb";
    string device;
    deque<string> devs;
    deque<PartitionInfo> infos;
    s->getPartitionsOfDisk( disk, infos );
    bool lvm1 = false;
    for( deque<PartitionInfo>::const_iterator i=infos.begin(); i!=infos.end(); ++i )
	if( i->partitionType!=EXTENDED && i->nr<16 )
	    devs.push_back( i->name );
    if( ret==0 )
	{
	ret = s->createEvmsContainer( "testevms", 8*1024, lvm1, devs );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = s->createEvmsVolume( "testevms", "test1", 1*1024, lvm1?1:2, device );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 && devs.size()>0 )
	{
	deque<string> ds;
	ds.push_back( devs.back() );
	ret = s->shrinkEvmsContainer( "testevms", ds );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    if( ret==0 )
	{
	ret = s->resizeVolume( device, 2900*1024 );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
#if 0
    if( ret==0 )
	{
	ret = s->createPartitionAny( disk, 1*1024*1024, device );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
#endif
    string ext_device = devs.back();
    if( ret==0 )
	{
	deque<string> ds;
	ds.push_back( ext_device );
	ret = s->extendEvmsContainer( "testevms", ds );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = s->createEvmsVolume( "testevms", "test2", 3*1024, 1, device );
	cout << "device:" << device << endl;
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    if( ret==0 )
	{
	ret = s->removeEvmsVolumeByDevice( device );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    if( ret==0 )
	{
	ret = s->createEvmsVolume( "testevms", "test2", 3*1024/2, 2, device );
	cout << "device:" << device << endl;
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    if( ret==0 )
	{
	ret = s->removeEvmsVolume( "testevms", "test2" );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    if( ret==0 )
	{
	deque<string> ds;
	ds.push_back( ext_device );
	ret = s->shrinkEvmsContainer( "testevms", ds );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    if( ret==0 )
	{
	ret = s->removeEvmsContainer( "testevms" );
	if( ret ) cerr << "retcode:" << ret << endl;
	}
    if( ret==0 )
	{
	ret = doCommit( s );
	}
    delete(s);
    }
