#include <dirent.h>
#include <glob.h>

#include <fstream>

#include <ycp/y2log.h>
#include <sys/utsname.h>

#include "y2storage/Storage.h"
#include "y2storage/Disk.h"
#include "y2storage/LvmVg.h"
#include "y2storage/IterPair.h"

struct Larger150 { bool operator()(const Disk&d) const {return(d.cylinders()>150);}};

bool TestLg150( const Disk& d )
    { return(d.cylinders()>150); };
bool TestCG30( const Partition& d )
    { return(d.cylSize()>30); };


Storage::Storage( bool ronly, bool tmode, bool autodetect ) :
    readonly(ronly), testmode(tmode)
    {
    y2milestone( "constructed Storage ronly:%d testmode:%d autodetect:%d",
                 ronly, testmode, autodetect );
    char * tenv = getenv( "YAST_IS_RUNNING" );
    inst_sys = tenv!=NULL && strcmp(tenv,"instsys")==0;
    if( !testmode )
	testmode = getenv( "YAST2_STORAGE_TMODE" )!=NULL;
    if( autodetect && !testmode )
	{
	detectArch();
	autodetectDisks();
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
    y2milestone( "instsys:%d testdir:%s", inst_sys, testdir.c_str() );

    if( testmode )
        {
	glob_t globbuf;

	if( glob( (testdir+"/disk_*").c_str(), GLOB_NOSORT, 0, &globbuf) == 0)
	    {
	    for (char** p = globbuf.gl_pathv; *p != 0; *p++)
		addToList( new Disk( this, *p ) );
	    }
	globfree (&globbuf);
	}
    setCacheChanges( true );
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
Storage::detectArch()
    {
    struct utsname buf;
    proc_arch = "i386";
    if( uname( &buf ) == 0 )
	{
	if( strncmp( buf.machine, "ppc", 3 )==0 )
	    {
	    proc_arch = "ppc";
	    }
	else if( strncmp( buf.machine, "ia64", 4 )==0 )
	    {
	    proc_arch = "ia64";
	    }
	else if( strncmp( buf.machine, "s390", 4 )==0 )
	    {
	    proc_arch = "s390";
	    }
	else if( strncmp( buf.machine, "sparc", 5 )==0 )
	    {
	    proc_arch = "sparc";
	    }
	}
    y2milestone( "Arch:%s", proc_arch.c_str() );
    }

void
Storage::autodetectDisks()
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
		if( d->getSysfsInfo( SysfsDir+"/"+Entry->d_name ) &&
		    d->detectGeometry() && d->detectPartitions() )
		    {
		    d->logData( "/var/log/YaST2" );
		    addToList( d );
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

    addToList( new Container( this, "md", Container::MD ) );
    addToList( new Container( this, "loop", Container::LOOP ) );
    addToList( new LvmVg( this, "system" ) );
    addToList( new LvmVg( this, "vg1" ) );
    addToList( new LvmVg( this, "vg2" ) );
    addToList( new LvmVg( this, "empty" ) );
    addToList( new Evms( this ) );
    addToList( new Evms( this, "vg1" ) );
    addToList( new Evms( this, "vg2" ) );
    addToList( new Evms( this, "empty" ) );
    }

string Storage::proc_arch;


namespace storage
{
    StorageInterface* createStorageInterface (bool ronly, bool testmode, bool autodetect)
    {
	return new Storage (ronly, testmode, autodetect);
    }
}

int 
Storage::createPartition( const string& disk, PartitionType type, unsigned long start,
			  unsigned long long sizeK, string& device )
    {
    int ret = 0;
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	unsigned long num_cyl = i->kbToCylinder( sizeK );
	ret = i->createPartition( type, start, num_cyl, device );
	}
    else
	{
	ret = STORAGE_DISK_NOTFOUND;
	}
    return( ret );
    }



bool
Storage::getDisks (list<string>& disks)
{
    disks.clear ();

    for (ConstDiskIterator i = diskBegin(); i != diskEnd(); ++i)
	disks.push_back (i->name());

    return true;
}


bool
Storage::getPartitions (const string& disk, list<PartitionInfo>& partitioninfos)
{
    partitioninfos.clear ();
    DiskIterator i = findDisk( disk );

    if( i != dEnd() )
    {
	Disk::PartPPair p = i->partPair (Disk::notDeleted);

	for (Disk::PartPIter i2 = p.begin(); i2 != p.end(); ++i2)
	    partitioninfos.push_back (i2->getPartitionInfo());

	return true;
    }

    return( i != dEnd() );
}

Storage::DiskIterator Storage::findDisk( const string& disk )
    {
    DiskIterator ret=dBegin();
    while( ret != dEnd() && ret->name()!=disk )
	ret++;
    return( ret );
    }


bool
Storage::getPartitions (list<PartitionInfo>& partitioninfos)
{
    partitioninfos.clear ();

    ConstPartPair p = partPair(Partition::notDeleted);
    for (ConstPartIterator i = p.begin(); i != p.end(); ++i)
	partitioninfos.push_back (i->getPartitionInfo());

    return true;
}


bool
Storage::getFsCapabilities (FsType fstype, FsCapabilities& fscapabilities)
{
    static FsCapabilities reiserfsCaps = {
	isExtendable: true,
	isExtendableWhileMounted: true,
	isReduceable: true,
	isReduceableWhileMounted: false,
	supportsUuid: true,
	supportsLabel: true,
	labelWhileMounted: false,
	labelLength: 16
    };

    static FsCapabilities ext2Caps = {
	isExtendable: true,
	isExtendableWhileMounted: false,
	isReduceable: true,
	isReduceableWhileMounted: false,
	supportsUuid: true,
	supportsLabel: true,
	labelWhileMounted: true,
	labelLength: 16
    };

    static FsCapabilities ext3Caps = {
	isExtendable: true,
	isExtendableWhileMounted: false,
	isReduceable: true,
	isReduceableWhileMounted: false,
	supportsUuid: true,
	supportsLabel: true,
	labelWhileMounted: true,
	labelLength: 16
    };

    static FsCapabilities xfsCaps = {
	isExtendable: true,
	isExtendableWhileMounted: true,
	isReduceable: false,
	isReduceableWhileMounted: false,
	supportsUuid: true,
	supportsLabel: true,
	labelWhileMounted: false,
	labelLength: 12
    };

    switch (fstype)
    {
	case REISERFS:
	    fscapabilities = reiserfsCaps;
	    return true;

	case EXT2:
	    fscapabilities = ext2Caps;
	    return true;

	case EXT3:
	    fscapabilities = ext3Caps;
	    return true;

	case XFS:
	    fscapabilities = xfsCaps;
	    return true;

	default:
	    return false;
    }
}
