#include <dirent.h>
#include <glob.h>

#include <fstream>

#include <ycp/y2log.h>
#include <sys/utsname.h>

#include "y2storage/Storage.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/Disk.h"
#include "y2storage/LvmVg.h"
#include "y2storage/IterPair.h"
#include "y2storage/ProcMounts.h"
#include "y2storage/EtcFstab.h"
#include "y2storage/AsciiFile.h"

struct Larger150 { bool operator()(const Disk&d) const {return(d.cylinders()>150);}};

bool TestLg150( const Disk& d )
    { return(d.cylinders()>150); };
bool TestCG30( const Partition& d )
    { return(d.cylSize()>30); };


Storage::Storage( bool ronly, bool tmode, bool autodetec ) :
    readonly(ronly), testmode(tmode), initialized(false), autodetect(autodetec)
    {
    y2milestone( "constructed Storage ronly:%d testmode:%d autodetect:%d",
                 ronly, testmode, autodetect );
    char * tenv = getenv( "YAST_IS_RUNNING" );
    inst_sys = tenv!=NULL && strcmp(tenv,"instsys")==0;
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

    detectDisks();
    detectLvmVgs();

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
    if( tempdir.size()>0 && access( tempdir.c_str(), R_OK )==0 )
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
	    addToList( new LvmVg( this, *i ) );
	if( instsys() )
	    LvmVg::activate( false );
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
	if( i->getUsedBy() != UB_NONE )
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
    y2milestone( "ret:%d device:%s", ret, ret?device.c_str():"" );
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
	if( i->getUsedBy() != UB_NONE )
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
    y2milestone( "ret:%d device:%s", ret, ret?device.c_str():"" );
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
Storage::defaultDiskLabel()
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
    y2milestone( "ret:%d mount:%s", ret, mount.c_str() );
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
    ContIterator cont;
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
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
	ret = vol->resize( newSizeMb );
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
    std::ostringstream buf;
    buf << "name:" << name << " peSizeK:" << peSizeK << " lvm1:" << lvm1
        << " devices:" << devs;
    y2milestone( "%s", buf.str().c_str() );
    LvmVgIterator i = findLvmVg( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( name.find_first_of( "\"\' /\n\t:*?" ) != string::npos )
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
	    copy( devs.begin(), devs.end(), d.begin() );
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
    std::ostringstream buf;
    buf << "name:" << name << " devices:" << devs;
    y2milestone( "%s", buf.str().c_str() );
    LvmVgIterator i = findLvmVg( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	list<string> d;
	copy( devs.begin(), devs.end(), d.begin() );
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
    std::ostringstream buf;
    buf << "name:" << name << " devices:" << devs;
    y2milestone( "%s", buf.str().c_str() );
    LvmVgIterator i = findLvmVg( name );
    if( readonly )
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	list<string> d;
	copy( devs.begin(), devs.end(), d.begin() );
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
    y2milestone( "ret:%d device:%s", ret, ret?device.c_str():"" );
    return( ret );
    }

int
Storage::removeLvmLv( const string& device )
    {
    int ret = 0;
    string vg, name;
    string d( device );
    if( d.find( "/dev/" )==0 )
	d.erase( 0, 5 );
    string::size_type pos = d.find( '/' );
    if( pos!=string::npos )
	{
	vg = d.substr( 0, pos );
	name = d.substr( pos+1 );
	}
    if( vg.size()>0 && name.size()>0 )
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
    y2milestone( "ret.size():%d", ret.size() );
    return( ret );
    }

int Storage::commit()
    {
    assertInit();
    CPair p = cPair();
    int ret = 0;
    y2milestone( "empty:%d", p.empty() );
    if( !p.empty() )
	{
	CommitStage a[] = { DECREASE, INCREASE, FORMAT, MOUNT };
	CommitStage* pt = a;
	while( unsigned(pt-a) < sizeof(a)/sizeof(a[0]) )
	    {
	    if( *pt==DECREASE )
		{
		ContIterator i = p.end();
		if( p.begin()!=p.end() )
		    {
		    do
			{
			if( i!=p.begin() )
			    --i;
			ret = i->commitChanges( *pt );
			y2milestone( "stage %d ret %d", *pt, ret );
			}
		    while( ret==0 && i != p.begin() );
		    }
		}
	    else
		{
		ContIterator i = p.begin();
		while( ret==0 && i != p.end() )
		    {
		    ret = i->commitChanges( *pt );
		    y2milestone( "stage %d ret %d", *pt, ret );
		    ++i;
		    }
		}
	    pt++;
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

bool
Storage::getDisks (deque<string>& disks)
{
    disks.clear ();
    assertInit();

    for (ConstDiskIterator i = diskBegin(); i != diskEnd(); ++i)
	disks.push_back (i->device());

    return true;
}


bool
Storage::getPartitions (const string& disk, deque<PartitionInfo>& partitioninfos)
{
    partitioninfos.clear ();
    assertInit();
    DiskIterator i = findDisk( disk );

    if( i != dEnd() )
    {
	Disk::PartPair p = i->partPair (Disk::notDeleted);

	for (Disk::PartIter i2 = p.begin(); i2 != p.end(); ++i2)
	    partitioninfos.push_back (i2->getPartitionInfo());
    }

    return( i != dEnd() );
}

bool
Storage::getPartitions (deque<PartitionInfo>& partitioninfos)
{
    partitioninfos.clear ();
    assertInit();

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
    string d( device );
    if( d.find( "/dev/" )!=0 )
	d = "/dev/" + d;
    VPair p = vPair( Volume::notDeleted );
    v = p.begin();
    const list<string>& al( v->altNames() );
    while( v!=p.end() && v->device()!=d && 
           find( al.begin(), al.end(), d )==al.end() )
	{
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
	    i->setUsedBy( typ, name );
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

void Storage::progressBarCb( const string& id, unsigned cur, unsigned max )
    {
    y2milestone( "id:%s cur:%d max:%d", id.c_str(), cur, max );
    if( progress_bar_cb )
	(*progress_bar_cb)( id, cur, max );
    }

void Storage::showInfoCb( const string& info )
    {
    y2milestone( "INSTALL INFO:%s", info.c_str() );
    if( install_info_cb )
	(*install_info_cb)( info );
    }

Storage::DiskIterator Storage::findDisk( const string& disk )
    {
    assertInit();
    string d( disk );
    if( d.find( "/dev/" )!=0 )
	d = "/dev/" + d;
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
    while( ret != p.end() && ret->name()!=name )
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

bool Storage::canUseDevice( const string& dev, bool disks_allowed )
    {
    bool ret=true;
    VolIterator v;
    if( !findVolume( dev, v ) )
	{
	if( disks_allowed )
	    {
	    DiskIterator i = findDisk( dev );
	    ret = i!=dEnd() && i->getUsedBy()==UB_NONE && i->numPartitions()==0;
	    }
	else
	    ret = false;
	}
    else
	ret = v->getUsedBy()==UB_NONE && v->getMount().size()==0;
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


Storage::SkipDeleted Storage::SkipDel;
