
#include <getopt.h>
#include <iostream>

#include <storage/StorageInterface.h>

using namespace storage;
using namespace std;

int
main( int argc_iv, char** argv_ppcv )
    {
    int ret;
    StorageInterface* s = createStorageInterface(Environment(false));
    string dev;
    ret = s->destroyPartitionTable("/dev/hdb", s->defaultDiskLabel("/dev/hdb"));
    if( ret ) cerr << "retcode:" << ret << endl;
    ret = s->createPartitionKb("/dev/hdb", EXTENDED, RegionInfo(5000*1024, 15000*1024), dev);
    cout << dev << endl;
    if( ret ) cerr << "retcode:" << ret << endl;
    unsigned kb = s->cylinderToKb( "/dev/hdb", 40 );
    int i=0;
    while( i<40 && ret==0 )
	{
	ret = s->createPartitionKb("/dev/hdb", LOGICAL, RegionInfo(5000*1024+i*kb, kb), dev);
	if(ret==0) cout << dev << endl;
	i++;
	}
    ret = s->commit();
    if( ret ) cerr << "retcode:" << ret << endl;
    delete(s);
    }
