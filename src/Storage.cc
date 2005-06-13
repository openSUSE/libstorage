/*
  Textdomain    "storage"
*/

#include <dirent.h>
#include <glob.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <fstream>
#include <sstream>
#include <iostream>

#include <sys/utsname.h>

#include <blocxx/AppenderLogger.hpp>
#include <blocxx/FileAppender.hpp>

#include "y2storage/Storage.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/Disk.h"
#include "y2storage/MdCo.h"
#include "y2storage/DmCo.h"
#include "y2storage/LoopCo.h"
#include "y2storage/LvmVg.h"
#include "y2storage/IterPair.h"
#include "y2storage/ProcMounts.h"
#include "y2storage/EtcFstab.h"
#include "y2storage/AsciiFile.h"

using namespace std;
using namespace storage;


void
Storage::initDefaultLogger ()
{
    using namespace blocxx;

    String name = "testlog";
    LoggerConfigMap configItems;
    String StrKey;
    String StrPath;
    StrKey.format("log.%s.location", name.c_str());

    if (geteuid ())
    {
	struct passwd* pw = getpwuid (geteuid ());
	if (pw)
	{
	    configItems[StrKey] = pw->pw_dir;
	    configItems[StrKey] += "/.y2log";
	}
	else
	{
	    configItems[StrKey] = "/y2log";
	}
    }
    else
    {
	configItems[StrKey] = "/var/log/YaST2/y2log";
    }

    LogAppenderRef logApp =
	LogAppender::createLogAppender( name, LogAppender::ALL_COMPONENTS,
					LogAppender::ALL_CATEGORIES,
					"%d %-5p %c - %m",
					LogAppender::TYPE_FILE,
					configItems );
    LoggerRef log( new AppenderLogger("libstorage", E_INFO_LEVEL, logApp));
    Logger::setDefaultLogger(log);
}


Storage::Storage( bool ronly, bool tmode, bool autodetec ) :
    readonly(ronly), testmode(tmode), initialized(false), autodetect(autodetec)
    {
    y2milestone( "constructed Storage ronly:%d testmode:%d autodetect:%d",
                 ronly, testmode, autodetect );
    char * tenv = getenv( "YAST_IS_RUNNING" );
    inst_sys = tenv!=NULL && strcmp(tenv,"instsys")==0;
    root_mounted = !inst_sys;
    if( !testmode )
	testmode = getenv( "YAST2_STORAGE_TMODE" )!=NULL;
    max_log_num = 5;
    tenv = getenv( "Y2MAXLOGNUM" );
    logdir = "/var/log/YaST2";
    if( tenv!=0 )
	string(tenv) >> max_log_num;
    y2milestone( "instsys:%d testmode:%d autodetect:%d log:%d", inst_sys,
                 testmode, autodetect, max_log_num );
    progress_bar_cb = NULL;
    install_info_cb = NULL;
    info_popup_cb = NULL;
    yesno_popup_cb = NULL;
    recursiveRemove = false;
    zeroNewPartitions = false;
    }

void
Storage::initialize()
    {
    initialized = true;
    char tbuf[100];
    strncpy( tbuf, "/tmp/liby2storageXXXXXX", sizeof(tbuf)-1 );
    if( mkdtemp( tbuf )==NULL )
	{
	cerr << "tmpdir creation " << tbuf << " failed. Aborting..." << endl;
	exit(1);
	}
    else
	tempdir = tbuf;
    if( autodetect )
	{
	detectArch();
	}
    if( testmode )
	{
	char * tenv = getenv( "YAST2_STORAGE_TDIR" );
	if( tenv!=NULL && strlen(tenv)>0 )
	    {
	    logdir = testdir = tenv;
	    }
	else
	    {
	    testdir = logdir;
	    }
	}
    y2milestone( "instsys:%d testdir:%s", inst_sys, testdir.c_str() );

    EvmsCo::activate(true);
    detectDisks();
    if( instsys() )
	MdCo::activate( true );
    detectMds();
    detectLvmVgs();
    detectEvms();
    detectDm();
    if( instsys() )
	MdCo::activate( false );

    if( testmode )
        {
 	system_cmd_testmode = true;
 	rootprefix = testdir;
 	fstab = new EtcFstab( rootprefix );
	string t = testdir+"/volume_info";
	if( access( t.c_str(), R_OK )==0 )
	    {
	    detectFsDataTestMode( t, vBegin(), vEnd() );
	    }
	}
    else
	{
	fstab = new EtcFstab( "/etc" );
	detectLoops();
	detectFsData( vBegin(), vEnd() );
	}
    setCacheChanges( true );
    }

