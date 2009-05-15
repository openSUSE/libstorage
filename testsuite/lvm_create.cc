
#include <stdlib.h>
#include <iostream>
#include <iterator>
#include <sstream>

#include "common.h"


using namespace storage;
using namespace std;


StorageInterface *s = 0;


void print_num_lvs(const string& disk, const string& vg)
{
    deque<LvmLvInfo> plist;
    s->getLvmLvInfo( vg, plist );

    cout << plist.size() << endl;
}

/* 
 * get all partitions on disks disks which could be added to a
 * logical volume group ( AFAIK --> PRIMARY, LOGICAL )
 */
deque<string> getDevs( deque<string> disks )
{
    StorageInterface *siface = createStorageInterface(TestEnvironment());

    deque<string> devs;
    deque<PartitionInfo> pInfos;
    
    for ( deque<string>::iterator i = disks.begin();
	  i != disks.end(); i++)
    {
	cout << siface->getPartitionInfo( *i, pInfos ) << endl;

	for (deque<PartitionInfo>::iterator f = pInfos.begin ();
	     f != pInfos.end(); f++)
	{
	    switch (f->partitionType)
	    {
	    case PRIMARY:
		devs.push_back(f->v.name);
		break;
	    case LOGICAL:
		devs.push_back(f->v.name);
		break;
	    default:
		break;
	    }
	}
	
    }
    
    delete siface;

    return devs;
}

void createLvs( const string& vg, int n, deque<string>& disks )
{
    printf("createLvs\n");

    s = createStorageInterface(TestEnvironment());

    /* create volume group with the above disks */
    cout << s->createLvmVg( vg, 4, false, getDevs(disks) ) << endl;

    /* create n logical volumes */
    int ret = 0;
    for ( int i = 0; i < n; i++ ) 
    {
	ostringstream name;
	name << "volume";
	name << i;

	string dev;
	ret = s->createLvmLv( vg, name.str(), 100, 1, dev );
    }
    cout << ret << endl;
    

    for ( deque<string>::iterator i = disks.begin();
	  i != disks.end(); i++ )
    {
	print_num_lvs( *i, vg);
	print_num_lvs( *i, vg);
    }

    delete s;
}


void createExtendedLv( const string& vg, const string& dev )
{
    printf("createExtendedLv\n");

    s = createStorageInterface(TestEnvironment());

    deque<string> devs;
    devs.push_back(dev); // add extended partition
    
    /* create volume group with the extended partition */
    cout << s->createLvmVg( vg, 4, false, devs ) << endl; // FAILS

    delete s;
}


int main( int argc_iv, char** argv_ppcv )
{
    system ("mkdir -p tmp");
    setenv ("YAST2_STORAGE_TDIR", "tmp", 1);

    system ("rm -rf tmp/*");

    deque<string> disks;

    /*
     * Check that we can create a volume group and 50 logical volumes on
     * ide disks.
     */
    disks.push_back( "/dev/hda" );
    disks.push_back( "/dev/hdb" );
    system ("cp data/disk_hda tmp");
    system ("cp data/disk_hdb tmp");
    createLvs( "system", 50, disks );
    disks.clear();

    /*
     * Check that we can create a volume group and 50 logical volumes on
     * scsi disks.
     */
    disks.push_back( "/dev/sda" );
    disks.push_back( "/dev/sdb" );
    system ("cp data/disk_sda tmp");
    system ("cp data/disk_sdb tmp");
    createLvs( "system", 50, disks );
    disks.clear();

    /*
     * Check that we can create a volume group and 100 logical volumes on
     * ide and scsi disks.
     */
    disks.push_back( "/dev/sda" );
    disks.push_back( "/dev/sdb" );
    disks.push_back( "/dev/hda" );
    disks.push_back( "/dev/hdb" );
    system ("cp data/disk_sda tmp");
    system ("cp data/disk_sdb tmp");
    system ("cp data/disk_hda tmp");
    system ("cp data/disk_hdb tmp");
    createLvs( "system", 50, disks );
    disks.clear();

    /*
     * Check that we _cannot_ create a volume group out of an extended
     * partition
     */
    system ("cp data/disk_hda tmp");
    createExtendedLv("system", "/dev/hda4");
}
