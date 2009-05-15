
#include <stdlib.h>
#include <iostream>
#include <iterator>

#include "common.h"


using namespace storage;
using namespace std;


StorageInterface *s = 0;


void print_md_info()
{
    deque<MdInfo> plist;

    s->getMdInfo( plist );

    for ( deque<MdInfo>::iterator p = plist.begin();
	  p != plist.end(); ++p )
    {
	cout << ' ' << p->v.name << ' ';
	cout << p->v.device << ' ';
	cout << p->nr << ' ';
	cout << p->type << ' ';
	cout << p->chunk << ' ';
    }

}

void createMD( MdType type, deque<string> devs )
{
    s = createStorageInterface(TestEnvironment());

    int ret = s->createMd( "/dev/md0", type, devs );
    if( ret==0 )
	ret = s->checkMd( "/dev/md0" );

    cout << "createMD: " << ret;
    if(ret==0)
	print_md_info();
    cout << endl;

    delete s;
}


int main( int argc_iv, char** argv_ppcv )
{
    system ("mkdir -p tmp");
    setenv ("YAST2_STORAGE_TDIR", "tmp", 1);

    system ("rm -rf tmp/*");
    
    deque<string> devs;

    system ("cp data/disk_hda tmp");
    system ("cp data/disk_hdb tmp");
    system ("cp data/disk_sda tmp");

    /*
     * Check that we _cannot_ create software raid devices with just one
     * partition
     */
    devs.push_back( "/dev/hda1" );
    cout << "Devices: " << devs.size() << " ----------------------\n";
    createMD(RAID0, devs); // fails, too few devices
    createMD(RAID1, devs); // fails, too few devices
    createMD(RAID5, devs); // fails, too few devices
    createMD(RAID6, devs); // fails, too few devices
    createMD(RAID10,devs); // fails, too few devices
    createMD(MULTIPATH, devs); // fails, too few devices
    createMD(RAID_UNK, devs); // RAID_UNK always fails

    /*
     * Check that this works for some raid levels with two devices
     */
    devs.push_back( "/dev/hda2" );
    cout << "Devices: " << devs.size() << " ----------------------\n";
    createMD(RAID0, devs); // works now
    createMD(RAID1, devs); // works now
    createMD(RAID5, devs); // fails, too few devices
    createMD(RAID6, devs); // fails, too few devices
    createMD(RAID10,devs); // works now
    createMD(MULTIPATH, devs); // works now
    createMD(RAID_UNK, devs); // RAID_UNK always fails

    /*
     * RAID6 still fails
     */
    devs.push_back( "/dev/hda3" );
    cout << "Devices: " << devs.size() << " ----------------------\n";
    createMD(RAID5, devs); // works now
    createMD(RAID6, devs); // fails, too few devices

    /* 
     * Everything works now
     */
    devs.push_back( "/dev/hda3" );
    cout << "Devices: " << devs.size() << " ----------------------\n";
    createMD(RAID6, devs); // works now

    /*
     * Check that we can create a software raid device with partitions
     * from different devices
     */
    devs.push_back( "/dev/hdb1" );
    cout << "Devices: " << devs.size() << " ----------------------\n";
    createMD(RAID0, devs);
    createMD(RAID1, devs);
    createMD(RAID5, devs);
    createMD(RAID6, devs);
    createMD(RAID10,devs);
    createMD(MULTIPATH, devs);


    /*
     * Check that we can create a software raid device with partitions from a scsi disk
     */
    devs.push_back( "/dev/sda1" );
    cout << "Devices: " << devs.size() << " ----------------------\n";
    createMD(RAID0, devs);
    createMD(RAID1, devs);
    createMD(RAID5, devs);
    createMD(RAID6, devs);
    createMD(RAID10,devs);
    createMD(MULTIPATH, devs);
}
