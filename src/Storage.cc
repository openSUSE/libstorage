#include <dirent.h>

#include <fstream>

#include <ycp/y2log.h>
#include <sys/utsname.h>

#include "y2storage/Storage.h"
#include "y2storage/Disk.h"
#include "y2storage/LvmVg.h"
#include "y2storage/IterPair.h"

struct Larger150 { bool operator()(const Disk&d) const {return(d.Cylinders()>150);}};

bool TestLg150( const Disk& d )
    { return(d.Cylinders()>150); };
bool TestCG30( const Partition& d )
    { return(d.CylSize()>30); };


Storage::Storage( bool ronly, bool tmode, bool autodetect ) : 
    readonly(ronly), testmode(tmode)
    {
    y2milestone( "constructed Storage ronly:%d testmode:%d autodetect:%d", 
                 ronly, testmode, autodetect );
    char * tenv = getenv( "YAST_IS_RUNNING" );
    instsys = tenv!=NULL && strcmp(tenv,"instsys")==0;
    if( !testmode )
	testmode = getenv( "YAST2_STORAGE_TMODE" )!=NULL;
    if( autodetect && !testmode )
	{
	DetectArch();
	AutodetectDisks();
	}
    if( testmode )
	{
	tenv = getenv( "YAST2_STORAGE_TDIR" );
	if( tenv!=NULL && strlen(tenv)>0 )
	    {
	    testdir = tenv;
	    }
	else
	    {
	    testdir = "/var/log/YaST2";
	    }
	}
    y2milestone( "instsys:%d testdir:%s", instsys, testdir.c_str() );

    if( testmode )
	{
	DIR *Dir;
	struct dirent *Entry;
	if( (Dir=opendir( testdir.c_str() ))!=NULL )
	    {
	    while( (Entry=readdir( Dir ))!=NULL )
		{
		if( strncmp( Entry->d_name, "disk_", 5 )==0 )
		    {
		    Disk * d = new Disk( this, testdir+"/"+Entry->d_name );
		    AddToList( d );
		    }
		}
	    }
	}
    }

Storage::~Storage()
    {
    for( CIter i=cont.begin(); i!=cont.end(); i++ )
	{
	delete( *i );
	}
    y2milestone( "destructed Storage" );
    }

void
Storage::DetectArch()
    {
    struct utsname buf;
    arch = "i386";
    if( uname( &buf ) == 0 )
	{
	if( strncmp( buf.machine, "ppc", 3 )==0 )
	    {
	    arch = "ppc";
	    }
	else if( strncmp( buf.machine, "ia64", 4 )==0 )
	    {
	    arch = "ia64";
	    }
	else if( strncmp( buf.machine, "s390", 4 )==0 )
	    {
	    arch = "s390";
	    }
	else if( strncmp( buf.machine, "sparc", 5 )==0 )
	    {
	    arch = "sparc";
	    }
	}
    y2milestone( "Arch:%s", arch.c_str() );
    }

void
Storage::AutodetectDisks()
    {
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
		Disk * d = new Disk( this, Entry->d_name, Size/2 );
		if( d->GetSysfsInfo( SysfsDir+"/"+Entry->d_name ) &&
		    d->DetectGeometry() && d->DetectPartitions() )
		    {
		    d->LogData( "/var/log/YaST2" );
		    AddToList( d );
		    }
		else
		    {
		    delete d;
		    }
		}
	    }
	closedir( Dir );
	}
    else
	{
	y2error( "Failed to open:%s", SysfsDir.c_str() );
	}
    AddToList( new Container( this, "md", Container::MD ) );
    AddToList( new Container( this, "loop", Container::LOOP ) );
    AddToList( new LvmVg( this, "system" ) );
    AddToList( new LvmVg( this, "vg1" ) );
    AddToList( new LvmVg( this, "vg2" ) );
    AddToList( new LvmVg( this, "empty" ) );
    AddToList( new Evms( this ) );
    AddToList( new Evms( this, "vg1" ) );
    AddToList( new Evms( this, "vg2" ) );
    AddToList( new Evms( this, "empty" ) );
    }

string Storage::arch;