Storage::~Storage()
    {
    if( max_log_num>0 )
	logVolumes( logdir );
    for( CIter i=cont.begin(); i!=cont.end(); ++i )
	{
	if( max_log_num>0 )
	    (*i)->logData( logdir );
	delete( *i );
	}
    if( !tempdir.empty() && access( tempdir.c_str(), R_OK )==0 )
	{
	SystemCmd c( "rmdir " + tempdir );
	if( c.retcode()!=0 )
	    {
	    y2error( "stray tmpfile" );
	    c.execute( "ls " + tempdir );
	    c.execute( "rm -rf " + tempdir );
	    }
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
Storage::detectDisks()
    {
    if( test() )
	{
	glob_t globbuf;

	if( glob( (testdir+"/disk_*[!~0-9]").c_str(), GLOB_NOSORT, 0,
	          &globbuf) == 0)
	    {
	    for (char** p = globbuf.gl_pathv; *p != 0; *p++)
		addToList( new Disk( this, *p ) );
	    }
 	globfree (&globbuf);
	}
    else if( autodetect )
	{
	autodetectDisks();
	}
    }

void Storage::detectMds()
    {
    if( test() )
	{
	string file = testdir+"/md";
	if( access( file.c_str(), R_OK )==0 )
	    {
	    addToList( new MdCo( this, file ) );
	    }
	}
    else
	{
	MdCo * v = new MdCo( this, true );
	if( v->numVolumes()>0 )
	    addToList( v );
	else
	    delete v;
	}
    }

void Storage::detectLoops()
    {
    if( test() )
	{
	string file = testdir+"/loop";
	if( access( file.c_str(), R_OK )==0 )
	    {
	    addToList( new LoopCo( this, file ) );
	    }
	}
    else
	{
	LoopCo * v = new LoopCo( this, true );
	if( v->numVolumes()>0 )
	    addToList( v );
	else
	    delete v;
	}
    }

void
Storage::detectLvmVgs()
    {
    if( test() )
	{
	glob_t globbuf;
	if( glob( (testdir+"/lvm_*[!~0-9]").c_str(), GLOB_NOSORT, 0,
	          &globbuf) == 0)
	    {
	    for (char** p = globbuf.gl_pathv; *p != 0; *p++)
		addToList( new LvmVg( this, *p, true ) );
	    }
 	globfree (&globbuf);
	}
    else
	{
	list<string> l;
	if( instsys() )
	    LvmVg::activate( true );
	LvmVg::getVgs( l );
	for( list<string>::const_iterator i=l.begin(); i!=l.end(); ++i )
	    {
	    LvmVg * v = new LvmVg( this, *i );
	    addToList( v );
	    v->checkConsistency();
	    }
	if( instsys() )
	    LvmVg::activate( false );
	}
    }

void
Storage::detectEvms()
    {
    if( test() )
	{
	glob_t globbuf;
	if( glob( (testdir+"/evms_*[!~0-9]").c_str(), GLOB_NOSORT, 0,
	          &globbuf) == 0)
	    {
	    // TODO
	    }
 	globfree (&globbuf);
	}
    else
	{
	EvmsCo::activate( true );
	EvmsTree data;
	EvmsCo::getEvmsList( data );
	if( !data.cont.empty() || !data.volumes.empty() )
	    {
	    if( !data.cont.empty() )
		{
		EvmsCo * e = new EvmsCo( this, data );
		addToList( e );
		}
	    for( list<EvmsCont>::const_iterator i=data.cont.begin(); 
	         i!=data.cont.end(); ++i )
		{
		EvmsCo * e = new EvmsCo( this, *i, data );
		addToList( e );
		e->checkConsistency();
		}
	    }
	if( (data.cont.empty()&&data.volumes.empty()) || instsys() )
	    EvmsCo::activate( false );
	}
    }

void
Storage::detectDm()
    {
    if( test() )
	{
	glob_t globbuf;
	if( glob( (testdir+"/dm_*[!~0-9]").c_str(), GLOB_NOSORT, 0,
	          &globbuf) == 0)
	    {
	    // TODO
	    }
 	globfree (&globbuf);
	}
    else if( getenv( "YAST2_STORAGE_NO_DM" )==NULL )
	{
	DmCo * v = new DmCo( this, true );
	if( v->numVolumes()>0 )
	    addToList( v );
	else
	    delete v;
	}
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
		    if( max_log_num>0 )
			d->logData( logdir );
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
    SystemCmd Blkid( "/sbin/blkid -c /dev/null" );
    SystemCmd Losetup( "/sbin/losetup -a" );
    ProcMounts Mounts;
    for( VolIterator i=begin; i!=end; ++i )
	{
	i->getLoopData( Losetup );
	i->getFsData( Blkid );
	i->getMountData( Mounts );
	i->getFstabData( *fstab );
	}
    if( max_log_num>0 )
	logVolumes( logdir );
    y2milestone( "detectFsData end" );
    }

void
Storage::printInfo( ostream& str )
    {
    assertInit();
    ConstContPair p = contPair();
    for( ConstContIterator i=p.begin(); i!=p.end(); ++i )
	{
	Container::ConstVolPair vp = i->volPair();
	i->print( str );
	str << endl;
	for( Container::ConstVolIterator i=vp.begin(); i!=vp.end(); ++i )
	    {
	    i->print( str );
	    str << endl;
	    }
	}
    }

void
Storage::detectFsDataTestMode( const string& file, const VolIterator& begin,
			       const VolIterator& end )
    {
    AsciiFile vd( file );
    for( VolIterator i=begin; i!=end; ++i )
	{
	int pos = -1;
	if( (pos=vd.find( 0, "^"+i->device()+" " ))>=0 )
	    {
	    i->getTestmodeData( vd[pos] );
	    }
	i->getFstabData( *fstab );
	}
    }

void
Storage::logVolumes( const string& Dir )
    {
    string fname( Dir + "/volume_info.tmp" );
    ofstream file( fname.c_str() );
    for( VolIterator i=vBegin(); i!=vEnd(); ++i )
	{
	if( i->getFs()!=FSUNKNOWN )
	    {
	    i->logVolume( file );
	    }
	}
    file.close();
    handleLogFile( fname );
    }

bool
Storage::testFilesEqual( const string& n1, const string& n2 )
    {
    bool ret = false;
    if( access( n1.c_str(), R_OK )==0 && access( n2.c_str(), R_OK )==0 )
	{
	SystemCmd c( "md5sum " + n1 + " " + n2 );
	if( c.retcode()==0 && c.numLines()>=2 )
	    {
	    ret = extractNthWord( 0, *c.getLine(0) )==
	          extractNthWord( 0, *c.getLine(1) );
	    }
	}
    y2milestone( "ret:%d n1:%s n2:%s", ret, n1.c_str(), n2.c_str() );
    return( ret );
    }

void
Storage::handleLogFile( const string& name )
    {
    string bname( name );
    string::size_type pos = bname.rfind( '.' );
    bname.erase( pos );
    y2milestone( "name:%s bname:%s", name.c_str(), bname.c_str() );
    if( access( bname.c_str(), R_OK )==0 )
	{
	if( !testFilesEqual( bname, name ) )
	    {
	    unsigned num = max_log_num;
	    string tmpo = bname + "-" + decString(num);
	    string tmpn;
	    if( access( tmpo.c_str(), R_OK )==0 )
		unlink( tmpo.c_str() );
	    while( num>1 )
		{
		tmpn = bname + "-" + decString(num-1);
		if( access( tmpn.c_str(), R_OK )==0 )
		    rename( tmpn.c_str(), tmpo.c_str() );
		tmpo = tmpn;
		num--;
		}
	    rename( bname.c_str(), tmpn.c_str() );
	    rename( name.c_str(), bname.c_str() );
	    }
	else
	    unlink( name.c_str() );
	}
    else
	rename( name.c_str(), bname.c_str() );
    }

void Storage::setRecursiveRemoval( bool val )
    {
    y2milestone( "val:%d", val );
    recursiveRemove = val;
    }

void Storage::setZeroNewPartitions( bool val )
    {
    y2milestone( "val:%d", val );
    zeroNewPartitions = val;
    }

string Storage::proc_arch;


namespace storage
{
    void initDefaultLogger ()
    {
	Storage::initDefaultLogger ();
    }

    StorageInterface* createDefaultStorageInterface ()
    {
	return new Storage ();
    }

    StorageInterface* createStorageInterface (bool ronly, bool testmode, bool autodetect)
    {
	return new Storage (ronly, testmode, autodetect);
    }

    void destroyStorageInterface (StorageInterface* p)
    {
	delete p;
    }
}


int
Storage::createPartition( const string& disk, PartitionType type, unsigned long start,
			  unsigned long size, string& device )
    {
    int ret = 0;
    assertInit();
    y2milestone( "disk:%s type:%d start:%ld size:%ld", disk.c_str(),
                 type, start, size );
    DiskIterator i = findDisk( disk );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != dEnd() )
	{
	if( i->getUsedByType() != UB_NONE )
	    ret = STORAGE_DISK_USED_BY;
	else
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
    y2milestone( "ret:%d device:%s", ret, ret?"":device.c_str() );
    return( ret );
    }

int
Storage::createPartitionKb( const string& disk, PartitionType type,
                            unsigned long long start,
			    unsigned long long sizeK, string& device )
    {
    int ret = 0;
    assertInit();
    y2milestone( "disk:%s type:%d start:%lld sizeK:%lld", disk.c_str(),
                 type, start, sizeK );
    DiskIterator i = findDisk( disk );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != dEnd() )
	{
	if( i->getUsedByType() != UB_NONE )
	    ret = STORAGE_DISK_USED_BY;
	else
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
	}
    else
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    y2milestone( "ret:%d device:%s", ret, ret?"":device.c_str() );
    return( ret );
    }

int
Storage::createPartitionAny( const string& disk, unsigned long long sizeK,
                             string& device )
    {
    int ret = 0;
    assertInit();
    y2milestone( "disk:%s sizeK:%lld", disk.c_str(), sizeK );
    DiskIterator i = findDisk( disk );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != dEnd() )
	{
	if( i->getUsedByType() != UB_NONE )
	    ret = STORAGE_DISK_USED_BY;
	else
	    {
	    unsigned long num_cyl = i->kbToCylinder( sizeK );
	    ret = i->createPartition( num_cyl, device, true );
	    }
	}
    else
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    y2milestone( "ret:%d device:%s", ret, ret?"":device.c_str() );
    return( ret );
    }

