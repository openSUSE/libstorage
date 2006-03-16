/*
  Textdomain    "storage"
*/

#include <dirent.h>
#include <glob.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
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
#include "y2storage/Dasd.h"
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
    defaultMountBy = MOUNTBY_DEVICE;
    detectMounted = true;
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
	{
	tempdir = tbuf;
	rmdir( tempdir.c_str() );
	}
    if( access( "/etc/sysconfig/storage", R_OK )==0 )
	{
	AsciiFile sc( "/etc/sysconfig/storage" );
	Regex r( '^' + Regex::ws + "DEVICE_NAMES" + Regex::ws + '=' );
	int line = sc.find( 0, r );
	if( line >= 0 )
	    {
	    list<string> ls = splitString( sc[line], " \t=\"" );
	    y2mil( "ls:" << ls );
	    if( ls.size()==2 )
		{
		string val = ls.back();
		tolower(val);
		if( val == "id" )
		    setDefaultMountBy( MOUNTBY_ID );
		else if( val == "path" )
		    setDefaultMountBy( MOUNTBY_PATH );
		}
	    }
	}
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
    detectObjects();
    setCacheChanges( true );
    ostringstream buf;
    printInfo( buf );
    std::list<string> l = splitString( buf.str(), "\n" );
    y2milestone( "DETECTED OBJECTS" );
    std::list<string>::const_iterator i = l.begin();
    while( i!=l.end() )
	{
	y2milestone( "%s", i->c_str() );
	++i;
	}
    y2milestone( "END DETECTED OBJECTS" );
    }

void Storage::detectObjects()
    {
    EvmsCo::activate(true);
    detectDisks();
    if( instsys() )
	{
	MdCo::activate( true );
	LvmVg::activate( true );
	}
    detectMds();
    detectLvmVgs();
    detectEvms();
    detectDm();

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
	fstab = new EtcFstab( "/etc", isRootMounted() );
	detectLoops();
	detectFsData( vBegin(), vEnd() );
	}
#if 0
    if( instsys() )
	{
	LvmVg::activate( false );
	EvmsCo::activate( false );
	MdCo::activate( false );
	}
#endif
    }

void Storage::deleteClist( CCont& co )
    {
    for( CIter i=co.begin(); i!=co.end(); ++i )
	delete( *i );
    co.clear();
    }

void Storage::deleteBackups()
    {
    map<string,CCont>::iterator i = backups.begin();
    while( i!=backups.end() )
	{
	deleteClist( i->second );
	++i;
	}
    backups.clear();
    }

Storage::~Storage()
    {
    if( max_log_num>0 )
	{
	logVolumes( logdir );
	for( CIter i=cont.begin(); i!=cont.end(); ++i )
	    {
	    (*i)->logData( logdir );
	    }
	}
    deleteClist(cont);
    deleteBackups();
    if( !tempdir.empty() && access( tempdir.c_str(), R_OK )==0 )
	{
	SystemCmd c( "rmdir " + tempdir );
	if( c.retcode()!=0 )
	    {
	    y2error( "stray tmpfile" );
	    c.execute( "ls -l" + tempdir );
	    c.execute( "rm -rf " + tempdir );
	    }
	}
    y2milestone( "destructed Storage" );
    }

void Storage::rescanEverything()
    {
    deleteClist(cont);
    detectObjects();
    }

