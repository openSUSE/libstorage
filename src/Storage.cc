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


Storage::Storage( bool ronly, bool tmode, bool autodetect ) : 
    readonly(ronly), testmode(tmode)
    {
    y2milestone( "constructed Storage ronly:%d testmode:%d autodetec:%d", 
                 ronly, testmode, autodetect );
    if( autodetect && !testmode )
	{
	AutodetectDisks();
	}
    char * tenv = getenv( "YAST_IS_RUNNING" );
    instsys = tenv!=NULL && strcmp(tenv,"instsys")==0;
    if( testmode )
	{
	tenv = getenv( "YAST2_STORAGE_TDIR" );
	if( tenv!=NULL && strlen(tenv)>0 )
	    {
	    testdir = tenv;
	    }
	else
	    {
	    testdir = "/var/lib/YaST2/storage_test";
	    }
	}
    y2milestone( "instsys:%d testdir:%s", instsys, testdir.c_str() );

    if( testmode )
	{
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
    for( CIter i=cont.begin(); i!=cont.end(); i++ )
	{
	delete( *i );
	}
    y2milestone( "destructed Storage" );
    }

void
Storage::AutodetectDisks()
    {
    AddToList( new Disk( this, "hdc" ));
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
		AddToList( new Disk( this, Entry->d_name ) );
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