int
Storage::nextFreePartition( const string& disk, PartitionType type,
                            unsigned& nr, string& device )
    {
    int ret = 0;
    assertInit();
    y2milestone( "disk:%s type:%u", disk.c_str(), type );
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	ret = i->nextFreePartition( type, nr, device );
	}
    else
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    y2milestone( "ret:%d device:%s", ret, ret?"":device.c_str() );
    return( ret );
    }

int
Storage::createPartitionMax( const string& disk, PartitionType type,
                             string& device )
    {
    int ret = 0;
    assertInit();
    y2milestone( "disk:%s type:%u", disk.c_str(), type );
    DiskIterator i = findDisk( disk );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != dEnd() )
	{
	if( i->getUsedByType() != UB_NONE )
	    ret = STORAGE_DISK_USED_BY;
	else
	    {
	    ret = i->createPartition( type, device );
	    }
	}
    else
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    y2milestone( "ret:%d device:%s", ret, ret?"":device.c_str() );
    return( ret );
    }

unsigned long long
Storage::cylinderToKb( const string& disk, unsigned long size )
    {
    unsigned long long ret = 0;
    assertInit();
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
    assertInit();
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
    assertInit();
    y2milestone( "partition:%s", partition.c_str() );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol ) && cont->type()==DISK )
	{
	Disk* disk = dynamic_cast<Disk *>(&(*cont));
	if( disk!=NULL )
	    {
	    if( vol->getUsedByType() == UB_NONE )
		ret = disk->removePartition( vol->nr() );
	    else if( !recursiveRemove )
		ret = STORAGE_REMOVE_USED_VOLUME;
	    else
		{
		ret = removeUsing( vol->device(), vol->getUsedBy() );
		if( ret==0 )
		    disk->removePartition( vol->nr() );
		}
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
Storage::updatePartitionArea( const string& partition, unsigned long start, 
                              unsigned long size )
    {
    int ret = 0;
    assertInit();
    y2milestone( "partition:%s start:%ld size:%ld", partition.c_str(), 
                 start, size );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol ) && cont->type()==DISK )
	{
	Disk* disk = dynamic_cast<Disk *>(&(*cont));
	if( disk!=NULL )
	    {
	    ret = disk->changePartitionArea( vol->nr(), start, size );
	    }
	else
	    {
	    ret = STORAGE_CHANGE_AREA_INVALID_CONTAINER;
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
    assertInit();
    y2milestone( "partition:%s id:%x", partition.c_str(), id );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol ) && cont->type()==DISK )
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
    assertInit();
    y2milestone( "disk:%s", disk.c_str() );
    DiskIterator i = findDisk( disk );

    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != dEnd() )
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
Storage::defaultDiskLabel() const
    {
    return( Disk::defaultLabel() );
    }

int
Storage::changeFormatVolume( const string& device, bool format, FsType fs )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s format:%d type:%s", device.c_str(), format,
                 Volume::fsTypeString(fs).c_str() );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->setFormat( format, fs );
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
Storage::changeLabelVolume( const string& device, const string& label )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s label:%s", device.c_str(), label.c_str() );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->setLabel( label );
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
Storage::changeMkfsOptVolume( const string& device, const string& opts )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s opts:%s", device.c_str(), opts.c_str() );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->setMkfsOption( opts );
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
Storage::changeMountPoint( const string& device, const string& mount )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s mount:%s", device.c_str(), mount.c_str() );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->changeMount( mount );
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
Storage::getMountPoint( const string& device, string& mount )
{
    int ret = 0;
    assertInit();
    y2milestone( "device:%s", device.c_str());
    VolIterator vol;
    ContIterator cont;
    if( findVolume( device, cont, vol ) )
    {
	mount = vol->getMount();
    }
    else
    {
	ret = STORAGE_VOLUME_NOT_FOUND;
    }
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::changeMountBy( const string& device, MountByType mby )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s mby:%s", device.c_str(),
                 Volume::mbyTypeString(mby).c_str() );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->changeMountBy( mby );
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
Storage::getMountBy( const string& device, MountByType& mby )
{
    int ret = 0;
    assertInit();
    y2milestone( "device:%s", device.c_str());
    VolIterator vol;
    ContIterator cont;
    if( findVolume( device, cont, vol ) )
    {
	mby = vol->getMountBy();
    }
    else
    {
	ret = STORAGE_VOLUME_NOT_FOUND;
    }
    y2milestone( "ret:%d mby:%s", ret, Volume::mbyTypeString(mby).c_str());
    return( ret );
}

int
Storage::changeFstabOptions( const string& device, const string& options )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s options:%s", device.c_str(), options.c_str() );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->changeFstabOptions( options );
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
Storage::getFstabOptions( const string& device, string& options )
{
    int ret = 0;
    assertInit();
    y2milestone( "device:%s", device.c_str());
    VolIterator vol;
    ContIterator cont;
    if( findVolume( device, cont, vol ) )
    {
	options = vol->getFstabOption();
    }
    else
    {
	ret = STORAGE_VOLUME_NOT_FOUND;
    }
    y2milestone( "ret:%d options:%s", ret, options.c_str() );
    return( ret );
}

int
Storage::addFstabOptions( const string& device, const string& options )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s options:%s", device.c_str(), options.c_str() );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	list<string> l = splitString( options, "," );
	list<string> opts = splitString( vol->getFstabOption(), "," );
	for( list<string>::const_iterator i=l.begin(); i!=l.end(); i++ )
	    {
	    if( find( opts.begin(), opts.end(), *i )==opts.end() )
		opts.push_back( *i );
	    }
	ret = vol->changeFstabOptions( mergeString( opts, "," ) );
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
Storage::removeFstabOptions( const string& device, const string& options )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s options:%s", device.c_str(), options.c_str() );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	list<string> l = splitString( options, "," );
	list<string> opts = splitString( vol->getFstabOption(), "," );
	for( list<string>::const_iterator i=l.begin(); i!=l.end(); i++ )
	    {
	    opts.remove_if( match_string( *i ));
	    }
	ret = vol->changeFstabOptions( mergeString( opts, "," ) );
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
Storage::setCrypt( const string& device, bool val )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s val:%d", device.c_str(), val );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->setEncryption( val );
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
Storage::getCrypt( const string& device, bool& val )
{
    int ret = 0;
    assertInit();
    y2milestone( "device:%s", device.c_str());
    VolIterator vol;
    ContIterator cont;
    if( findVolume( device, cont, vol ) )
    {
	val = vol->getEncryption();
    }
    else
    {
	ret = STORAGE_VOLUME_NOT_FOUND;
    }
    y2milestone( "ret:%d  val:%d", ret, val );
    return( ret );
}