const string& Storage::tmpDir() const
    {
    if( access( tempdir.c_str(), W_OK )!= 0 )
	mkdir( tempdir.c_str(), 0700 );
    return( tempdir );
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
	if( !v->isEmpty() )
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
	if( !v->isEmpty() )
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
    else if( getenv( "YAST2_STORAGE_NO_LVM" )==NULL )
	{
	list<string> l;
	LvmVg::getVgs( l );
	for( list<string>::const_iterator i=l.begin(); i!=l.end(); ++i )
	    {
	    LvmVg * v = new LvmVg( this, *i );
	    if( !v->inactive() )
		{
		addToList( v );
		v->checkConsistency();
		}
	    else
		{
		y2milestone( "inactive VG %s", i->c_str() );
		v->unuseDev();
		delete( v );
		}
	    }
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
    else if( getenv( "YAST2_STORAGE_NO_EVMS" )==NULL )
	{
	EvmsCo::activate( true );
	EvmsTree data;
	EvmsCo::getEvmsList( data );
	if( !data.volumes.empty() )
	    {
	    EvmsCo * e = new EvmsCo( this, data );
	    addToList( e );
	    for( list<EvmsCont>::const_iterator i=data.cont.begin();
	         i!=data.cont.end(); ++i )
		{
		EvmsCo * e = new EvmsCo( this, *i, data );
		addToList( e );
		e->checkConsistency();
		}
	    }
	if( (data.cont.empty()&&data.volumes.empty()) )
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
	if( !v->isEmpty() )
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
	map<string,string> by_path;
	map<string,string> by_id;
	getFindMap( "/dev/disk/by-path", by_path );
	getFindMap( "/dev/disk/by-id", by_id, false );
	while( (Entry=readdir( Dir ))!=NULL )
	    {
	    int Range=0;
	    unsigned long long Size = 0;
	    string SysfsFile = SysfsDir+"/"+Entry->d_name+"/range";
	    y2milestone( "autodetectDisks sysfsfile:%s", SysfsFile.c_str() );
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
	    string dn = Entry->d_name;
	    if( Range>1 && (Size>0||dn.find( "dasd" )==0) )
		{
		string::size_type pos = dn.find('!');
		while( pos!=string::npos )
		    {
		    dn[pos] = '/';
		    pos = dn.find('!',pos);
		    }
		y2milestone( "autodetectDisks disk sysfs:%s parted:%s",
		             Entry->d_name, dn.c_str() );
		Disk * d = NULL;
		if( dn.find( "dasd" )==0 )
		    d = new Dasd( this, dn, Size/2 );
		else
		    d = new Disk( this, dn, Size/2 );
		d->setUdevData( by_path[dn], by_id[dn] );
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
    SystemCmd Blkid( "BLKID_SKIP_CHECK_MDRAID=1 /sbin/blkid -c /dev/null" );
    SystemCmd Losetup( "/sbin/losetup -a" );
    ProcMounts Mounts;
    for( VolIterator i=begin; i!=end; ++i )
	{
	if( i->getUsedByType()==UB_NONE )
	    {
	    i->getLoopData( Losetup );
	    i->getFsData( Blkid );
	    if( detectMounted )
		i->getMountData( Mounts );
	    i->getFstabData( *fstab );
	    y2mil( "detect:" << *i );
	    }
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
	for( Container::ConstVolIterator j=vp.begin(); j!=vp.end(); ++j )
	    {
	    j->print( str );
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

void Storage::setDefaultMountBy( MountByType val )
    {
    y2mil( "val:" << Volume::mbyTypeString(val) );
    defaultMountBy = val;
    }

void Storage::setRootPrefix( const string& root )
    {
    y2milestone( "root:%s", root.c_str() );
    rootprefix = root;
    }

void Storage::setDetectMountedVolumes( bool val )
    {
    y2milestone( "val:%d", val );
    detectMounted = val;
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
	    {
	    ret = i->createPartition( type, start, size, device, true );
	    if( ret==0 && haveEvms() )
		{
		handleEvmsCreateDevice( disk, device, type==EXTENDED );
		}
	    }
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
	    if( ret==0 && haveEvms() )
		{
		handleEvmsCreateDevice( disk, device, type==EXTENDED );
		}
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
	    if( ret==0 && haveEvms() )
		{
		handleEvmsCreateDevice( disk, device );
		}
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
	    if( ret==0 && haveEvms() )
		{
		handleEvmsCreateDevice( disk, device );
		}
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
	    if( vol->getUsedByType() == UB_NONE || recursiveRemove )
		{
		if( vol->getUsedByType() != UB_NONE )
		    ret = removeUsing( vol->device(), vol->getUsedBy() );
		if( ret==0 )
		    ret = disk->removePartition( vol->nr() );
		if( ret==0 && cont->type()==DISK && haveEvms() )
		    {
		    handleEvmsRemoveDevice( disk, vol->device(),
		                            disk->isLogical(vol->nr()) );
		    }
		}
	    else
		ret = STORAGE_REMOVE_USED_VOLUME;
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
Storage::resizePartition( const string& partition, unsigned long sizeCyl )
    {
    int ret = 0;
    assertInit();
    y2milestone( "partition:%s newCyl:%lu", partition.c_str(), sizeCyl );
    VolIterator vol;
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol ) && cont->type()==DISK )
	{
	Disk* disk = dynamic_cast<Disk *>(&(*cont));
	Partition* p = dynamic_cast<Partition *>(&(*vol));
	if( disk!=NULL && p!=NULL )
	    {
	    ret = disk->resizePartition( p, sizeCyl );
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
Storage::forgetChangePartitionId( const string& partition )
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
	    ret = disk->forgetChangePartitionId( vol->nr() );
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
    y2milestone( "disk:%s label:%s", disk.c_str(), label.c_str() );
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

int
Storage::initializeDisk( const string& disk, bool value )
    {
    int ret = 0;
    assertInit();
    y2milestone( "disk:%s value:%d", disk.c_str(), value );
    DiskIterator i = findDisk( disk );

    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != dEnd() )
	{
	ret = i->initializeDisk( value );
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
    y2milestone( "ret:%d val:%d", ret, val );
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
Storage::setIgnoreFstab( const string& device, bool val )
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
	vol->setIgnoreFstab( val );
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
Storage::getIgnoreFstab( const string& device, bool& val )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s", device.c_str());
    VolIterator vol;
    ContIterator cont;
    if( findVolume( device, cont, vol ) )
	{
	val = vol->ignoreFstab();
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    y2milestone( "ret:%d val:%d", ret, val );
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
	ret = cont->resizeVolume( &(*vol), newSizeMb*1024 );
	eraseFreeInfo( vol->device() );
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
Storage::forgetResizeVolume( const string& device )
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
	vol->forgetResize();
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
	if( vol->getUsedByType() == UB_NONE || recursiveRemove )
	    {
	    if( vol->getUsedByType() != UB_NONE )
		ret = removeUsing( vol->device(), vol->getUsedBy() );
	    if( ret==0 )
		ret = cont->removeVolume( &(*vol) );
	    if( ret==0 && cont->type()==DISK && haveEvms() )
		{
		Disk* disk = dynamic_cast<Disk *>(&(*cont));
		string tmp = vol->device();
		string::size_type pos = tmp.find_last_not_of( "0123456789" );
		tmp.erase( 0, pos+1 );
		unsigned num = 0;
		if( !tmp.empty() )
		    tmp >> num;
		bool rename = disk!=NULL && num>0 && disk->isLogical(num);
		handleEvmsRemoveDevice( disk, vol->device(), rename );
		}
	    }
	else
	    ret = STORAGE_REMOVE_USED_VOLUME;
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
	if( ret==0 && !devs.empty() )
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
	ret = i->removeVg();
	if( ret==0 && i->created() )
	    ret = removeContainer( &(*i) );
	if( ret==0 && haveEvms() )
	    {
	    string evn = (string)"lvm2/" + name;
	    EvmsCoIterator e = findEvmsCo( evn );
	    y2mil( "n1:" << evn << " found:" << (e!=evCoEnd()) );
	    if( e==evCoEnd() )
		{
		evn = (string)"lvm/" + name;
		e = findEvmsCo( evn );
		y2mil( "n2:" << evn << " found:" << (e!=evCoEnd()) );
		}
	    if( e!=evCoEnd() )
		{
		removeContainer( &(*e) );
		}
	    }
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
Storage::changeLvStripeSize( const string& vg, const string& name,
			     unsigned long long stripeSize )
    {
    int ret = 0;
    assertInit();
    y2milestone( "vg:%s name:%s striepSize:%llu", vg.c_str(), name.c_str(),
                 stripeSize );
    LvmVgIterator i = findLvmVg( vg );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	ret = i->changeStripeSize( name, stripeSize );
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

int Storage::evmsActivate()
    {
    int ret = 0;
    assertInit();
    y2mil( "start" );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else
	{
	ConstEvmsPair p = evmsPair();
	if( !p.empty() )
	    y2warning( "evms already active:%u", p.length() );
	else
	    {
	    ret = EvmsCo::activateDevices();
	    if( ret==0 )
		{
		detectEvms();
		}
	    }
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
	ret = v->EvmsCo::setPeSize( peSizeK );
	if( ret==0 && !devs.empty() )
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
Storage::modifyEvmsContainer( const string& old_name, const string& new_name,
                              unsigned long long peSizeK, bool lvm1 )
    {
    int ret = 0;
    assertInit();
    y2mil( "old name:" << old_name << " new name:" << new_name <<
           " peSizeK:" << peSizeK << " lvm1:" << lvm1 );
    EvmsCoIterator i = findEvmsCo( old_name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( new_name.empty() ||
        new_name.find_first_of( "\"\' /\n\t:*?" ) != string::npos )
	{
	ret = STORAGE_EVMS_INVALID_NAME;
	}
    else if( i != evCoEnd() )
	{
	y2mil( "orig co:" << *i );
	ret = i->modifyCo( new_name, peSizeK, lvm1 );
	if( ret==0 )
	    {
	    y2mil( "modi co:" << *i );
	    Container *v = &(*i);
	    ret = removeContainer( v, false );
	    addToList( v );
	    }
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
	ret = i->removeCo();
	if( ret==0 && i->created() )
	    ret = removeContainer( &(*i) );
	if( ret==0 )
	    {
	    string lvn = name;
	    if( lvn.find( '/' )!=string::npos )
		lvn = lvn.substr( lvn.find( '/' )+1 );
	    LvmVgIterator l = findLvmVg( lvn );
	    if( l!=lvgEnd() )
		{
		removeContainer( &(*l) );
		}
	    }
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
Storage::changeEvmsStripeSize( const string& coname, const string& name,
                               unsigned long long stripeSize )

    {
    int ret = 0;
    assertInit();
    y2milestone( "co:%s name:%s", coname.c_str(), name.c_str() );
    EvmsCoIterator i = findEvmsCo( coname );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != evCoEnd() )
	{
	ret = i->changeStripeSize( name, stripeSize );
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

int Storage::extendMd( const string& name, const string& dev )
    {
    int ret = 0;
    assertInit();
    y2milestone( "name:%s dev:%s", name.c_str(), dev.c_str() );
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
	    ret = md->extendMd( num, dev );
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

int Storage::shrinkMd( const string& name, const string& dev )
    {
    int ret = 0;
    assertInit();
    y2milestone( "name:%s dev:%s", name.c_str(), dev.c_str() );
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
	    ret = md->shrinkMd( num, dev );
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

int Storage::changeMdType( const string& name, MdType rtype )
    {
    int ret = 0;
    assertInit();
    y2milestone( "name:%s rtype:%d", name.c_str(), rtype );
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
	    ret = md->changeMdType( num, rtype );
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

int Storage::changeMdChunk( const string& name, unsigned long chunk )
    {
    int ret = 0;
    assertInit();
    y2milestone( "name:%s dev:%lu", name.c_str(), chunk );
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
	    ret = md->changeMdChunk( num, chunk );
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

int Storage::changeMdParity( const string& name, MdParity ptype )
    {
    int ret = 0;
    assertInit();
    y2milestone( "name:%s dev:%d", name.c_str(), ptype );
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
	    ret = md->changeMdParity( num, ptype );
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

int Storage::checkMd( const string& name )
    {
    int ret = 0;
    assertInit();
    y2milestone( "name:%s", name.c_str() );
    unsigned num = 0;
    MdCo *md = NULL;
    if( Md::mdStringNum( name, num ) && haveMd(md) )
	ret = md->checkMd(num);
    else
	ret = STORAGE_MD_NOT_FOUND;
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
Storage::modifyFileLoop( const string& device, const string& lname, 
                         bool reuseExisting, unsigned long long sizeK )
    {
    int ret = 0;
    assertInit();
    y2milestone( "device:%s lname:%s reuse:%d sizeK:%lld", device.c_str(), 
                 lname.c_str(), reuseExisting, sizeK );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	LoopCo *loop = NULL;
	if( haveLoop(loop) )
	    ret = loop->updateLoop( device, lname, reuseExisting, sizeK );
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
	    list<commitAction*> l;
	    i->getCommitActions( l );
	    ac.splice( ac.end(), l );
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
	    delete *i;
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
	    return( static_cast<const Dm*>(rhs)->stripes() >
	            static_cast<const Dm*>(lhs)->stripes() );
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
                          list<Volume*>& vl, list<commitAction*>& todo )
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
    for( list<Container*>::const_iterator i=co.begin(); i!=co.end(); ++i )
	todo.push_back( new commitAction( stage, (*i)->type(), *i ));
    for( list<Volume*>::const_iterator i=vl.begin(); i!=vl.end(); ++i )
	todo.push_back( new commitAction( stage, (*i)->cType(), *i ));
    std::ostringstream b;
    b.str("");
    b << "unsorted actions <";
    for( list<commitAction*>::const_iterator i=todo.begin(); i!=todo.end(); ++i )
	{
	if( i!=todo.begin() )
	    b << " ";
	if( (*i)->container )
	    b << "C:" << (*i)->co()->device();
	else
	    b << "V:" << (*i)->vol()->device();
	}
    b << "> ";
    y2milestone( "%s", b.str().c_str() );
    todo.sort( cont_less<commitAction>() );
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
    b.str("");
    b << "sorted actions <";
    for( list<commitAction*>::const_iterator i=todo.begin(); i!=todo.end(); ++i )
	{
	if( i!=todo.begin() )
	    b << " ";
	if( (*i)->container )
	    b << "C:" << (*i)->co()->device();
	else
	    b << "V:" << (*i)->vol()->device();
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
    extendedError.clear();
    SystemCmd c;
    CPair p = cPair( notLoop );
    int ret = 0;
    y2milestone( "empty:%d", p.empty() );
    if( !p.empty() )
	{
	ret = commitPair( p, notLoop );
	}
    p = cPair( isLoop );
    y2milestone( "empty:%d", p.empty() );
    if( ret==0 && !p.empty() )
	{
	ret = commitPair( p, isLoop );
	}
    VPair vp = vPair( fstabAdded );
    for( VolIterator i=vp.begin(); i!=vp.end(); ++i )
	i->setFstabAdded(false);
    y2milestone( "ret:%d", ret );
    ContIterator cc;
    if( findContainer( "/dev/evms", cc ))
	logCo( &(*cc) );
    return( ret );
    }

int
Storage::commitPair( CPair& p, bool (* fnc)( const Container& ) )
    {
    int ret = 0;
    y2milestone( "p.length:%d", p.length() );
    CommitStage a[] = { DECREASE, INCREASE, FORMAT, MOUNT };
    CommitStage* pt = a;
    bool evms_activate = false;
    while( unsigned(pt-a) < lengthof(a) )
	{
	bool new_pair = false;
	list<Container*> colist;
	list<Volume*> vlist;
	ContIterator i = p.begin();
	while( ret==0 && i != p.end() )
	    {
	    ret = i->getToCommit( *pt, colist, vlist );
	    ++i;
	    }
#if 0
	if( *pt == FORMAT && instsys() )
	    {
	    activateHld( true );
	    }
#endif
	list<commitAction*> todo;
	sortCommitLists( *pt, colist, vlist, todo );
	list<commitAction*>::iterator ac = todo.begin();
	while( ret==0 && ac != todo.end() )
	    {
	    bool cont = (*ac)->container;
	    CType type = cont ? (*ac)->co()->type() : (*ac)->vol()->cType();
	    Container *co = cont ? (*ac)->co() : 
	                           const_cast<Container*>((*ac)->vol()->getContainer());
	    if( !evms_activate && *pt==INCREASE && (type==DISK||type==MD) )
		{
		evms_activate = true;
		}
	    if( !evms_activate && *pt==DECREASE && 
	        (type==LVM||type==DISK||type==MD) )
		{
		evms_activate = true;
		}
	    if( !evms_activate && (*pt==MOUNT||*pt==FORMAT) )
		{
		evms_activate = true;
		}
	    if( evms_activate && haveEvms() &&
	        ((*pt==INCREASE && type==EVMS)||*pt==FORMAT||*pt==MOUNT))
		{
		evmsActivateDevices();
		evms_activate = false;
		}
	    if( cont )
		{
		bool cont_removed = co->deleted() && (type==LVM||type==EVMS);
		ret = co->commitChanges( *pt );
		cont_removed = cont_removed && ret==0;
		if( cont_removed )
		    {
		    removeContainer( co );
		    new_pair = true;
		    }
		}
	    else
		{
		ret = co->commitChanges( *pt, (*ac)->vol() );
		}
	    delete( *ac );
	    ++ac;
	    }
	y2milestone( "stage:%d evms_activate:%d new_pair:%d", *pt,
	             evms_activate, new_pair );
	if( new_pair )
	    {
	    p = cPair( fnc );
	    new_pair = false;
	    }
	pt++;
	}
    if( evms_activate && haveEvms() )
	{
	// Todo schützen normale devices
	evmsActivateDevices();
	evms_activate = false;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

static bool needSaveFromEvms( const Container& c ) 
    { return( c.type()==DISK||c.type()==MD); }

void
Storage::evmsActivateDevices()
    {
    string tblname = "wrzlbrnft";
    SystemCmd c;
    list<Container*> co;
    list<Volume*> vol;
    CPair p = cPair( needSaveFromEvms );
    ContIterator i = p.begin();
    while( i != p.end() )
	{
	i->getToCommit( FORMAT, co, vol );
	i->getToCommit( MOUNT, co, vol );
	++i;
	}
    if( !vol.empty() )
	{
	vol.sort( sort_vol_normal );
	vol.unique();
	std::ostringstream b;
	b << "saved vol <";
	for( list<Volume*>::const_iterator i=vol.begin(); i!=vol.end(); ++i )
	    {
	    if( i!=vol.begin() )
		b << " ";
	    b << (*i)->device();
	    }
	b << "> ";
	y2milestone( "%s", b.str().c_str() );
	c.execute( "dmsetup remove " + tblname );
	string fname = tmpDir()+"/tfile";
	unlink( fname.c_str() );
	ofstream tfile( fname.c_str() );
	unsigned count=0;
	for( list<Volume*>::const_iterator i=vol.begin(); i!=vol.end(); ++i )
	    {
	    tfile << count*10 << " 10 linear " << (*i)->device() <<  " 0"
	          << endl;
	    count++;
	    }
	tfile.close();
	c.execute( "cat " + fname );
	c.execute( "dmsetup create " + tblname + " <" + fname );
	unlink( fname.c_str() );
	}
    EvmsCo::activateDevices();
    if( !vol.empty() )
	c.execute( "dmsetup remove " + tblname );
    }

static bool isDmContainer( const Container& co )
    {
    return( co.type()==EVMS || co.type()==DM || co.type()==LVM );
    }

void Storage::removeDmMapsTo( const string& dev )
    {
    y2milestone( "dev:%s", dev.c_str() );
    VPair vp = vPair( isDmContainer );
    for( VolIterator v=vp.begin(); v!=vp.end(); ++v )
	{
	Dm * dm = dynamic_cast<Dm *>(&(*v));
	if( dm!=NULL )
	    {
	    if( dm->mapsTo( dev ) )
		dm->removeTable();
	    }
	else
	    y2warning( "not a Dm descendant %s", v->device().c_str() );
	}
    }

void
Storage::getDiskList( bool (* CheckFnc)( const Disk& ), std::list<Disk*>& dl )
    {
    dl.clear();
    DiskPair dp = dPair( CheckFnc );
    DiskIterator i = dp.begin();
    while( i!=dp.end() )
	{
	y2mil( "disk:" << i->device() );
	dl.push_back( &(*i) );
	++i;
	}
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
	y2mil( "co:" << *i );
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
    struct FsCapabilitiesX : public FsCapabilities
    {
	FsCapabilitiesX (bool isExtendableX, bool isExtendableWhileMountedX,
			 bool isReduceableX, bool isReduceableWhileMountedX,
			 bool supportsUuidX, bool supportsLabelX,
			 bool labelWhileMountedX, unsigned int labelLengthX,
			 unsigned long long minimalFsSizeKX)
	    : FsCapabilities ()
	{
	    isExtendable = isExtendableX;
	    isExtendableWhileMounted = isExtendableWhileMountedX;
	    isReduceable = isReduceableX;
	    isReduceableWhileMounted = isReduceableWhileMountedX;
	    supportsUuid = supportsUuidX;
	    supportsLabel = supportsLabelX;
	    labelWhileMounted = labelWhileMountedX;
	    labelLength = labelLengthX;
	    minimalFsSizeK = minimalFsSizeKX;
	}
    };

    static FsCapabilitiesX reiserfsCaps (true, true, true, false, true, true,
					 false, 16, 50*1024);

    static FsCapabilitiesX ext2Caps (true, false, true, false, true, true,
				     true, 16, 16);

    static FsCapabilitiesX ext3Caps (true, false, true, false, true, true,
				     true, 16, 10*1024);

    static FsCapabilitiesX xfsCaps (true, true, false, false, true, true,
				    false, 12, 40*1024);

    static FsCapabilitiesX ntfsCaps (true, false, true, false, false, false,
				     false, 0, 10*1024);

    static FsCapabilitiesX fatCaps (true, false, true, false, false, false,
				    false, 0, 16);

    static FsCapabilitiesX swapCaps (true, false, true, false, false, true,
				     true, 16, 64);

    static FsCapabilitiesX jfsCaps (false, false, false, false, false, false,
				     false, 0, 10*1024);

    static FsCapabilitiesX hfsCaps (false, false, false, false, false, false,
				     false, 0, 10*1024);

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

	case JFS:
	    fscapabilities = jfsCaps;
	    return true;

	case HFS:
	    fscapabilities = hfsCaps;
	    return true;

	case SWAP:
	    fscapabilities = swapCaps;
	    return true;

	default:
	    return false;
    }
}

bool Storage::haveEvms()
    {
    bool ret = false;
    ContIterator c;
    if( findContainer( "/dev/evms", c ) && !c->isEmpty() )
	ret = true;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void Storage::handleEvmsRemoveDevice( const Disk* disk, const string& d,
                                      bool rename )
    {
    y2mil( "device:" << d << " rename:" << rename );
    ContIterator c;
    if( findContainer( "/dev/evms", c ))
	{
	VolIterator v;
	if( findVolume( d, v, true ) )
	    {
	    Partition* p = dynamic_cast<Partition *>(&(*v));
	    if( p && p->type()==EXTENDED )
		{
		string dname = p->disk()->name();
		DiskIterator d = findDisk( dname );
		if( d!=dEnd() )
		    {
		    Disk::PartPair dp = d->partPair();
		    Disk::PartIter pi = dp.begin();
		    while( pi!=dp.end() )
			{
			if( pi->type()==LOGICAL )
			    {
			    string dev = "/dev/evms/" + pi->name();
			    y2mil( "evms new dev:" << dev );
			    VolIterator vv;
			    if( findVolume( dev, vv ))
				{
				vv->setDeleted();
				vv->setSilent();
				}
			    }
			++pi;
			}
		    }
		}
	    }
	string dev = "/dev/evms/" + undevDevice(d);
	if( findVolume( dev, v ))
	    {
	    v->setDeleted();
	    v->setSilent();
	    y2mil( "v:" << *v );
	    }
	if( disk->isEmpty() && !findVolume( "/dev/evms/"+disk->name(), v) )
	    {
	    EvmsCo* co = dynamic_cast<EvmsCo *>(&(*c));
	    if( co != NULL )
		co->addLv( disk->sizeK(), disk->name(), false );
	    }
	if( rename )
	    {
	    string tmp( dev );
	    string::size_type pos = tmp.find_last_not_of( "0123456789" );
	    tmp.erase( 0, pos+1 );
	    y2mil( "tmp:" << tmp << " pos:" << pos );
	    unsigned num = 0;
	    if( !tmp.empty() )
		tmp >> num;
	    num++;
	    dev.erase( pos+1 );
	    while( findVolume( dev+decString(num), v ))
		{
		string tmp = dev+decString(num-1);
		y2mil( "bef rename:" << *v );
		v->rename( tmp.substr( dev.rfind( '/' )+1 ));
		y2mil( "aft rename:" << *v );
		num++;
		}
	    }
	logCo( &(*c) );
	}
    }


void Storage::handleEvmsCreateDevice( const string& disk, const string& d, bool extended )
    {
    y2mil( "disk:" << disk << " device:" << d << " ext:" << extended );
    ContIterator c;
    if( findContainer( "/dev/evms", c ) && c->type()==EVMS )
	{
	VolIterator v;
	VolIterator w;
	string dev;
	if( !extended )
	    {
	    dev = "/dev/evms/" + undevDevice(d);
	    if( findVolume( d, v ) && !findVolume( dev, w ))
		{
		EvmsCo* co = dynamic_cast<EvmsCo *>(&(*c));
		if( co != NULL )
		    {
		    string name = dev.substr( dev.rfind( '/' )+1 );
		    Evms* l = new Evms( *co, name, v->sizeK(), 1u );
		    co->addVolume( l );
		    y2mil( "l:" << *l );
		    if( findVolume( dev, w ))
			y2mil( "w:" << *w );
		    }
		}
	    logCo( &(*c) );
	    }
	dev = "/dev/evms/" + undevDevice(disk);
	if( findVolume( dev, v ))
	    {
	    v->setDeleted();
	    v->setSilent();
	    y2mil( "del evms disk:" << *v );
	    logCo( &(*c) );
	    }
	}
    }

void Storage::removeDmTableTo( const Volume& vol )
    {
    if( vol.cType()==DISK || vol.cType()==MD )
	{
	y2mil( "dev:" << vol.device() );
	removeDmMapsTo( vol.device() );
	if( vol.cType()==DISK )
	    removeDmMapsTo( vol.getContainer()->device() );
	removeDmTable( undevDevice( vol.device() ));
	}
    }
    
void Storage::removeDmTableTo( const string& device )
    {
    y2mil( "dev:" << device );
    VolIterator vol;
    if( findVolume( device, vol ))
	removeDmTableTo( *vol );
    }
    
void Storage::removeDmTable( const string& table )
    {
    SystemCmd c( "dmsetup table \"" + table + "\"" );
    if( c.retcode()==0 )
	{
	c.execute( "dmsetup remove \"" + table + "\"" );
	}
    }

void
Storage::logCo( Container* c )
    {
    std::ostringstream b;
    c->print( b );
    y2mil( "log co:" << b.str() );
    for( Container::ConstPlainIterator i=c->begin(); i!=c->end(); ++i )
	{
	b.str("");
	(*i)->print( b );
	y2mil( "log vo:" << b.str() );
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

bool Storage::findContainer( const string& device, ContIterator& c )
    {
    CPair cp = cPair();
    c = cp.begin();
    while( c!=cp.end() && c->device()!=device )
	++c;
    return( c!=cp.end() );
    }

bool Storage::findVolume( const string& device, VolIterator& v, bool also_del )
    {
    assertInit();
    string label;
    string uuid;
    string d;
    if( device.find( "LABEL=" )==0 )
	label = device.substr( 6 );
    else if( device.find( "UUID=" )==0 )
	uuid = device.substr( 5 );
    else
	d = normalizeDevice( device );
    if( !label.empty() || !uuid.empty() )
	y2milestone( "label:%s uuid:%s", label.c_str(), uuid.c_str() );
    VPair p = vPair( also_del?NULL:Volume::notDeleted );
    v = p.begin();
    if( label.empty() && uuid.empty() )
	{
	while( v!=p.end() && v->device()!=d )
	    {
	    const list<string>& al( v->altNames() );
	    if( find( al.begin(), al.end(), d )!=al.end() )
		break;
	    ++v;
	    }
	if( v==p.end() && d.find("/dev/loop")==0 )
	    {
	    v = p.begin();
	    while( v!=p.end() && v->loopDevice()!=d )
		++v;
	    }
	}
    else if( !label.empty() )
	{
	while( v!=p.end() && v->getLabel()!=label )
	    ++v;
	}
    else if( !uuid.empty() )
	{
	while( v!=p.end() && v->getUuid()!=uuid )
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
    CallbackProgressBar cb = getCallbackProgressBarTheOne();
    if( cb )
	(*cb)( id, cur, max );
    }

void Storage::showInfoCb( const string& info )
    {
    y2milestone( "INSTALL INFO:%s", info.c_str() );
    CallbackShowInstallInfo cb = getCallbackShowInstallInfoTheOne();
    lastAction = info;
    if( cb )
	(*cb)( info );
    }

void Storage::infoPopupCb( const string& info )
    {
    y2milestone( "INFO POPUP:%s", info.c_str() );
    CallbackInfoPopup cb = getCallbackInfoPopupTheOne();
    if( cb )
	(*cb)( info );
    }

bool Storage::yesnoPopupCb( const string& info )
    {
    y2milestone( "YESNO POPUP:%s", info.c_str() );
    CallbackYesNoPopup cb = getCallbackYesNoPopupTheOne();
    if( cb )
	return (*cb)( info );
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

bool Storage::isDisk( const string& dev )
    {
    return( findDisk( dev ) != dEnd() );
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

int Storage::removeContainer( Container* val, bool call_del )
    {
    y2milestone( "name:%s call_del:%d", val->name().c_str(), call_del );
    int ret = 0;
    CIter i=cont.begin();
    while( i!=cont.end() && *i!=val )
	++i;
    if( i!=cont.end() )
	{
	if( call_del )
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
    y2mil( "device:" << device << uby );
    string name = uby.name();
    int ret=0;
    switch( uby.type() )
	{
	case UB_MD:
	    ret = removeVolume( "/dev/" + name );
	    break;
	case UB_DM:
	    ret = removeVolume( "/dev/dm-" + name );
	    break;
	case UB_LVM:
	    ret = removeLvmVg( name );
	    break;
	case UB_EVMS:
	    if( !name.empty() )
		ret = removeEvmsContainer( name );
	    else
		ret = removeVolume( "/dev/evms/" + name );
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
	int ret = fstab->changeRootPrefix( root()+"/etc" );
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
    ProcMounts mountData;
    if( findVolume( device, vol ) )
	{
	mp = mountData.getMount( vol->mountDevice() );
	if( mp.empty() )
	    mp = mountData.getMount( vol->altNames() );
	ret = !mp.empty();
	}
    else
	{
	mp = mountData.getMount( device );
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
    bool ret = true;
    assertInit();
    y2milestone( "device:%s mp:%s", device.c_str(), mp.c_str() );
    VolIterator vol;
    if( !readonly && findVolume( device, vol ) )
	{
	if( vol->needLosetup() )
	    {
	    ret = vol->doLosetup()==0;
	    }
	if( ret )
	    {
	    ret = vol->mount( mp )==0;
	    }
	if( !ret )
	    vol->loUnsetup();
	}
    else
	ret = false;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

bool
Storage::readFstab( const string& dir, deque<VolumeInfo>& infos )
    {
    static deque<VolumeInfo> vil;
    vil.clear();
    bool ret = false;
    VolIterator vol;
    assertInit();
    y2milestone( "dir:%s", dir.c_str() );
    EtcFstab *fstab = new EtcFstab( dir, true );
    list<FstabEntry> le;
    fstab->getEntries( le );
    for( list<FstabEntry>::const_iterator i=le.begin(); i!=le.end(); ++i )
	{
	y2mil( "entry:" << *i );
	if( findVolume( i->dentry, vol ) )
	    {
	    VolumeInfo info;
	    vol->getInfo( info );
	    vol->mergeFstabInfo( info, *i );
	    y2mil( "volume:" << *vol );
	    vil.push_back( info );
	    }
	}
    delete fstab;
    infos = vil;
    ret = !infos.empty();
    y2milestone( "ret:%d", ret );
    return( ret );
    }

bool
Storage::getFreeInfo( const string& device, unsigned long long& resize_free,
		      unsigned long long& df_free,
		      unsigned long long& used, bool& win, bool use_cache )
    {
    bool ret = false;
    assertInit();
    resize_free = df_free = used = 0;
    y2milestone( "device:%s use_cache:%d", device.c_str(), use_cache );
    VolIterator vol;
    if( findVolume( device, vol ) )
	{
	if( use_cache && getFreeInfo( vol->device(), df_free, resize_free,
	                              used, win ))
	    {
	    ret = true;
	    }
	else
	    {
	    bool needUmount = false;
	    string mp;
	    if( !vol->isMounted() )
		{
		string mdir = tmpDir() + "/tmp_mp";
		unlink( mdir.c_str() );
		rmdir( mdir.c_str() );
		if( vol->getFs()!=FSUNKNOWN && mkdir( mdir.c_str(), 0700 )==0 &&
		    mountDevice( device, mdir ) )
		    {
		    needUmount = true;
		    mp = mdir;
		    }
		}
	    else
		mp = vol->getMount();
	    if( !mp.empty() )
		{
		struct statvfs fsbuf;
		ret = statvfs( mp.c_str(), &fsbuf )==0;
		if( ret )
		    {
		    df_free = fsbuf.f_bfree;
		    df_free *= fsbuf.f_bsize;
		    df_free /= 1024;
		    resize_free = df_free;
		    used = fsbuf.f_blocks-fsbuf.f_bfree;
		    used *= fsbuf.f_bsize;
		    used /= 1024;
		    y2milestone( "blocks:%lu free:%lu bsize:%lu", fsbuf.f_blocks,
				 fsbuf.f_bfree, fsbuf.f_bsize );
		    y2milestone( "free:%llu used:%llu", df_free, used );
		    }
		if( ret && vol->getFs()==NTFS )
		    {
		    SystemCmd c( "ntfsresize -f -i " + device );
		    string fstr = " might resize at ";
		    string::size_type pos;
		    if( c.retcode()==0 &&
			(pos=c.getString()->find( fstr ))!=string::npos )
			{
			y2milestone( "pos %zd", pos );
			pos = c.getString()->find_first_not_of( " \t\n", pos+fstr.size());
			y2milestone( "pos %zd", pos );
			string number = c.getString()->substr( pos,
							       c.getString()->find_first_not_of( "0123456789", pos ));
			y2milestone( "number \"%s\"", number.c_str() );
			unsigned long long t;
			number >> t;
			y2milestone( "number %llu", t );
			if( t-vol->sizeK()<resize_free )
			    resize_free = t-vol->sizeK();
			y2milestone( "resize_free %llu", t );
			}
		    else
			ret = false;
		    }
		win = false;
		char * files[] = { "boot.ini", "msdos.sys", "io.sys",
				   "config.sys", "MSDOS.SYS", "IO.SYS" };
		string f;
		unsigned i=0;
		while( !win && i<lengthof(files) )
		    {
		    f = mp + "/" + files[i];
		    win = access( f.c_str(), R_OK )==0;
		    i++;
		    }
		}
	    if( needUmount )
		{
		umountDevice( device );
		rmdir( mp.c_str() );
		rmdir( tmpDir().c_str() );
		}

	    if( vol->needLosetup() && vol->doLosetup() )
		{
		ret = vol->mount( mp )==0;
		if( !ret )
		    vol->loUnsetup();
		}
	    setFreeInfo( vol->device(), df_free, resize_free, used, win );
	    }
	}
    if( ret )
	y2milestone( "resize_free:%llu df_free:%llu used:%llu",
	             resize_free, df_free, used );
    y2milestone( "ret:%d win:%d", ret, win );
    return( ret );
    }

void Storage::setFreeInfo( const string& device, unsigned long long df_free,
                           unsigned long long resize_free,
			   unsigned long long used, bool win )
    {
    y2milestone( "device:%s df_free:%llu resize_free:%llu used:%llu win:%d",
		 device.c_str(), df_free, resize_free, used, win );

    FreeInfo inf( df_free, resize_free, used, win );
    freeInfo[device] = inf;
    }

bool
Storage::getFreeInfo( const string& device, unsigned long long& df_free,
		      unsigned long long& resize_free,
		      unsigned long long& used, bool& win )
    {
    map<string,FreeInfo>::iterator i = freeInfo.find( device );
    bool ret = i!=freeInfo.end();
    if( ret )
	{
	df_free = i->second.df_free;
	resize_free = i->second.resize_free;
	used = i->second.used;
	win = i->second.win;
	}
    y2milestone( "device:%s ret:%d", device.c_str(), ret );
    if( ret )
	y2milestone( "df_free:%llu resize_free:%llu used:%llu win:%d",
		     df_free, resize_free, used, win );
    return( ret );
    }

void
Storage::eraseFreeInfo( const string& device )
    {
    map<string,FreeInfo>::iterator i = freeInfo.find( device );
    if( i!=freeInfo.end() )
	freeInfo.erase(i);
    }

int
Storage::createBackupState( const string& name )
    {
    int ret = readonly?STORAGE_CHANGE_READONLY:0;
    assertInit();
    y2milestone( "name:%s", name.c_str() );
    if( ret==0 )
	{
	if(checkBackupState(name))
	    removeBackupState( name );
	CCIter i=cont.begin();
	while( i!=cont.end() )
	    {
	    backups[name].push_back( (*i)->getCopy() );
	    ++i;
	    }
	}
    y2mil( "states:" << backupStates() );
    y2milestone( "ret:%d", ret );
    if( ret==0 )
	y2milestone( "comp:%d", equalBackupStates( name, "", true ));
    return( ret );
    }

int
Storage::removeBackupState( const string& name )
    {
    int ret = readonly?STORAGE_CHANGE_READONLY:0;
    assertInit();
    y2milestone( "name:%s", name.c_str() );
    if( ret==0 )
	{
	if( !name.empty() )
	    {
	    map<string,CCont>::iterator i = backups.find( name );
	    if( i!=backups.end())
		{
		deleteClist( i->second );
		backups.erase(i);
		}
	    else
		ret = STORAGE_BACKUP_STATE_NOT_FOUND;
	    }
	else
	    deleteBackups();
	}
    y2mil( "states:" << backupStates() );
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
Storage::restoreBackupState( const string& name )
    {
    int ret = readonly?STORAGE_CHANGE_READONLY:0;
    assertInit();
    y2milestone( "name:%s", name.c_str() );
    if( ret==0 )
	{
	map<string,CCont>::iterator b = backups.find( name );
	if( b!=backups.end())
	    {
	    cont.clear();
	    CCIter i=b->second.begin();
	    while( i!=b->second.end() )
		{
		cont.push_back( (*i)->getCopy() );
		++i;
		}
	    }
	else
	    ret = STORAGE_BACKUP_STATE_NOT_FOUND;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

bool
Storage::checkBackupState( const string& name )
    {
    bool ret = false;
    assertInit();
    y2milestone( "name:%s", name.c_str() );
    map<string,CCont>::iterator i = backups.find( name );
    ret = i!=backups.end();
    y2milestone( "ret:%d", ret );
    return( ret );
    }

struct equal_name
    {
    equal_name( const string& n ) : val(n) {};
    bool operator()(const Container* c) { return( c->name()==val ); }
    const string& val;
    };

bool
Storage::equalBackupStates( const string& lhs, const string& rhs,
                            bool verbose_log ) const
    {
    y2milestone( "lhs:%s rhs:%s verbose:%d", lhs.c_str(), rhs.c_str(),
                 verbose_log );
    map<string,CCont>::const_iterator i;
    const CCont* l = NULL;
    const CCont* r = NULL;
    if( lhs.empty() )
	l = &cont;
    else
	{
	i = backups.find( lhs );
	if( i!=backups.end() )
	    l = &i->second;
	}
    if( rhs.empty() )
	r = &cont;
    else
	{
	i = backups.find( rhs );
	if( i!=backups.end() )
	    r = &i->second;
	}
    bool ret = (l==NULL&&r==NULL) || (r!=NULL&&l!=NULL);
    if( ret && r!=NULL && l!=NULL )
	{
	CCIter i=l->begin();
	CCIter j;
	while( (ret||verbose_log) && i!=l->end() )
	    {
	    j = find_if( r->begin(), r->end(), equal_name( (*i)->name()) );
	    if( j!=r->end() )
		ret = (*i)->compareContainer( *j, verbose_log ) && ret;
	    else
		{
		ret = false;
		if( verbose_log )
		    y2mil( "container -->" << (**i) );
		}
	    ++i;
	    }
	i=r->begin();
	while( (ret||verbose_log) && i!=r->end() )
	    {
	    j = find_if( l->begin(), l->end(), equal_name( (*i)->name()) );
	    if( j==r->end() )
		{
		ret = false;
		if( verbose_log )
		    y2mil( "container <--" << (**i) );
		}
	    ++i;
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string
Storage::backupStates() const
    {
    string ret;
    map<string,CCont>::const_iterator i = backups.begin();
    for( map<string,CCont>::const_iterator i = backups.begin();
         i!=backups.end(); ++i )
	{
	if( i!=backups.begin() )
	    ret += ',';
	ret += i->first;
	}

    return( ret );
    }

void
Storage::activateHld( bool val )
    {
    y2milestone( "val:%d", val );
    if( val )
	{
	Dm::activate(val);
	MdCo::activate(val);
	}
    LvmVg::activate(val);
    EvmsCo::activate(val);
    if( !val )
	{
	Dm::activate(val);
	MdCo::activate(val);
	}
    }

int Storage::addFstabEntry( const string& device, const string& mount,
                            const string& vfs, const string& options,
			    unsigned freq, unsigned passno )
    {
    int ret = readonly?STORAGE_CHANGE_READONLY:0;
    assertInit();
    y2milestone( "device:%s mount:%s vfs:%s opts:%s freq:%u passno:%u",
                 device.c_str(), mount.c_str(), vfs.c_str(), options.c_str(),
		 freq, passno );
    if( ret==0 && (device.empty()||mount.empty()||vfs.empty()))
	{
	ret = STORAGE_INVALID_FSTAB_VALUE;
	}
    if( ret==0 && fstab==NULL )
	{
	ret = STORAGE_NO_FSTAB_PTR;
	}
    if( ret==0 )
	{
	FstabChange c;
	c.device = c.dentry = device;
	c.mount = mount;
	c.fs = vfs;
	if( options.empty() )
	    c.opts.push_back( "defaults" );
	else
	    c.opts = splitString( options, "," );
	c.freq = freq;
	c.passno = passno;
	fstab->addEntry( c );
	if( isRootMounted() )
	    {
	    string dir = root() + mount;
	    if( access( dir.c_str(), R_OK )!=0 )
		createPath( dir );
	    ret = fstab->flush();
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void Storage::setExtError( const string& txt )
    {
    extendedError = txt;
    }

int Storage::waitForDevice() const
    {
    int ret = 0;
    char * prog = "/usr/bin/udev.count_events";
    if( access( prog, X_OK )==0 )
	{
	y2milestone( "calling prog:%s", prog );
	SystemCmd c( prog );
	y2milestone( "returned prog:%s retcode:%d", prog, c.retcode() );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Storage::waitForDevice( const string& device ) const
    {
    int ret = 0;
    waitForDevice();
    bool exist = access( device.c_str(), R_OK )==0;
    y2milestone( "device:%s exist:%d", device.c_str(), exist );
    if( !exist )
	{
	unsigned count=0;
	while( !exist && count<500 )
	    {
	    usleep( 10000 );
	    exist = access( device.c_str(), R_OK )==0;
	    count++;
	    }
	y2milestone( "device:%s exist:%d", device.c_str(), exist );
	}
    if( !exist )
	ret = STORAGE_DEVICE_NODE_NOT_FOUND;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

namespace storage
{
std::ostream& operator<< (std::ostream& s, Storage &v )
    {
    v.printInfo(s);
    return(s);
    }
}



Storage::SkipDeleted Storage::SkipDel;

storage::CallbackProgressBar Storage::progress_bar_cb_ycp;
storage::CallbackShowInstallInfo Storage::install_info_cb_ycp;
storage::CallbackInfoPopup Storage::info_popup_cb_ycp;
storage::CallbackYesNoPopup Storage::yesno_popup_cb_ycp;

