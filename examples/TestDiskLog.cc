#include <getopt.h>
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
    string dev;
    ret = s->destroyPartitionTable( "/dev/hdb", s->defaultDiskLabel() );
    if( ret ) cerr << "retcode:" << ret << endl;
    ret = s->createPartitionKb( "/dev/hdb", EXTENDED, 5000*1024, 10000*1024, dev );
    cout << dev << endl;
    if( ret ) cerr << "retcode:" << ret << endl;
    int i=0;
    while( i<40 && ret==0 )
	{
	ret = s->createPartitionKb( "/dev/hdb", LOGICAL, (5000+i*300)*1024, 300*1024, 
	                            dev );
	if(ret==0) cout << dev << endl;
	i++;
	}
    ret = s->commit();
    if( ret ) cerr << "retcode:" << ret << endl;
    delete(s);
    }
