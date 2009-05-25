
#include <stdlib.h>
#include <iostream>
#include <iterator>
#include <sstream>

#include "common.h"


using namespace storage;
using namespace std;


StorageInterface *s = 0;

void print_num_pvs( const string& vg)
{
    deque<string> disks;
    disks.push_back( "/dev/hda" );
    disks.push_back( "/dev/hdb" );
    disks.push_back( "/dev/sda" );
    disks.push_back( "/dev/sdb" );

    int count = 0;
    for ( deque<string>::iterator i = disks.begin();
	  i != disks.end(); i++ )
    {
	deque<PartitionInfo> pinfos;
	cout << s->getPartitionInfo( *i, pinfos )<< endl;

	for ( deque<PartitionInfo>::iterator p = pinfos.begin();
	      p != pinfos.end(); p++ )
	{
	    if ( p->v.usedByDevice == "/dev/" + vg )
		count++;	    
	}
    }
    cout << count << endl;
}


void extendVg( const string& vg, deque<string> devs_extend )
{
    printf("extendVg\n");

    s = createStorageInterface(TestEnvironment());

    deque<string> devs;
    devs.push_back( "/dev/hda1" );
    /* create volume group with the above disk */
    cout << s->createLvmVg( vg, 4, false, devs ) << endl;

    print_num_pvs( vg );

    /* extend it by devs_extend given devs */
    cout << s->extendLvmVg( vg, devs_extend ) << endl;
    
    print_num_pvs( vg );

    delete s;
}


int main( int argc_iv, char** argv_ppcv )
{
    system ("mkdir -p tmp");
    setenv("LIBSTORAGE_TESTDIR", "tmp", 1);

    system ("rm -rf tmp/*");

    system ("cp data/disk_hda tmp");
    system ("cp data/disk_hdb tmp");
    system ("cp data/disk_sda tmp");
    system ("cp data/disk_sdb tmp");

    deque<string> devs;

    /*
     * Check that we can extend a volume group ( consisting of one PV
     * /dev/hda1 ) by some partitions on the same ide disk
     */
    devs.push_back("/dev/hda2");
    devs.push_back("/dev/hda5");
    extendVg( "system", devs );
    devs.clear();

    /*
     * Check that we can extend a volume group ( consisting of one PV
     * /dev/hda1 ) by some partitions on the another ide disk
     */
    devs.push_back("/dev/hdb1");
    devs.push_back("/dev/hdb2");
    extendVg( "system", devs );
    devs.clear();

    /*
     * Check that we can extend a volume group ( consisting of one PV
     * /dev/hda1 ) by some partitions on the another scsi disk
     */
    devs.push_back("/dev/sda1");
    devs.push_back("/dev/sdb1");
    extendVg( "system", devs );
    devs.clear();
}
