#include <iostream>
#include <iterator>
#include <sstream>

#include <y2storage/StorageInterface.h>

using namespace storage;
using namespace std;

StorageInterface *s = 0;


void print_disks( const string &container_name )
{
    EvmsCoInfo info;
    ContainerInfo cinfo;

    cout << s->getContEvmsCoInfo( container_name,
				  cinfo,
				  info)
	 << ' ';
    
    cout << info.devices_add << ' ';
    cout << endl;


}

void extendContainer( const string& container_name, deque<string> devs_extend )
{
    printf("extendContainer\n");

    s = createStorageInterface( false, true, false );
    
    deque<string> devs;
    devs.push_back( "/dev/hda1" );
    /* create evms container with the above disk */
    cout << s->createEvmsContainer( container_name,
				    1024,
				    false,
				    devs )
	 << ' ';
    
    print_disks( container_name );
    
    /* extend it by devs_extend given devs */
    cout << s->extendEvmsContainer( container_name,
				    devs_extend ) << ' ';

    print_disks( container_name );

    delete s;
}

void shrinkContainer( const string& container_name, deque<string> devs_shrink )
{
    printf("shrinkContainer\n");

    s = createStorageInterface( false, true, false );
    
    deque<string> devs;
    devs.push_back( "/dev/hda1" );
    devs.push_back( "/dev/hdb1" );
    devs.push_back( "/dev/sda1" );
    /* create evms container with the above disk */
    cout << s->createEvmsContainer( container_name,
				    1024,
				    false,
				    devs )
	 << ' ';
    
    print_disks( container_name );
    
    /* shrink it by devs_shrink given devs */
    cout << s->shrinkEvmsContainer( container_name,
				    devs_shrink ) << ' ';

    print_disks( container_name );

    delete s;
}


int main( int argc_iv, char** argv_ppcv )
{
    system ("mkdir -p tmp");
    setenv ("YAST2_STORAGE_TDIR", "tmp", 1);

    system ("rm -rf tmp/*");

    system ("cp data/disk_hda tmp");
    system ("cp data/disk_hdb tmp");
    system ("cp data/disk_sda tmp");
    system ("cp data/disk_sdb tmp");

    deque<string> devs;

    /*
     * Check that we can extend an evms container (consisting of one
     * partition /dev/hda1) by some partitions on the same ide disk
     */
    devs.push_back("/dev/hda2");
    devs.push_back("/dev/hda5");
    extendContainer( "evms_container", devs );
    devs.clear();

    /*
     * Check that we can extend an evms container (consisting of one
     * partiton /dev/hda1) by some partitions on the another ide disk
     */
    devs.push_back("/dev/hdb1");
    devs.push_back("/dev/hdb2");
    extendContainer( "evms_container", devs );
    devs.clear();

    /*
     * Check that we can extend an evms container (consisting of one
     * partition /dev/hda1) by some partitions on the another scsi disk
     */
    devs.push_back("/dev/sda1");
    devs.push_back("/dev/sdb1");
    extendContainer( "evms_container", devs );
    devs.clear();


    /*
     * Check that we can shrink an evms container (consisting of three
     * partition /dev/hda1, /dev/hdb1 and /dev/sda1) by one partition
     */
    devs.push_back("/dev/sda1");
    shrinkContainer( "evms_container", devs );
    devs.clear();

    /*
     * Check that we can shrink an evms container (consisting of three
     * partition /dev/hda1, /dev/hdb1 and /dev/sda1 ) by two partitions
     */
    devs.push_back("/dev/sda1");
    devs.push_back("/dev/hdb1");
    shrinkContainer( "evms_container", devs );
    devs.clear();

    /*
     * Check that we _cannot_ shrink an evms container (consisting of three
     * partition /dev/hda1, /dev/hdb1 and /dev/sda1 ) by another partition
     */
    devs.push_back("/dev/sdb1");
    shrinkContainer( "evms_container", devs ); // fails
    devs.clear();
}
