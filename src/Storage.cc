#include <dirent.h>

#include <fstream>

#include <ycp/y2log.h>

#include "y2storage/Storage.h"
#include "y2storage/Disk.h"
#include "y2storage/LvmVg.h"
#include "y2storage/IterPair.h"

struct Larger150 { bool operator()(const Disk&d) const {return(d.Cylinders()>150);}};

bool TestLg150( const Disk& d )
    { return(d.Cylinders()>150); };
bool TestCG30( const Partition& d )
    { return(d.CylSize()>30); };


Storage::Storage( bool ronly, bool autodetect ) : readonly(ronly)
    {
    y2milestone( "constructed Storage ronly:%d autodetec:%d", ronly, autodetect );
    if( autodetect )
	{
	AutodetectDisks();
	}
    /*
    ConstPartInter b(DiskPair( TestLg150 ));
    ConstPartInter e(DiskPair( TestLg150 ), true);
    IterPair<ConstPartInter> p( b, e );

    IterPair<ConstPartInter2> q = IterPair<ConstPartInter2>( ConstPartInter2( b ), 
                                                             ConstPartInter2( e ) );
    IterPair<ConstPartIterator> r((ConstPartIterator(ConstPartPIterator( q, TestCG30 ))), 
	                          (ConstPartIterator(ConstPartPIterator( q, TestCG30, true))));
    for( ConstPartIterator i=r.begin(); i!=r.end(); ++i )
	{
	cout << "Name:" << i->Device() << " Start:" << (*i).CylStart()
	     << " Cyl:" << (*i).CylSize() << endl;
	}
    */
    }

Storage::~Storage()
    {
    y2milestone( "destructed Storage" );
    }

void
Storage::AutodetectDisks()
    {
    AddDisk( "hdc" );
    string SysfsDir = "/sys/block";
    DIR *Dir;
    struct dirent *Entry;
    if( (Dir=opendir( SysfsDir.c_str() ))!=NULL )
	{
	while( (Entry=readdir( Dir ))!=NULL )
	    {
	    int Range=0;
	    unsigned long long Size = 0;
	    string SysfsFile = SysfsDir+"/"+Entry->d_name+"/range";
	    if( access( SysfsFile.c_str(), R_OK )==0 )
		{
		ifstream File( SysfsFile.c_str() );
		File >> Range;
		}
	    SysfsFile = SysfsDir+"/"+Entry->d_name+"/size";
	    if( access( SysfsFile.c_str(), R_OK )==0 )
		{
		ifstream File( SysfsFile.c_str() );
		File >> Size;
		}
	    if( Range>1 && Size>0 )
		{
		AddDisk( Entry->d_name );
		}
	    }
	closedir( Dir );
	}
    else
	{
	y2error( "Failed to open:%s", SysfsDir.c_str() );
	}
    Disks.push_back( new Container( "md", Container::MD ) );
    Disks.push_back( new Container( "loop", Container::LOOP ) );
    Disks.push_back( new LvmVg( "system" ) );
    Disks.push_back( new LvmVg( "vg1" ) );
    Disks.push_back( new LvmVg( "vg2" ) );
    Disks.push_back( new LvmVg( "empty" ) );
    }

int 
Storage::AddDisk( const string& Name )
    {
    y2milestone( "Name: %s", Name.c_str() );
    Disks.push_front( new Disk( Name ) );
    return( 0 );
    }

