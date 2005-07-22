#include <iostream>
#include <iterator>
#include <sstream>

#include <y2storage/StorageInterface.h>

using namespace storage;
using namespace std;

StorageInterface *s = 0;

void print_container_info( const string &name )
{
    EvmsCoInfo info;
    ContainerInfo cinfo;

    int ret =  s->getContEvmsCoInfo( name,
				     cinfo,
				     info);

    cout << ret  << ' ';

    if ( ret != 0 )
    {
	cout << endl;
	return;
    }

    cout << cinfo.type << ' ';
    cout << ' ' << cinfo.volcnt << ' ';
    cout << ' ' << cinfo.device << ' ';
    cout << ' ' << cinfo.name << ' ';
    cout << endl;

}

void print_evms_info( const string &name )
{
    deque<EvmsInfo> plist;

    cout << s->getEvmsInfo( name, plist ) << ' ';
    
    for ( deque<EvmsInfo>::iterator p = plist.begin();
	  p != plist.end(); ++p )
    {
	cout << p->v.name << ' ';
	cout << p->v.device << ' ';
	cout << p->dm_table << ' ';
	cout << p->compatible << ' ';
	cout << p->stripe << ' ';
	cout << endl;
    }
}

void createContainer( const string &name, deque<string> devs )
{
    s = createStorageInterface( false, true, false );

    cout << "create container: ";

    cout << s->createEvmsContainer( name,
				    1024,
				    false,
				    devs )
	 << ' ';

    print_container_info( name );

    delete s;

}

void removeContainer() {
    s = createStorageInterface( false, true, false );

    cout << "remove container: ";

    deque<string> devs;
    devs.push_back( "/dev/hda1" );
    devs.push_back( "/dev/hdb1" );
    devs.push_back( "/dev/sda1" );

    string container_name = "evms_container";

    cout <<  s->createEvmsContainer( container_name,
				     1024,
				     false,
				     devs )
	 << ' ';

    cout << s->removeEvmsContainer( container_name ) << ' ';
    
    // this failes because the container is not there anymore
    print_container_info( container_name );

    delete s;
}

void createVolumes()
{
    s = createStorageInterface( false, true, false );

    cout << "create volumes: ";

    string container_name = "evms_container";

    deque<string> devs;
    devs.push_back( "/dev/hda1" );
    devs.push_back( "/dev/hdb1" );
    devs.push_back( "/dev/sda1" );

    cout <<  s->createEvmsContainer( container_name,
				     1024,
				     false,
				     devs )
	 << ' ';
    
    string device = "mydevice";
    

    for ( int i = 0; i < 10; i++ )
    {
	ostringstream vol;
	vol << "volume_";
	vol << i;

	cout << s->createEvmsVolume( container_name,
				     vol.str(),
				     1000,
				     1,
				     device )
	     << ' ';
    }
    
    print_evms_info( "evms_container" );

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


    deque<string> devs;

    /*
     * Check that we can create an evms container with an ide disk
     */
    devs.push_back( "/dev/hda1" );
    createContainer( "evms_container", devs); 

    /*
     * Check that we can create an evms container with an additional ide
     * disk
     */
    devs.push_back( "/dev/hdb1" );
    createContainer( "evms_container", devs); 

    /*
     * Check that we can create an evms container with an additional scsi
     * disk
     */
    devs.push_back( "/dev/sda1" );
    createContainer( "evms_container", devs); 

    /*
     * Check that we can create and add volumes to an evms container
     */
    createVolumes();

    /*
     * Check that we can remove a container
     */
    removeContainer();
}
