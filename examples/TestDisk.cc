#include <getopt.h>

#include <ycp/y2log.h>

#include <y2storage/StorageInterface.h>

using namespace storage;
using namespace std;

int
main( int argc_iv, char** argv_ppcv )
    {
    int ret;
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
    y2milestone( "dev %s", dev.c_str() );
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
