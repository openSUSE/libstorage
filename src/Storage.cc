#include <dirent.h>
#include <glob.h>

#include <fstream>

#include <ycp/y2log.h>
#include <sys/utsname.h>

#include "y2storage/Storage.h"
#include "y2storage/SystemCmd.h"
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

	if( glob( (testdir+"/disk_*[!~]").c_str(), GLOB_NOSORT, 0, &globbuf) == 0)
	    {
	    for (char** p = globbuf.gl_pathv; *p != 0; *p++)
		addToList( new Disk( this, *p ) );
	    }
	globfree (&globbuf);
	}
    else
	detectFsData( vBegin(), vEnd() );
    setCacheChanges( true );
    }

Storage::~Storage()
    {
    for( CIter i=cont.begin(); i!=cont.end(); ++i )
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
    }

void
Storage::detectFsData( const VolIterator& begin, const VolIterator& end )
    {
    y2milestone( "detectFsData begin" );
    SystemCmd Blkid( "/sbin/blkid" );
    for( VolIterator i=begin; i!=end; ++i )
	{
	i->getFsData( Blkid );
	}
    y2milestone( "detectFsData end" );
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
			  unsigned long size, string& device )
    {
    int ret = 0;
    y2milestone( "disk:%s type:%d start:%ld size:%ld", disk.c_str(),
                 type, start, size );
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	ret = i->createPartition( type, start, size, device );
	}
    else
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d device:%s", ret, ret?device.c_str():"" );
    return( ret );
    }

int
Storage::createPartitionKb( const string& disk, PartitionType type, 
                            unsigned long long start,
			    unsigned long long sizeK, string& device )
    {
    int ret = 0;
    y2milestone( "disk:%s type:%d start:%lld sizeK:%lld", disk.c_str(),
                 type, start, sizeK );
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	unsigned long num_cyl = i->kbToCylinder( sizeK );
	unsigned long long tmp_start = start;
	if( tmp_start > i->kbToCylinder(1)/2 )
	    tmp_start -= i->kbToCylinder(1)/2;
	else 
	    tmp_start = 0;
	unsigned long start_cyl = i->kbToCylinder( tmp_start )+1;
	ret = i->createPartition( type, start_cyl, num_cyl, device, true );
	}
    else
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    y2milestone( "ret:%d device:%s", ret, ret?device.c_str():"" );
    return( ret );
    }

unsigned long long
Storage::cylinderToKb( const string& disk, unsigned long size )
    {
    unsigned long long ret = 0;
    y2milestone( "disk:%s size:%ld", disk.c_str(), size );
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	ret = i->cylinderToKb( size );
	}
    y2milestone( "ret:%lld", ret );
    return( ret );
    }

unsigned long
Storage::kbToCylinder( const string& disk, unsigned long long sizeK )
    {
    unsigned long ret = 0;
    y2milestone( "disk:%s sizeK:%lld", disk.c_str(), sizeK );
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	ret = i->kbToCylinder( sizeK );
	}
    y2milestone( "ret:%ld", ret );
    return( ret );
    }

int
Storage::removePartition( const string& partition )
    {
    int ret = 0;
    y2milestone( "partition:%s", partition.c_str() );
    VolIterator vol;
    ContIterator cont;
    if( findVolume( partition, cont, vol ) && cont->type()==Container::DISK )
	{
	Disk* disk = dynamic_cast<Disk *>(&(*cont));
	if( disk!=NULL )
	    {
	    ret = disk->removePartition( vol->nr() );
	    }
	else
	    {
	    ret = STORAGE_REMOVE_PARTITION_INVALID_CONTAINER;
	    }
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::changePartitionId( const string& partition, unsigned id )
    {
    int ret = 0;
    y2milestone( "partition:%s id:%x", partition.c_str(), id );
    VolIterator vol;
    ContIterator cont;
    if( findVolume( partition, cont, vol ) && cont->type()==Container::DISK )
	{
	Disk* disk = dynamic_cast<Disk *>(&(*cont));
	if( disk!=NULL )
	    {
	    ret = disk->changePartitionId( vol->nr(), id );
	    }
	else
	    {
	    ret = STORAGE_CHANGE_PARTITION_ID_INVALID_CONTAINER;
	    }
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::destroyPartitionTable( const string& disk, const string& label )
    {
    int ret = 0;
    y2milestone( "disk:%s", disk.c_str() );
    DiskIterator i = findDisk( disk );

    if( i != dEnd() )
	{
	ret = i->destroyPartitionTable( label );
	}
    else
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string 
Storage::defaultDiskLabel()
    {
    return( Disk::defaultLabel() );
    }

int Storage::checkCache()
    {
    int ret=0; 
    if( !cacheChanges() ) 
	ret = commit(); 
    return(ret);
    }

int Storage::commit()
    {
    struct tmp
	{ static bool TestHdb( const Container& c )
	    {
	    y2milestone( "name:%s", c.name().c_str() );
	    return( c.name().find("hdb")!=string::npos ); }};
    CPair p = cPair( tmp::TestHdb );
    int ret = 0;
    y2milestone( "empty:%d", p.empty() );
    if( !p.empty() )
	{
	Container::CommitStage a[] = { Container::DECREASE, Container::INCREASE,
	                               Container::FORMAT, Container::MOUNT };
	Container::CommitStage* pt = a;
	while( unsigned(pt-a) < sizeof(a)/sizeof(a[0]) )
	    {
	    int t = p.begin()->commitChanges( *pt );
	    y2milestone( "%d ret %d", *pt, ret );
	    if( ret==0 && t!=0 )
		ret = t;
	    pt++;
	    }
	}
    y2milestone( "ret:%d", ret );
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
	Disk::PartPair p = i->partPair (Disk::notDeleted);

	for (Disk::PartIter i2 = p.begin(); i2 != p.end(); ++i2)
	    partitioninfos.push_back (i2->getPartitionInfo());
    }

    return( i != dEnd() );
}

bool Storage::findVolume( const string& device, ContIterator& c,
                          VolIterator& v )
    {
    bool ret = false;
    VPair p = vPair( Volume::notDeleted, notDeleted );
    v = p.begin();
    while( v!=p.end() && v->device()!=device )
	++v;
    if( v!=p.end() )
	{
	const Container *co = v->getContainer();
	CPair cp = cPair( notDeleted );
	c = cp.begin();
	while( c!=cp.end() && &(*c)!=co )
	    ++c;
	ret = c!=cp.end();
	}
    y2milestone( "device:%s ret:%d c->device:%s v->device:%s", device.c_str(),
                 ret, ret?c->device().c_str():"nil", 
		 ret?v->device().c_str():"nil" );
    return( ret );
    }

Storage::DiskIterator Storage::findDisk( const string& disk )
    {
    DiskIterator ret=dBegin();
    while( ret != dEnd() && ret->device()!=disk )
	++ret;
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
