#include <iostream>

#include <blocxx/AppenderLogger.hpp>
#include <blocxx/FileAppender.hpp>

#include <y2storage/StorageInterface.h>

using namespace storage;
using namespace std;
using namespace blocxx;

void 
initLogger()
    {
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
    int ret;
    initLogger();
    StorageInterface* s = createStorageInterface();
    string disk = "/dev/hdb";
    string dev;
    ret = s->destroyPartitionTable( disk, s->defaultDiskLabel() );
    if( ret ) cerr << "retcode:" << ret << endl;
    ret = s->commit();
    if( ret ) cerr << "retcode:" << ret << endl;
    unsigned long cyl = s->kbToCylinder( disk, 500*1024 );
    ret = s->createPartition( disk, PRIMARY, 1, cyl, dev );
    if( ret ) cerr << "retcode:" << ret << endl;
    cout << dev << endl;
    ret = s->commit();
    if( ret ) cerr << "retcode:" << ret << endl;
    ret = s->changePartitionId( dev, 0x82 );
    if( ret ) cerr << "retcode:" << ret << endl;
    ret = s->commit();
    if( ret ) cerr << "retcode:" << ret << endl;
    ret = s->removePartition( dev );
    if( ret ) cerr << "retcode:" << ret << endl;
    ret = s->commit();
    if( ret ) cerr << "retcode:" << ret << endl;
    ret = s->createPartition( disk, PRIMARY, 1, cyl*4, dev );
    if( ret ) cerr << "retcode:" << ret << endl;
    ret = s->commit();
    if( ret ) cerr << "retcode:" << ret << endl;
    delete(s);
    }
