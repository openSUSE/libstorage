#include <dirent.h>

#include <fstream>

#include <ycp/y2log.h>

#include "y2storage/Storage.h"
#include "y2storage/Disk.h"

    struct Larger150 { bool operator()(const Disk&d) const {return(d.Cylinders()>150);}};

Storage::Storage( bool ronly, bool autodetect ) : readonly(ronly)
    {
    y2milestone( "constructed Storage ronly:%d autodetec:%d", ronly, autodetect );
    if( autodetect )
	{
	AutodetectDisks();
	}
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
    }

int 
Storage::AddDisk( const string& Name )
    {
    y2milestone( "Name: %s", Name.c_str() );
    Disks.push_front( new Disk( Name ) );
    return( 0 );
    }