int
Storage::setCryptPassword( const string& device, const string& pwd )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s", device.c_str() );
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    y2milestone( "password:%s", pwd.c_str() );
#endif

    VolIterator vol;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, vol ) )
	{
	ret = vol->setCryptPwd( pwd );
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
Storage::forgetCryptPassword( const string& device )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s", device.c_str() );

    VolIterator vol;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, vol ) )
	{
	vol->clearCryptPwd();
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
Storage::getCryptPassword( const string& device, string& pwd )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s", device.c_str() );

    pwd.clear();
    VolIterator vol;
    if( findVolume( device, vol ) )
	{
	pwd = vol->getCryptPwd();
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    y2milestone( "password:%s", pwd.c_str() );
#endif
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::resizeVolume( const string& device, unsigned long long newSizeMb )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s newSizeMb:%llu", device.c_str(), newSizeMb );

    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = cont->resizeVolume( &(*vol), newSizeMb );
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
Storage::removeVolume( const string& device )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s", device.c_str() );

    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	if( vol->getUsedByType() == UB_NONE )
	    ret = cont->removeVolume( &(*vol) );
	else if( !recursiveRemove )
	    ret = STORAGE_REMOVE_USED_VOLUME;
	else
	    {
	    ret = removeUsing( vol->device(), vol->getUsedBy() );
	    if( ret==0 )
		cont->removeVolume( &(*vol) );
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
Storage::createLvmVg( const string& name, unsigned long long peSizeK,
		      bool lvm1, const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil( "name:" << name << " peSizeK:" << peSizeK << " lvm1:" << lvm1 <<
	   " devices:" << devs );
    LvmVgIterator i = findLvmVg( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( name.empty() ||
        name.find_first_of( "\"\' /\n\t:*?" ) != string::npos )
	{
	ret = STORAGE_VG_INVALID_NAME;
	}
    else if( i == lvgEnd() )
	{
	LvmVg *v = new LvmVg( this, name, lvm1 );
	v->setCreated();
	ret = v->setPeSize( peSizeK );
	if( ret==0 )
	    {
	    list<string> d;
	    back_insert_iterator<list<string> > inserter(d);
	    copy( devs.begin(), devs.end(), inserter );
	    ret = v->extendVg( d );
	    }
	if( ret==0 )
	    addToList( v );
	else
	    delete( v );
	}
    else
	{
	ret = STORAGE_LVM_VG_EXISTS;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::removeLvmVg( const string& name )
    {
    int ret = 0;
    assertInit();
    y2milestone( "name:%s", name.c_str() );
    LvmVgIterator i = findLvmVg( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	if( i->created() )
	    ret = removeContainer( &(*i) );
	else
	    ret = i->removeVg();
	}
    else
	{
	ret = STORAGE_LVM_VG_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::extendLvmVg( const string& name, const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil( "name:" << name << " devices:" << devs );
    LvmVgIterator i = findLvmVg( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	list<string> d;
	back_insert_iterator<list<string> > inserter(d);
	copy( devs.begin(), devs.end(), inserter );
	ret = i->extendVg( d );
	}
    else
	{
	ret = STORAGE_LVM_VG_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::shrinkLvmVg( const string& name, const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil( "name:" << name << " devices:" << devs );
    LvmVgIterator i = findLvmVg( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	list<string> d;
	back_insert_iterator<list<string> > inserter(d);
	copy( devs.begin(), devs.end(), inserter );
	ret = i->reduceVg( d );
	}
    else
	{
	ret = STORAGE_LVM_VG_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::createLvmLv( const string& vg, const string& name,
                      unsigned long long sizeM, unsigned stripe,
		      string& device )
    {
    int ret = 0;
    assertInit();
    y2milestone( "vg:%s name:%s sizeM:%llu stripe:%u", vg.c_str(),
                 name.c_str(), sizeM, stripe );
    LvmVgIterator i = findLvmVg( vg );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	ret = i->createLv( name, sizeM*1024, stripe, device );
	}
    else
	{
	ret = STORAGE_LVM_VG_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d device:%s", ret, ret?"":device.c_str() );
    return( ret );
    }

int
Storage::removeLvmLvByDevice( const string& device )
    {
    int ret = 0;
    string vg, name;
    string d = undevDevice( device );
    string::size_type pos = d.find( '/' );
    if( pos!=string::npos )
	{
	vg = d.substr( 0, pos );
	name = d.substr( pos+1 );
	}
    if( !vg.empty() && !name.empty() )
	ret = removeLvmLv( vg, name );
    else
	ret = STORAGE_LVM_INVALID_DEVICE;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::removeLvmLv( const string& vg, const string& name )
    {
    int ret = 0;
    assertInit();
    y2milestone( "vg:%s name:%s", vg.c_str(), name.c_str() );
    LvmVgIterator i = findLvmVg( vg );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	ret = i->removeLv( name );
	}
    else
	{
	ret = STORAGE_LVM_VG_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::createEvmsContainer( const string& name, unsigned long long peSizeK,
			      bool lvm1, const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil( "name:" << name << " peSizeK:" << peSizeK << " lvm1:" << lvm1 <<
	   " devices:" << devs );
    EvmsCoIterator i = findEvmsCo( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( name.empty() ||
        name.find_first_of( "\"\' /\n\t:*?" ) != string::npos )
	{
	ret = STORAGE_EVMS_INVALID_NAME;
	}
    else if( i == evCoEnd() )
	{
	EvmsCo *v = new EvmsCo( this, name, lvm1 );
	v->setCreated();
	ret = v->setPeSize( peSizeK );
	if( ret==0 )
	    {
	    list<string> d;
	    back_insert_iterator<list<string> > inserter(d);
	    copy( devs.begin(), devs.end(), inserter );
	    ret = v->extendCo( d );
	    }
	if( ret==0 )
	    addToList( v );
	else
	    delete( v );
	}
    else
	{
	ret = STORAGE_EVMS_CO_EXISTS;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::removeEvmsContainer( const string& name )
    {
    int ret = 0;
    assertInit();
    y2milestone( "name:%s", name.c_str() );
    EvmsCoIterator i = findEvmsCo( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != evCoEnd() )
	{
	if( i->created() )
	    ret = removeContainer( &(*i) );
	else
	    ret = i->removeCo();
	}
    else
	{
	ret = STORAGE_EVMS_CO_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::extendEvmsContainer( const string& name, const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil( "name:" << name << " devices:" << devs );
    EvmsCoIterator i = findEvmsCo( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != evCoEnd() )
	{
	list<string> d;
	back_insert_iterator<list<string> > inserter(d);
	copy( devs.begin(), devs.end(), inserter );
	ret = i->extendCo( d );
	}
    else
	{
	ret = STORAGE_EVMS_CO_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::shrinkEvmsContainer( const string& name, const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil( "name:" << name << " devices:" << devs );
    EvmsCoIterator i = findEvmsCo( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != evCoEnd() )
	{
	list<string> d;
	back_insert_iterator<list<string> > inserter(d);
	copy( devs.begin(), devs.end(), inserter );
	ret = i->reduceCo( d );
	}
    else
	{
	ret = STORAGE_EVMS_CO_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::createEvmsVolume( const string& co, const string& name,
			   unsigned long long sizeM, unsigned stripe,
			   string& device )
    {
    int ret = 0;
    assertInit();
    y2milestone( "co:%s name:%s sizeM:%llu stripe:%u", co.c_str(),
                 name.c_str(), sizeM, stripe );
    EvmsCoIterator i = findEvmsCo( co );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != evCoEnd() )
	{
	ret = i->createVol( name, sizeM*1024, stripe, device );
	}
    else
	{
	ret = STORAGE_EVMS_CO_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d device:%s", ret, ret?"":device.c_str() );
    return( ret );
    }

int
Storage::removeEvmsVolumeByDevice( const string& device )
    {
    int ret = 0;
    string co, name;
    string d = undevDevice( device );
    if( d.find( "evms/" )==0 )
	d.erase( 0, 5 );
    string::size_type pos = d.find( '/' );
    pos = d.find( '/', pos+1 );
    if( pos!=string::npos )
	{
	co = d.substr( 0, pos );
	name = d.substr( pos+1 );
	}
    if( !co.empty() && !name.empty() )
	ret = removeEvmsVolume( co, name );
    else
	ret = STORAGE_EVMS_INVALID_DEVICE;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::removeEvmsVolume( const string& co, const string& name )
    {
    int ret = 0;
    assertInit();
    y2milestone( "co:%s name:%s", co.c_str(), name.c_str() );
    EvmsCoIterator i = findEvmsCo( co );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != evCoEnd() )
	{
	ret = i->removeVol( name );
	}
    else
	{
	ret = STORAGE_EVMS_CO_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::createMd( const string& name, MdType rtype,
                   const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil( "name:" << name << " MdType:" << Md::pName(rtype) << 
           " devices:" << devs );
    unsigned num = 0;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 && !Md::mdStringNum( name, num ))
	{
	ret = STORAGE_MD_INVALID_NAME;
	}
    MdCo *md = NULL;
    bool have_md = true;
    if( ret==0 )
	{
	have_md = haveMd(md);
	if( !have_md )
	    md = new MdCo( this, false );
	}
    if( ret==0 && md!=NULL )
	{
	list<string> d;
	d.insert( d.end(), devs.begin(), devs.end() );
	ret = md->createMd( num, rtype, d );
	}
    if( !have_md )
	{
	if( ret==0 )
	    addToList( md );
	else if( md!=NULL )
	    delete md;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Storage::createMdAny( MdType rtype, const deque<string>& devs,
			  string& device )
    {
    int ret = 0;
    assertInit();
    y2mil( "MdType:" << Md::pName(rtype) << " devices:" << devs );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    MdCo *md = NULL;
    bool have_md = true;
    unsigned num = 0;
    if( ret==0 )
	{
	have_md = haveMd(md);
	if( !have_md )
	    md = new MdCo( this, false );
	else
	    num = md->unusedNumber();
	if( md==NULL )
	    ret = STORAGE_MEMORY_EXHAUSTED;
	}
    if( ret==0 )
	{
	list<string> d;
	d.insert( d.end(), devs.begin(), devs.end() );
	ret = md->createMd( num, rtype, d );
	}
    if( !have_md )
	{
	if( ret==0 )
	    {
	    addToList( md );
	    }
	else if( md!=NULL )
	    delete md;
	}
    if( ret==0 )
	{
	device = "/dev/md" + decString(num);
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d device:%s", ret, ret==0?device.c_str():"" );
    return( ret );
    }

int Storage::removeMd( const string& name, bool destroySb )
    {
    int ret = 0;
    assertInit();
    y2milestone( "name:%s destroySb:%d", name.c_str(), destroySb );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    unsigned num = 0;
    if( ret==0 && !Md::mdStringNum( name, num ))
	{
	ret = STORAGE_MD_INVALID_NAME;
	}
    if( ret==0 )
	{
	MdCo *md = NULL;
	if( haveMd(md) )
	    ret = md->removeMd( num, destroySb );
	else
	    ret = STORAGE_MD_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

bool Storage::haveMd( MdCo*& md )
    {
    md = NULL;
    CPair p = cPair();
    ContIterator i = p.begin();
    while( i != p.end() && i->type()!=MD )
	++i;
    if( i != p.end() )
	md = static_cast<MdCo*>(&(*i));
    return( i != p.end() );
    }

int 
Storage::createFileLoop( const string& lname, bool reuseExisting,
                         unsigned long long sizeK, const string& mp,
			 const string& pwd, string& device )
    {
    int ret = 0;
    assertInit();
    y2milestone( "lname:%s reuseExisting:%d sizeK:%llu mp:%s", lname.c_str(),
                 reuseExisting, sizeK, mp.c_str() );
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    y2milestone( "pwd:%s", pwd.c_str() );
#endif
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    LoopCo *loop = NULL;
    Volume *vol = NULL;
    bool have_loop = true;
    if( ret==0 )
	{
	have_loop = haveLoop(loop);
	if( !have_loop )
	    loop = new LoopCo( this, false );
	if( loop==NULL )
	    ret = STORAGE_MEMORY_EXHAUSTED;
	}
    if( ret==0 )
	{
	ret = loop->createLoop( lname, reuseExisting, sizeK, device );
	}
    if( ret==0 && !loop->findVolume( device, vol ))
	{
	ret = STORAGE_CREATED_LOOP_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = vol->setCryptPwd( pwd );
	}
    if( ret==0 && (!reuseExisting || access( (root()+lname).c_str(), R_OK )!=0 ))
	{
	ret = vol->setFormat( true, EXT3 );
	}
    if( ret==0 )
	{
	ret = vol->setEncryption( true );
	}
    if( ret==0 and !mp.empty() )
	{
	ret = vol->changeMount( mp );
	}
    if( !have_loop )
	{
	if( ret==0 )
	    {
	    addToList( loop );
	    }
	else if( loop!=NULL )
	    delete loop;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d device:%s", ret, ret==0?device.c_str():"" );
    return( ret );
    }

int 
Storage::removeFileLoop( const string& lname, bool removeFile )
    {
    int ret = 0;
    assertInit();
    y2milestone( "lname:%s removeFile:%d", lname.c_str(), removeFile );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	LoopCo *loop = NULL;
	if( haveLoop(loop) )
	    ret = loop->removeLoop( lname, removeFile );
	else
	    ret = STORAGE_LOOP_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }


bool Storage::haveLoop( LoopCo*& loop )
    {
    loop = NULL;
    CPair p = cPair();
    ContIterator i = p.begin();
    while( i != p.end() && i->type()!=LOOP )
	++i;
    if( i != p.end() )
	loop = static_cast<LoopCo*>(&(*i));
    return( i != p.end() );
    }

int Storage::checkCache()
    {
    int ret=0;
    if( !isCacheChanges() )
	ret = commit();
    return(ret);
    }

deque<string> Storage::getCommitActions( bool mark_destructive )
    {
    deque<string> ret;
    CPair p = cPair();
    y2milestone( "empty:%d", p.empty() );
    if( !p.empty() )
	{
	list<commitAction*> ac;
	for( ContIterator i = p.begin(); i != p.end(); ++i )
	    {
	    i->getCommitActions( ac );
	    }
	ac.sort( cont_less<commitAction>() );
	string txt;
	for( list<commitAction*>::const_iterator i=ac.begin(); i!=ac.end(); ++i )
	    {
	    txt.erase();
	    if( mark_destructive && (*i)->destructive )
		txt += "<font color=red>";
	    txt += (*i)->descr;
	    if( mark_destructive && (*i)->destructive )
		txt += "</font>";
	    ret.push_back( txt );
	    }
	}
    y2milestone( "ret.size():%zd", ret.size() );
    return( ret );
    }

static bool sort_cont_up( const Container* rhs, const Container* lhs )
    {
    return( *rhs > *lhs );
    }

static bool sort_cont_down( const Container* rhs, const Container* lhs )
    {
    return( *rhs < *lhs );
    }

static bool sort_vol_normal( const Volume* rhs, const Volume* lhs )
    {
    return( *rhs < *lhs );
    }

static bool sort_vol_delete( const Volume* rhs, const Volume* lhs )
    {
    if( rhs->isMounted()==lhs->isMounted()  )
	{
        if( rhs->isMounted() )
	    return( rhs->getMount()>lhs->getMount() );
	else
	    return( *rhs > *lhs );
	}
    else
	return( rhs->isMounted() );
    }

static bool sort_vol_create( const Volume* rhs, const Volume* lhs )
    {
    if( rhs->cType()==lhs->cType() )
	{
	if( rhs->cType()==LVM||rhs->cType()==EVMS )
	    return( static_cast<const LvmLv*>(rhs)->stripes() >
	            static_cast<const LvmLv*>(lhs)->stripes() );
	else
	    return( *rhs < *lhs );
	}
    else
	return( *rhs < *lhs );
    }

static bool sort_vol_mount( const Volume* rhs, const Volume* lhs )
    {
    if( rhs->getMount()=="swap" )
	return( false );
    else if( lhs->getMount()=="swap" )
	return( true );
    else
	return( rhs->getMount()<lhs->getMount() );
    }

void
Storage::sortCommitLists( CommitStage stage, list<Container*>& co,
                          list<Volume*>& vl )
    {
    co.sort( (stage==DECREASE)?sort_cont_up:sort_cont_down );
    if( stage==DECREASE )
	vl.sort( sort_vol_delete );
    else if( stage==INCREASE )
	vl.sort( sort_vol_create );
    else if( stage==MOUNT )
	vl.sort( sort_vol_mount );
    else
	vl.sort( sort_vol_normal );
    std::ostringstream b;
    y2milestone( "stage %d", stage );
    b << "sorted co <";
    for( list<Container*>::const_iterator i=co.begin(); i!=co.end(); ++i )
	{
	if( i!=co.begin() )
	    b << " ";
	b << (*i)->name();
	}
    b << "> ";
    y2milestone( "%s", b.str().c_str() );
    b.str("");
    b << "sorted vol <";
    for( list<Volume*>::const_iterator i=vl.begin(); i!=vl.end(); ++i )
	{
	if( i!=vl.begin() )
	    b << " ";
	b << (*i)->device();
	}
    b << "> ";
    y2milestone( "%s", b.str().c_str() );
    }

static bool notLoop( const Container& c ) { return( c.type()!=LOOP ); }
static bool fstabAdded( const Volume& v ) { return( v.fstabAdded()); }

int Storage::commit()
    {
    assertInit();
    lastAction.clear();
    CPair p = cPair( notLoop );
    int ret = 0;
    y2milestone( "empty:%d", p.empty() );
    if( !p.empty() )
	{
	ret = commitPair( p );
	}
    p = cPair( isLoop );
    y2milestone( "empty:%d", p.empty() );
    if( ret==0 && !p.empty() )
	{
	ret = commitPair( p );
	}
    VPair vp = vPair( fstabAdded );
    for( VolIterator i=vp.begin(); i!=vp.end(); ++i )
	i->setFstabAdded(false);
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int  
Storage::commitPair( CPair& p )
    {
    int ret = 0;
    y2milestone( "p.length:%d", p.length() );
    CommitStage a[] = { DECREASE, INCREASE, FORMAT, MOUNT };
    CommitStage* pt = a;
    while( unsigned(pt-a) < lengthof(a) )
	{
	bool cont_removed = false;
	list<Container*> colist;
	list<Volume*> vlist;
	ContIterator i = p.begin();
	while( ret==0 && i != p.end() )
	    {
	    ret = i->getToCommit( *pt, colist, vlist );
	    ++i;
	    }
	sortCommitLists( *pt, colist, vlist );
	list<Volume*>::iterator vli = vlist.begin();
	list<Container*>::iterator cli;
	while( ret==0 && vli != vlist.end() )
	    {
	    Container *co = const_cast<Container*>((*vli)->getContainer());
	    cli = find( colist.begin(), colist.end(), co );
	    if( *pt!=DECREASE && cli!=colist.end() )
		{
		ret = co->commitChanges( *pt );
		colist.erase( cli );
		}
	    if( ret==0 )
		ret = co->commitChanges( *pt, *vli );
	    ++vli;
	    }
	if( ret==0 && !colist.empty() )
	    ret = performContChanges( *pt, colist, cont_removed );
	if( cont_removed )
	    p = cPair();
	pt++;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::performContChanges( CommitStage stage, const list<Container*>& co,
                             bool& cont_removed )
    {
    y2milestone( "list size:%zu", co.size() );
    int ret = 0;
    cont_removed = false;
    list<Container*> cont_remove;
    list<Container*>::const_iterator cli = co.begin();
    while( ret==0 && cli != co.end() )
	{
	Container *co = *cli;
	y2milestone( "before commit:%p", co );
	ret = co->commitChanges( stage );
	y2milestone( "after commit:%p", co );
	if( stage==DECREASE && co->deleted() &&
	    (co->type()==LVM||co->type()==EVMS) )
	    cont_remove.push_back( co );
	++cli;
	}
    if( !cont_remove.empty() )
	{
	for( list<Container*>::iterator c=cont_remove.begin();
	     c!=cont_remove.end(); ++c )
	    {
	    int r = removeContainer( *c );
	    if( ret==0 )
		ret = r;
	    }
	cont_removed = true;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

static bool showContainers( const Container& c ) 
    { return( !c.deleted()||c.type()==DISK ); }

void 
Storage::getContainers( deque<ContainerInfo>& infos )
    {
    infos.clear ();
    assertInit();
    ConstContPair p = contPair( showContainers );
    for( ConstContIterator i = p.begin(); i != p.end(); ++i)
	{
	infos.push_back( ContainerInfo() );
	i->getInfo( infos.back() );
	}
    }

void 
Storage::getVolumes( deque<VolumeInfo>& infos )
    {
    infos.clear ();
    assertInit();
    ConstVolPair p = volPair( Volume::notDeleted );
    for( ConstVolIterator i = p.begin(); i != p.end(); ++i)
	{
	infos.push_back( VolumeInfo() );
	i->getInfo( infos.back() );
	}
    }

int 
Storage::getVolume( const string& device, VolumeInfo& info )
    {
    int ret = 0;
    VolIterator v;
    if( findVolume( device, v ))
	{
	v->getInfo( info );
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    return( ret );
    }

int Storage::getDiskInfo( const string& disk, DiskInfo& info )
    {
    int ret = 0;
    assertInit();
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	i->getInfo( info );
	}
    else
	ret = STORAGE_DISK_NOT_FOUND;
    return( ret );
    }

int Storage::getContDiskInfo( const string& disk, ContainerInfo& cinfo, 
			      DiskInfo& info )
    {
    int ret = 0;
    assertInit();
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	((const Container*)&(*i))->getInfo( cinfo );
	i->getInfo( info );
	}
    else
	ret = STORAGE_DISK_NOT_FOUND;
    return( ret );
    }

int Storage::getPartitionInfo( const string& disk, 
			       deque<storage::PartitionInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	Disk::PartPair p = i->partPair (Disk::notDeleted);
	for (Disk::PartIter i2 = p.begin(); i2 != p.end(); ++i2)
	    {
	    plist.push_back( PartitionInfo() );
	    i2->getInfo( plist.back() );
	    ((const Volume*)&(*i2))->getInfo( plist.back().v );
	    }
	}
    else
	ret = STORAGE_DISK_NOT_FOUND;
    return( ret );
    }

int Storage::getLvmVgInfo( const string& name, LvmVgInfo& info )
    {
    int ret = 0;
    assertInit();
    LvmVgIterator i = findLvmVg( name );
    if( i != lvgEnd() )
	{
	i->getInfo( info );
	}
    else
	ret = STORAGE_LVM_VG_NOT_FOUND;
    return( ret );
    }

int Storage::getContLvmVgInfo( const string& name, ContainerInfo& cinfo,
                               LvmVgInfo& info )
    {
    int ret = 0;
    assertInit();
    LvmVgIterator i = findLvmVg( name );
    if( i != lvgEnd() )
	{
	((const Container*)&(*i))->getInfo( cinfo );
	i->getInfo( info );
	}
    else
	ret = STORAGE_LVM_VG_NOT_FOUND;
    return( ret );
    }

int Storage::getLvmLvInfo( const string& name, 
			   deque<storage::LvmLvInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    LvmVgIterator i = findLvmVg( name );
    if( i != lvgEnd() )
	{
	LvmVg::LvmLvPair p = i->lvmLvPair(LvmVg::lvNotDeleted);
	for( LvmVg::LvmLvIter i2 = p.begin(); i2 != p.end(); ++i2)
	    {
	    plist.push_back( LvmLvInfo() );
	    i2->getInfo( plist.back() );
	    ((const Volume*)&(*i2))->getInfo( plist.back().v );
	    }
	}
    else
	ret = STORAGE_LVM_VG_NOT_FOUND;
    return( ret );
    }

int Storage::getEvmsCoInfo( const string& name, EvmsCoInfo& info )
    {
    int ret = 0;
    assertInit();
    EvmsCoIterator i = findEvmsCo( name );
    if( i != evCoEnd() )
	{
	i->getInfo( info );
	}
    else
	ret = STORAGE_EVMS_CO_NOT_FOUND;
    return( ret );
    }

int Storage::getContEvmsCoInfo( const string& name, ContainerInfo& cinfo,
			        EvmsCoInfo& info )
    {
    int ret = 0;
    assertInit();
    EvmsCoIterator i = findEvmsCo( name );
    if( i != evCoEnd() )
	{
	((const Container*)&(*i))->getInfo( cinfo );
	i->getInfo( info );
	}
    else
	ret = STORAGE_EVMS_CO_NOT_FOUND;
    return( ret );
    }

int Storage::getEvmsInfo( const string& name, 
			  deque<storage::EvmsInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    EvmsCoIterator i = findEvmsCo( name );
    if( i != evCoEnd() )
	{
	EvmsCo::EvmsPair p = i->evmsPair(EvmsCo::lvNotDeleted);
	for( EvmsCo::EvmsIter i2 = p.begin(); i2 != p.end(); ++i2 )
	    {
	    plist.push_back( EvmsInfo() );
	    i2->getInfo( plist.back() );
	    ((const Volume*)&(*i2))->getInfo( plist.back().v );
	    }
	}
    else
	ret = STORAGE_EVMS_CO_NOT_FOUND;
    return( ret );
    }

int Storage::getMdInfo( deque<storage::MdInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    ConstMdPair p = mdPair(Md::notDeleted);
    for( ConstMdIterator i = p.begin(); i != p.end(); ++i )
	{
	plist.push_back( MdInfo() );
	i->getInfo( plist.back() );
	((const Volume*)&(*i))->getInfo( plist.back().v );
	}
    return( ret );
    }

int Storage::getLoopInfo( deque<storage::LoopInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    ConstLoopPair p = loopPair(Loop::notDeleted);
    for( ConstLoopIterator i = p.begin(); i != p.end(); ++i )
	{
	plist.push_back( LoopInfo() );
	i->getInfo( plist.back() );
	((const Volume*)&(*i))->getInfo( plist.back().v );
	}
    return( ret );
    }

int Storage::getDmInfo( deque<storage::DmInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    ConstDmPair p = dmPair(Dm::notDeleted);
    for( ConstDmIterator i = p.begin(); i != p.end(); ++i )
	{
	plist.push_back( DmInfo() );
	i->getInfo( plist.back() );
	((const Volume*)&(*i))->getInfo( plist.back().v );
	}
    return( ret );
    }


bool
Storage::getFsCapabilities (FsType fstype, FsCapabilities& fscapabilities) const
{
    static FsCapabilities reiserfsCaps = {
	isExtendable: true,
	isExtendableWhileMounted: true,
	isReduceable: true,
	isReduceableWhileMounted: false,
	supportsUuid: true,
	supportsLabel: true,
	labelWhileMounted: false,
	labelLength: 16,
	minimalFsSizeK: 50*1024
    };

    static FsCapabilities ext2Caps = {
	isExtendable: true,
	isExtendableWhileMounted: false,
	isReduceable: true,
	isReduceableWhileMounted: false,
	supportsUuid: true,
	supportsLabel: true,
	labelWhileMounted: true,
	labelLength: 16,
	minimalFsSizeK: 1*1024
    };

    static FsCapabilities ext3Caps = {
	isExtendable: true,
	isExtendableWhileMounted: false,
	isReduceable: true,
	isReduceableWhileMounted: false,
	supportsUuid: true,
	supportsLabel: true,
	labelWhileMounted: true,
	labelLength: 16,
	minimalFsSizeK: 10*1024
    };

    static FsCapabilities xfsCaps = {
	isExtendable: true,
	isExtendableWhileMounted: true,
	isReduceable: false,
	isReduceableWhileMounted: false,
	supportsUuid: true,
	supportsLabel: true,
	labelWhileMounted: false,
	labelLength: 12,
	minimalFsSizeK: 40*1024
    };

    static FsCapabilities ntfsCaps = {
	isExtendable: true,
	isExtendableWhileMounted: false,
	isReduceable: true,
	isReduceableWhileMounted: false,
	supportsUuid: false,
	supportsLabel: false,
	labelWhileMounted: false,
	labelLength: 0,
	minimalFsSizeK: 10*1024
    };

    static FsCapabilities fatCaps = {
	isExtendable: true,
	isExtendableWhileMounted: false,
	isReduceable: true,
	isReduceableWhileMounted: false,
	supportsUuid: false,
	supportsLabel: false,
	labelWhileMounted: false,
	labelLength: 0,
	minimalFsSizeK: 1*1024
	};

    static FsCapabilities swapCaps = {
	isExtendable: true,
	isExtendableWhileMounted: false,
	isReduceable: true,
	isReduceableWhileMounted: false,
	supportsUuid: false,
	supportsLabel: false,
	labelWhileMounted: false,
	labelLength: 0,
	minimalFsSizeK: 1*1024
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

	case VFAT:
	    fscapabilities = fatCaps;
	    return true;

	case NTFS:
	    fscapabilities = ntfsCaps;
	    return true;

	case SWAP:
	    fscapabilities = swapCaps;
	    return true;

	default:
	    return false;
    }
}

bool Storage::findVolume( const string& device, ContIterator& c,
                          VolIterator& v )
    {
    bool ret = false;
    if( findVolume( device, v ))
	{
	const Container *co = v->getContainer();
	CPair cp = cPair();
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

bool Storage::findVolume( const string& device, VolIterator& v )
    {
    assertInit();
    string d = normalizeDevice( device );
    VPair p = vPair( Volume::notDeleted );
    v = p.begin();
    while( v!=p.end() && v->device()!=d )
        {
	const list<string>& al( v->altNames() );
	if( find( al.begin(), al.end(), d )!=al.end() )
	    break;
	++v;
	}
    return( v!=p.end() );
    }

bool Storage::setUsedBy( const string& dev, UsedByType typ, const string& name )
    {
    bool ret=true;
    VolIterator v;
    if( !findVolume( dev, v ) )
	{
	DiskIterator i = findDisk( dev );
	if( i != dEnd() )
	    {
	    y2mil( "disk before" << *i << endl );
	    i->setUsedBy( typ, name );
	    y2mil( "disk after" << *i << endl );
	    }
	else
	    {
	    ret = false;
	    y2error( "could not set used by %d:%s for %s", typ, name.c_str(),
		     dev.c_str() );
	    }
	}
    else
	{
	v->setUsedBy( typ, name );
	}
    y2milestone( "dev:%s usedBy %d:%s ret:%d", dev.c_str(), typ, name.c_str(),
                 ret );
    return( ret );
    }

UsedByType Storage::usedBy( const string& dev )
    {
    UsedByType ret=UB_NONE;
    VolIterator v;
    if( !findVolume( dev, v ) )
	{
	DiskIterator i = findDisk( dev );
	if( i != dEnd() )
	    {
	    ret = i->getUsedByType();
	    }
	}
    else
	{
	ret = v->getUsedByType();
	}
    y2milestone( "dev:%s ret:%d", dev.c_str(), ret );
    return( ret );
    }

void Storage::progressBarCb( const string& id, unsigned cur, unsigned max )
    {
    y2milestone( "id:%s cur:%d max:%d", id.c_str(), cur, max );
    if( progress_bar_cb )
	(*progress_bar_cb)( id, cur, max );
    }

void Storage::showInfoCb( const string& info )
    {
    y2milestone( "INSTALL INFO:%s", info.c_str() );
    lastAction = info;
    if( install_info_cb )
	(*install_info_cb)( info );
    }

void Storage::infoPopupCb( const string& info )
    {
    y2milestone( "INFO POPUP:%s", info.c_str() );
    if( info_popup_cb )
	(*info_popup_cb)( info );
    }

bool Storage::yesnoPopupCb( const string& info )
    {
    y2milestone( "YESNO POPUP:%s", info.c_str() );
    if( yesno_popup_cb )
	return (*yesno_popup_cb)( info );
    else
	return( true );
    }

Storage::DiskIterator Storage::findDisk( const string& disk )
    {
    assertInit();
    string d = normalizeDevice( disk );
    DiskPair p = dPair();
    DiskIterator ret=p.begin();
    while( ret != p.end() && ret->device()!=d )
	++ret;
    return( ret );
    }

Storage::LvmVgIterator Storage::findLvmVg( const string& name )
    {
    assertInit();
    LvmVgPair p = lvgPair();
    LvmVgIterator ret=p.begin();
    while( ret != p.end() && (ret->deleted() || ret->name()!=name) )
	++ret;
    return( ret );
    }

Storage::EvmsCoIterator Storage::findEvmsCo( const string& name )
    {
    assertInit();
    EvmsCoPair p = evCoPair();
    EvmsCoIterator ret=p.begin();
    string name1 = name;
    string name2 = name;
    if( !name.empty() && name.find( "lvm/" )!=0 && name.find( "lvm2/" )!=0 )
	{
	name1 = "lvm/" + name1;
	name2 = "lvm2/" + name2;
	}
    while( ret != p.end() && 
           (ret->deleted() || (ret->name()!=name1 && ret->name()!=name2)) )
	++ret;
    return( ret );
    }

bool Storage::knownDevice( const string& dev, bool disks_allowed )
    {
    bool ret=true;
    VolIterator v;
    if( !findVolume( dev, v ) )
	{
	ret = disks_allowed && findDisk( dev )!=dEnd();
	}
    y2milestone( "dev:%s ret:%d", dev.c_str(), ret );
    return( ret );
    }

const Volume*
Storage::getVolume( const string& dev )
    {
    const Volume* ret=NULL;
    VolIterator v;
    if( findVolume( dev, v ) )
	{
	ret = &(*v);
	}
    y2milestone( "dev:%s ret:%s", dev.c_str(),
                 ret?ret->device().c_str():"nil" );
    return( ret );
    }

bool Storage::canUseDevice( const string& dev, bool disks_allowed )
    {
    bool ret=true;
    VolIterator v;
    if( !findVolume( dev, v ) )
	{
	if( disks_allowed )
	    {
	    DiskIterator i = findDisk( dev );
	    ret = i!=dEnd() && i->getUsedByType()==UB_NONE &&
	          i->numPartitions()==0;
	    }
	else
	    ret = false;
	}
    else
	{
	ret = v->canUseDevice();
	}
    y2milestone( "dev:%s ret:%d", dev.c_str(), ret );
    return( ret );
    }

string Storage::deviceByNumber( const string& majmin )
    {
    string ret="";
    string::size_type pos = majmin.find( ":" );
    if( pos!=string::npos )
	{
	unsigned ma, mi;
	majmin.substr( 0, pos ) >> ma;
	majmin.substr( pos+1 ) >> mi;
	ConstVolPair p = volPair( Volume::notDeleted );
	ConstVolIterator v = p.begin();
	while( v!=p.end() && (ma!=v->majorNr() || mi!=v->minorNr()))
	   {
	   ++v;
	   }
	if( v==p.end() )
	    {
	    ConstDiskPair d = diskPair();
	    ConstDiskIterator di = d.begin();
	    while( di!=d.end() && (ma!=di->majorNr() || mi!=di->minorNr()))
		++di;
	    if( di!=d.end() )
		ret = di->device();
	    }
	else
	    ret = v->device();
	}
    y2milestone( "majmin %s ret:%s", majmin.c_str(), ret.c_str() );
    return( ret );
    }

unsigned long long Storage::deviceSize( const string& dev )
    {
    unsigned long long ret=0;
    VolIterator v;
    if( !findVolume( dev, v ) )
	{
	DiskIterator i = findDisk( dev );
	if( i!=dEnd() )
	    ret = i->capacityInKb();
	}
    else
	ret = v->sizeK();
    y2milestone( "dev:%s ret:%llu", dev.c_str(), ret );
    return( ret );
    }

int Storage::removeContainer( Container* val )
    {
    y2milestone( "name:%s", val->name().c_str() );
    int ret = 0;
    CIter i=cont.begin();
    while( i!=cont.end() && *i!=val )
	++i;
    if( i!=cont.end() )
	{
	delete( *i );
	cont.erase( i );
	}
    else
	{
	ret = STORAGE_CONTAINER_NOT_FOUND;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Storage::removeUsing( const string& device, const storage::usedBy& uby )
    {
    y2mil( "device:" << device << " usedBy:" << uby );
    int ret=0;
    switch( uby.type() )
	{
	case UB_MD:
	    ret = removeVolume(  "/dev/md" + uby.name() );
	    break;
	case UB_DM:
	    ret = removeVolume(  "/dev/dm-" + uby.name() );
	    break;
	case UB_LVM:
	    ret = removeLvmVg( uby.name() );
	    break;
	case UB_EVMS:
	    if( !uby.name().empty() )
		ret = removeEvmsContainer( uby.name() );
	    else
		ret = removeVolume( "/dev/evms/" + uby.name() );
	    break;
	case UB_NONE:
	    y2warning( "%s used by none", device.c_str() );
	    break;
	default:
	    ret = STORAGE_REMOVE_USING_UNKNOWN_TYPE;
	    break;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void Storage::rootMounted()
    {
    MdCo* md;
    root_mounted = true;
    if( !root().empty() )
	{
    	if( haveMd(md) )
	    md->syncRaidtab();
	int ret = fstab->changeRootPrefix( root() );
	if( ret!=0 )
	    y2error( "changeRootPrefix returns %d", ret );
	}
    }

bool
Storage::checkDeviceMounted( const string& device, string& mp )
    {
    bool ret = false;
    assertInit();
    y2milestone( "device:%s", device.c_str() );
    VolIterator vol;
    if( findVolume( device, vol ) )
	{
	ProcMounts mountData;
	mp = mountData.getMount( vol->mountDevice() );
	if( mp.empty() )
	    mp = mountData.getMount( vol->altNames() );
	ret = !mp.empty();
	}
    y2milestone( "ret:%d mp:%s", ret, mp.c_str() );
    return( ret );
    }

bool
Storage::umountDevice( const string& device )
    {
    bool ret = false;
    assertInit();
    y2milestone( "device:%s", device.c_str() );
    VolIterator vol;
    if( !readonly && findVolume( device, vol ) )
	{
	if( vol->umount()==0 )
	    {
	    vol->loUnsetup();
	    ret = true;
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

bool
Storage::mountDevice( const string& device, const string& mp )
    {
    bool ret = false;
    assertInit();
    y2milestone( "device:%s mp:%s", device.c_str(), mp.c_str() );
    VolIterator vol;
    if( !readonly && findVolume( device, vol ) )
	{
	if( vol->needLosetup() && vol->doLosetup() )
	    {
	    ret = vol->mount( mp )==0;
	    if( !ret )
		vol->loUnsetup();
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }


Storage::SkipDeleted Storage::SkipDel;
