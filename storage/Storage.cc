/*
 * Copyright (c) [2004-2010] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#include <dirent.h>
#include <glob.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <errno.h>
#include <set>
#include <array>
#include <fstream>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include "config.h"
#include "storage/Storage.h"
#include "storage/StorageTmpl.h"
#include "storage/AppUtil.h"
#include "storage/SystemCmd.h"
#include "storage/Disk.h"
#include "storage/Dasd.h"
#include "storage/MdCo.h"
#include "storage/MdPartCo.h"
#include "storage/DmCo.h"
#include "storage/LoopCo.h"
#include "storage/LvmVg.h"
#include "storage/IterPair.h"
#include "storage/SystemInfo.h"
#include "storage/ProcMounts.h"
#include "storage/ProcParts.h"
#include "storage/Blkid.h"
#include "storage/EtcFstab.h"
#include "storage/EtcRaidtab.h"
#include "storage/AsciiFile.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    void
    initDefaultLogger()
    {
    string path;
    string file;
    if (geteuid ())
    {
	struct passwd* pw = getpwuid (geteuid ());
	if (pw)
	{
	    path = pw->pw_dir;
	    file = ".y2log";
	}
	else
	{
	    path = "/";
	    file = "y2log";
	}
    }
    else
    {
	path = "/var/log/YaST2";
	file = "y2log";
    }

	createLogger("default", path, file);
    }


    std::ostream& operator<<(std::ostream& s, const Environment& env)
    {
	return s << "readonly:" << env.readonly << " testmode:" << env.testmode 
		 << " autodetect:" << env.autodetect << " instsys:" << env.instsys
		 << " logdir:" << env.logdir << " testdir:" << env.testdir;
    }


Storage::Storage(const Environment& env)
    : env(env), lock(readonly(), testmode()), initialized(false), fstab(NULL), raidtab(NULL)
{
    y2mil("constructed Storage with " << env);
    y2mil("libstorage version " VERSION);

    root_mounted = !instsys();
    efiboot = false;
    hald_pid = 0;

    max_log_num = 5;
    char* tenv = getenv("Y2MAXLOGNUM");
    if( tenv!=0 )
	string(tenv) >> max_log_num;
    y2mil("max_log_num:" << max_log_num);

    progress_bar_cb = NULL;
    install_info_cb = NULL;
    info_popup_cb = NULL;
    yesno_popup_cb = NULL;
    commit_error_popup_cb = NULL;
    password_popup_cb = NULL;

    recursiveRemove = false;
    zeroNewPartitions = false;
    defaultMountBy = MOUNTBY_ID;
    defaultFs = EXT4;
    detectMounted = true;

    SystemCmd::setTestmode(testmode());

    imsm_driver = IMSM_UNDECIDED;

    logSystemInfo();
}


void
Storage::logSystemInfo() const
{
    if (!testmode())
    {
    AsciiFile("/proc/version").logContent();
    SystemCmd(LSBIN " -1 /lib/modules");
    }
}


void
Storage::initialize()
    {
    initialized = true;
    char tbuf[32] = "/tmp/libstorage-XXXXXX";
    if( mkdtemp( tbuf )==NULL )
	{
	y2err("tmpdir creation " << tbuf << " failed. Aborting...");
	exit(EXIT_FAILURE);
	}
    else
	{
	tempdir = tbuf;
	}

    bindtextdomain("libstorage", "/usr/share/locale");
    bind_textdomain_codeset("libstorage", "UTF-8");

    if (access(SYSCONFIGFILE, R_OK) == 0)
    {
	AsciiFile sc(SYSCONFIGFILE);
	const vector<string>& lines = sc.lines();

	Regex rx1('^' + Regex::ws + "DEVICE_NAMES" + '=' + "(['\"]?)([^'\"]*)\\1" + Regex::ws + '$');
	if (find_if(lines, regex_matches(rx1)) != lines.end())
	{
	    string val = boost::to_lower_copy(rx1.cap(2), locale::classic());
	    if( val == "id" )
		setDefaultMountBy( MOUNTBY_ID );
	    else if( val == "path" )
		setDefaultMountBy( MOUNTBY_PATH );
	    else if( val == "device" )
		setDefaultMountBy( MOUNTBY_DEVICE );
	    else if( val == "uuid" )
		setDefaultMountBy( MOUNTBY_UUID );
	    else if( val == "label" )
		setDefaultMountBy( MOUNTBY_LABEL );
	    else
		y2war("unknown default mount-by method '" << val << "' in " SYSCONFIGFILE);
	}

	Regex rx2('^' + Regex::ws + "DEFAULT_FS" + '=' + "(['\"]?)([^'\"]*)\\1" + Regex::ws + '$');
	if (find_if(lines, regex_matches(rx2)) != lines.end())
	{
	    string val = boost::to_lower_copy(rx2.cap(2), locale::classic());
	    if (val == "ext2")
		setDefaultFs(EXT2);
	    else if (val == "ext3")
		setDefaultFs(EXT3);
	    else if (val == "ext4")
		setDefaultFs(EXT4);
	    else if (val == "reiser")
		setDefaultFs(REISERFS);
	    else if (val == "xfs")
		setDefaultFs(XFS);
	    else
		y2war("unknown default filesystem '" << val << "' in " SYSCONFIGFILE);
	}
    }

    if (testmode())
    {
	string t = testdir() + "/arch.info";
	if (access(t.c_str(), R_OK) == 0)
	    readArchInfo(t);
	efiboot = (arch() == "ia64");
    }
    else if (autodetect())
    {
	detectArch();
	efiboot = (arch() == "ia64");
    }

    detectObjects();
    setCacheChanges( true );

    for (list<std::pair<string, Text>>::const_iterator i = infoPopupTxts.begin(); 
	 i != infoPopupTxts.end(); ++i)
	{
	if (!isUsedBy(i->first))
	    infoPopupCb( i->second );
	else
	    y2mil( "suppressing cb:" << i->second.native );
	}
    logProcData();
    }


void Storage::dumpObjectList()
{
    assertInit();
    ostringstream buf;
    classic(buf);
    printInfo(buf);
    std::list<string> l = splitString( buf.str(), "\n" );
    y2mil("DETECTED OBJECTS BEGIN");
    for (std::list<string>::const_iterator i = l.begin(); i != l.end(); i++)
	y2mil(*i);
    y2mil("DETECTED OBJECTS END");
}


void Storage::detectObjects()
{
	if (instsys())
	{
	    if( discoverMdPVols() == true )
	    {
		// if 'yes' then activate md prior to dm
		MdPartCo::activate( true, tmpDir() );
		waitForDevice();
	    }
	    //Note:
	    //dmraid will not activate devices that were activated by
	    //mdadm. So this is safe.

	    DmraidCo::activate(true);
	    MdCo::activate(true, tmpDir());
	    LvmVg::activate(true);
	}

    danglingUsedBy.clear();

    SystemInfo systeminfo;

    detectDisks(systeminfo);
    detectDmraid(systeminfo);
    detectDmmultipath(systeminfo);
    detectMdParts(systeminfo);
    detectMds();
    detectDm(systeminfo, true);
    detectLvmVgs();
    detectDm(systeminfo, false);
    delete fstab;
    if (testmode())
	{
 	rootprefix = testdir();
 	fstab = new EtcFstab( rootprefix );
	raidtab = new EtcRaidtab(rootprefix);
	}
    else
    {
	fstab = new EtcFstab( "/etc", isRootMounted() );
	if (!instsys())
	    raidtab = new EtcRaidtab(root());
    }
    
    detectLoops(systeminfo);
    if( !instsys() )
	detectNfs(*fstab, systeminfo);

    if (!danglingUsedBy.empty())
	y2err("dangling used-by left after detection: " << danglingUsedBy);
    
    LvmVgPair p = lvgPair();
    y2mil( "p length:" << p.length() );
    for( LvmVgIterator i=p.begin(); i!=p.end(); ++i )
	i->normalizeDmDevices();

    if (testmode())
        {
	string t = testdir() + "/volume.info";
	if( access( t.c_str(), R_OK )==0 )
	    detectFsDataTestMode( t, vBegin(), vEnd() );

	t = testdir() + "/free.info";
	if( access( t.c_str(), R_OK )==0 )
	    readFreeInfo(t);
	}
    else
	{
	detectFsData(vBegin(), vEnd(), systeminfo);
	logContainersAndVolumes(logdir());
	}

    dumpObjectList();

    if( instsys() )
	{
	SystemCmd c(GREPBIN " ^md.*dm- /proc/mdstat");
	SystemCmd rm;
	for( unsigned i=0; i<c.numLines(); i++ )
	    {
	    rm.execute(MDADMBIN " --stop " + quote("/dev/" + extractNthWord(0, c.getLine(i))));
	    }
	}
    }


bool
Storage::discoverMdPVols()
{
  if( !instsys() )
    {
    return false;
    }
  string mdDevs = "";
  bool ret = MdPartCo::isImsmPlatform();
  if( ret == true )
    {
    y2mil("Intel SW RAID Platform detected.");

    list <string> l;
    if( MdPartCo::scanForRaid(l) != 0 )
      {
      MdPartCo::activate( true, tmpDir() );
      waitForDevice();
      l = MdPartCo::getMdRaids();
      MdPartCo::activate( false, "" );
      }
    if (!l.empty())
      {
      // At least ONE Volume must be detected
      mdDevs.clear();
      for( list<string>::const_iterator i=l.begin(); i!=l.end(); ++i )
        {
        mdDevs += " " + *i;
        }
      y2mil(" md raids:" + mdDevs);
      if( !mdDevs.empty())
        {
	if (getImsmDriver() == IMSM_UNDECIDED)
	{
        Text txt = sformat(
            // popup text %1$s is replaced by disk name e.g. /dev/hda
            _("You are running on the Intel(R) Matrix Storage Manager compatible platform.\n"
                "\n"
                "Following MD compatible RAID devices were detected:\n"
                "%1$s\n"
                "If they are clean devices or contain partitions then you can choose to use\n"
                "MD Partitionable RAID sysbsystem to handle them. In case of clean device you\n"
                "will be able to install system on it and boot from such RAID.\n"
                "Do you want MD Partitionable RAID subsystem to manage those partitions?"
            ), mdDevs.c_str() );

        if( yesnoPopupCb(txt) )
          {
          ret = true;
          setImsmDriver(IMSM_MDADM);
          }
        else
          {
          ret = false;
	  setImsmDriver(IMSM_DMRAID);
          }
	}
	else
	{
	    ret = getImsmDriver() == IMSM_MDADM;
	}
        }
      else
        {
        /* No mdDevs */
        ret = false;
        }
      }
    else
      {
      /* No RAIDs at all */
      ret = false;
      }
    }
  y2mil(" Exiting with status: " << ret);
  return ret;
}


void Storage::deleteBackups()
    {
    for (map<string, CCont>::iterator i = backups.begin(); i != backups.end(); ++i)
	clearPointerList(i->second);
    backups.clear();
    }

Storage::~Storage()
    {
	logContainersAndVolumes(logdir());
    clearPointerList(cont);
    deleteBackups();
    if (!tempdir.empty() && rmdir(tempdir.c_str()) != 0)
    {
	y2err("stray tmpfile");
	SystemCmd(LSBIN " -l " + quote(tempdir));
    }
    delete fstab;
    y2mil("destructed Storage");
    }

void Storage::rescanEverything()
    {
    y2mil("rescan everything");
    clearPointerList(cont);
    detectObjects();
    }


    string
    Storage::bootMount() const
    {
	if (efiBoot())
	    return "/boot/efi";
	else
	    return "/boot";
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
	else if( strncmp( buf.machine, "x86_64", 5 )==0 )
	    {
	    proc_arch = "x86_64";
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
    if( proc_arch == "ppc" )
	{
	AsciiFile cpu( "/proc/cpuinfo" );
	int l = cpu.find_if_idx(string_starts_with("machine\t"));
	y2mil("line:" << l);
	if( l >= 0 )
	    {
	    string line = cpu[l];
	    line = extractNthWord( 2, line );
	    y2mil("line:" << line);
	    is_ppc_mac = line.find( "PowerMac" )==0 || line.find( "PowerBook" )==0;
	    is_ppc_pegasos = line.find( "EFIKA5K2" )==0;
	    if( is_ppc_mac == 0 && is_ppc_pegasos == 0 )
		{
		line = cpu[l];
		line = extractNthWord( 3, line );
		y2mil("line:" << line);
		is_ppc_pegasos = line.find( "Pegasos" )==0;
		}
	    }
	}
    y2mil("Arch:" << proc_arch << " IsPPCMac:" << is_ppc_mac << " IsPPCPegasos:" << is_ppc_pegasos);
    }


void
    Storage::detectDisks(SystemInfo& systeminfo)
    {
    if (testmode())
	{
	    const list<string> l = glob(testdir() + "/disk_*.info", GLOB_NOSORT);
	    for (list<string>::const_iterator i = l.begin(); i != l.end(); ++i)
		addToList(new Disk(this, AsciiFile(*i)));
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_DISK") == NULL)
	{
	    autodetectDisks(systeminfo);
	}
    }


void
Storage::detectMdParts(SystemInfo& systeminfo)
{
  if( testmode() )
    {
    }
  else
    {
    list<string> l = MdPartCo::getMdRaids();
    list<string> mdpartlist = MdPartCo::filterMdPartCo(l, systeminfo.getProcParts(), instsys());

    map<string, list<string>> by_id = getUdevMap("/dev/disk/by-id");
    for(list<string>::const_iterator i = mdpartlist.begin(); i != mdpartlist.end(); ++i)
      {
	  MdPartCo* v = new MdPartCo(this, *i, systeminfo);
      list<string> nm = by_id[v->name()];
      if( !nm.empty() )
        {
        v->setUdevData(nm);
        }
      addToList( v );
      }
    }
}

void Storage::detectMds()
    {
    if (testmode())
	{
	    string file = testdir() + "/md.info";
	if( access( file.c_str(), R_OK )==0 )
	    {
	    addToList(new MdCo(this, AsciiFile(file)));
	    }
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_MDRAID") == NULL)
	{
	MdCo * v = new MdCo( this, true );
	if( !v->isEmpty() )
	    addToList( v );
	else
	    delete v;
	}
    }

    void
    Storage::detectLoops(SystemInfo& systeminfo)
    {
    if (testmode())
	{
	    string file = testdir() + "/loop.info";
	if( access( file.c_str(), R_OK )==0 )
	    {
	    addToList(new LoopCo(this, AsciiFile(file)));
	    }
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_LOOP") == NULL)
	{
	    LoopCo* v = new LoopCo(this, true, systeminfo);
	if( !v->isEmpty() )
	    addToList( v );
	else
	    delete v;
	}
    }


    void
    Storage::detectNfs(const EtcFstab& fstab, SystemInfo& systeminfo)
    {
    if (testmode())
	{
	    string file = testdir() + "/nfs.info";
	if( access( file.c_str(), R_OK )==0 )
	    {
	    addToList(new NfsCo(this, AsciiFile(file)));
	    }
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_NFS") == NULL)
	{
	NfsCo* v = new NfsCo(this, fstab, systeminfo);
	if( !v->isEmpty() )
	    addToList( v );
	else
	    delete v;
	}
    }

void
Storage::detectLvmVgs()
    {
    if (testmode())
	{
	    const list<string> l = glob(testdir() + "/lvmvg_*.info", GLOB_NOSORT);
	    for (list<string>::const_iterator i = l.begin(); i != l.end(); ++i)
		addToList(new LvmVg(this, AsciiFile(*i)));
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_LVM") == NULL)
	{
	const list<string> l = LvmVg::getVgs();
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
		y2mil("inactive VG " << *i);
		v->unuseDev();
		delete( v );
		}
	    }
	}
    }


void
    Storage::detectDmraid(SystemInfo& systeminfo)
{
    if (testmode())
    {
	    // TODO: load test data
    }
    else if (autodetect() && getenv("LIBSTORAGE_NO_DMRAID") == NULL)
    {
	const list<string> l = DmraidCo::getRaids(systeminfo);
	if (!l.empty())
	{
	    const map<string, list<string>> by_id = getUdevMap("/dev/disk/by-id");
	    for( list<string>::const_iterator i=l.begin(); i!=l.end(); ++i )
	    {
		    DmraidCo* v = new DmraidCo(this, *i, systeminfo);
		    map<string, list<string>>::const_iterator it = by_id.find("dm-" + decString(v->minorNr()));
		    if (it != by_id.end())
			v->setUdevData(it->second);
		    addToList( v );
	    }
	}
    }
}


void
    Storage::detectDmmultipath(SystemInfo& systeminfo)
{
    if (testmode())
    {
	    // TODO: load test data
    }
    else if (autodetect() && getenv("LIBSTORAGE_NO_DMMULTIPATH") == NULL)
    {
	const list<string> l = DmmultipathCo::getMultipaths(systeminfo);
	if (!l.empty())
	{
	    const map<string, list<string>> by_id = getUdevMap("/dev/disk/by-id");
	    for( list<string>::const_iterator i=l.begin(); i!=l.end(); ++i )
	    {
		    DmmultipathCo* v = new DmmultipathCo(this, *i, systeminfo);
		    map<string, list<string>>::const_iterator it = by_id.find("dm-" + decString(v->minorNr()));
		    if (it != by_id.end())
			v->setUdevData(it->second);
		    addToList( v );
	    }
	}
    }
}


void
    Storage::detectDm(SystemInfo& systeminfo, bool only_crypt)
    {
    if (testmode())
	{
	    // TODO: load test data
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_DM") == NULL)
	{
	    DmCo* v = NULL;
	    if (haveDm(v))
	    {
		v->second(true, systeminfo, only_crypt);
	    }
	    else
	    {
		v = new DmCo(this, true, systeminfo, only_crypt);
		if (!v->isEmpty() )
		{
		    addToList( v );
		    v->updateDmMaps();
		}
		else
		    delete v;
	    }
	}
    }


struct DiskData
    {
    enum DTyp { DISK, DASD, XEN };

    DiskData( const string& n, DTyp t, unsigned long long sz ) : d(NULL) { s=sz; typ=t; name=n; }

    Disk* d;
    DTyp typ;
    unsigned long long s;
    string name;
    string dev;
    };


std::ostream& operator<< ( std::ostream& s, const storage::DiskData& d )
    {
    s << d.name << "," << d.typ << "," << d.s << "," << d.d;
    if( !d.dev.empty() && d.dev!=d.name )
	s << "," << d.dev;
    return( s );
    }


void
    Storage::initDisk( DiskData& data, SystemInfo& systeminfo)
    {
    y2mil( "data:" << data );
    data.dev = boost::replace_all_copy(data.name, "!", "/");
    y2mil("name sysfs:" << data.name << " parted:" << data.dev);
    Disk * d = NULL;
    switch( data.typ )
	{
	case DiskData::DISK:
	    d = new Disk( this, data.dev, data.s );
	    break;
	case DiskData::DASD:
	    d = new Dasd( this, data.dev, data.s );
	    break;
	case DiskData::XEN:
	    {
	    string::size_type p = data.dev.find_last_not_of( "0123456789" );
	    int nr = -1;
	    data.dev.substr( p+1 ) >> nr;
	    data.dev.erase( p+1 );
	    if( nr>=0 )
		{
		    d = new Disk(this, data.dev, (unsigned) nr, data.s, systeminfo);
		}
	    break;
	    }
	}
    if( d && 
        (d->getSysfsInfo(SYSFSDIR "/" + data.name)||data.typ==DiskData::XEN) &&
	(data.typ == DiskData::XEN || d->detect(systeminfo.getProcParts())))
	{
	data.d = d;
	}
    else
	{
	delete d;
	}
    }


void
    Storage::autodetectDisks(SystemInfo& systeminfo)
    {
    DIR *Dir;
    struct dirent *Entry;
    if( (Dir=opendir(SYSFSDIR))!=NULL )
    {
	const map<string, list<string>> by_path = getUdevMap("/dev/disk/by-path");
	const map<string, list<string>> by_id = getUdevMap("/dev/disk/by-id");
	list<DiskData> dl;
	while( (Entry=readdir( Dir ))!=NULL )
	{
	    string dn = Entry->d_name;

	    if (dn == "." || dn == "..")
		continue;

	    // we do not treat mds as disks although they can be partitioned since kernel 2.6.28
	    if (boost::starts_with(dn, "md"))
		continue;

	    Disk::SysfsInfo sysfsinfo;
	    if (!Disk::getSysfsInfo(SYSFSDIR "/" + dn, sysfsinfo))
		continue;

	    if (sysfsinfo.range > 1 && (sysfsinfo.size > 0 || dn.find("dasd") == 0))
	    {
		DiskData::DTyp t = (dn.find("dasd") == 0) ? DiskData::DASD : DiskData::DISK;
		dl.push_back(DiskData(dn, t, sysfsinfo.size / 2));
	    }
	    else if (sysfsinfo.range == 1 && sysfsinfo.size > 0)
	    {
		if (sysfsinfo.vbd)
		{
		    dl.push_back(DiskData(dn, DiskData::XEN, sysfsinfo.size / 2));
		}
	    }
	}
	closedir( Dir );
	y2mil( "dl: " << dl );
	for( list<DiskData>::iterator i = dl.begin(); i!=dl.end(); ++i )
	    {
		initDisk(*i, systeminfo);
	    }
	y2mil( "dl: " << dl );
	for( list<DiskData>::const_iterator i = dl.begin(); i!=dl.end(); ++i )
	    {
	    if( i->d )
	        {
		string tmp1;
		map<string, list<string>>::const_iterator it1 = by_path.find(i->dev);
		if (it1 != by_path.end())
		    tmp1 = it1->second.front();

		list<string> tmp2;
		map<string, list<string>>::const_iterator it2 = by_id.find(i->dev);			
		if (it2 != by_id.end())
		    tmp2 = it2->second;

		i->d->setUdevData(tmp1, tmp2);
		addToList( i->d );
		}
	    }
	}
    else
	{
	y2err("Failed to open:" SYSFSDIR);
	}
    }


    void
    Storage::detectFsData(const VolIterator& begin, const VolIterator& end, SystemInfo& systeminfo)
    {
    y2mil("detectFsData begin");
    SystemCmd Losetup(LOSETUPBIN " -a");
    for( VolIterator i=begin; i!=end; ++i )
	{
	if (!i->isUsedBy())
	    {
	    i->getLoopData( Losetup );
	    i->getFsData(systeminfo.getBlkid());
	    y2mil( "detect:" << *i );
	    }
	}
    for( VolIterator i=begin; i!=end; ++i )
	{
	if (!i->isUsedBy())
	    {
	    i->getMountData(systeminfo.getProcMounts(), !detectMounted);
	    i->getFstabData( *fstab );
	    y2mil( "detect:" << *i );
	    if( i->getFs()==FSUNKNOWN && i->getEncryption()==ENC_NONE )
		{
		    Blkid::Entry entry;
		    if (systeminfo.getBlkid().getEntry(i->dev, entry) && entry.is_luks)
			i->initEncryption(ENC_LUKS);
		}
	    }
	}
    y2mil("detectFsData end");
    }


void
Storage::printInfo(ostream& str) const
{
    ConstContPair p = contPair();
    for (ConstContIterator i = p.begin(); i != p.end(); ++i)
    {
	i->print(str);
	str << endl;

	Container::ConstVolPair vp = i->volPair();
	for (Container::ConstVolIterator j = vp.begin(); j != vp.end(); ++j)
	{
	    j->print(str);
	    str << endl;
	}
    }
}


void
Storage::detectFsDataTestMode( const string& file, const VolIterator& begin,
			       const VolIterator& end )
{
    AsciiFile vd( file );
    const vector<string>& lines = vd.lines();

    for( VolIterator i=begin; i!=end; ++i )
    {
	vector<string>::const_iterator pos = find_if(lines, string_starts_with(i->device() + " "));
	if (pos != lines.end())
	{
	    i->getTestmodeData(*pos);
	}
	i->getFstabData( *fstab );
    }
}


    void
    Storage::logContainersAndVolumes(const string& Dir) const
    {
	if (max_log_num > 0)
	{
	    for (CCIter i = cont.begin(); i != cont.end(); ++i)
		(*i)->logData(Dir);

	    logVolumes(Dir);
	    logFreeInfo(Dir);
	    logArchInfo(Dir);
	}
    }


void
Storage::logVolumes(const string& Dir) const
    {
    string fname(Dir + "/volume.info.tmp");
    ofstream file( fname.c_str() );
    classic(file);
    ConstVolPair p = volPair();
    for (ConstVolIterator i = p.begin(); i != p.end(); ++i)
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
Storage::testFilesEqual(const string& n1, const string& n2)
{
    bool ret = false;
    if (access(n1.c_str(), R_OK) == 0 && access(n2.c_str(), R_OK) == 0)
	ret = AsciiFile(n1).lines() == AsciiFile(n2).lines();
    y2deb("ret:" << ret << " n1:" << n1 << " n2:" << n2);
    return ret;
}


void
Storage::handleLogFile(const string& name) const
    {
    string bname( name );
    string::size_type pos = bname.rfind( '.' );
    bname.erase( pos );
    y2mil("name:" << name << " bname:" << bname);
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


void Storage::setRecursiveRemoval(bool val)
{
    y2mil("val:" << val);
    recursiveRemove = val;
}


int
Storage::getRecursiveUsing(const string& device, list<string>& devices)
{
    y2mil("device:" << device);
    assertInit();
    devices.clear();
    int ret = getRecursiveUsingHelper(device, devices);
    y2mil("ret:" << ret << " devices:" << devices);
    return ret;
}


int
Storage::getRecursiveUsingHelper(const string& device, list<string>& devices)
    {
    int ret = 0;
    ConstContIterator cont;
    ConstVolIterator vol;

    if (findContainer(device, cont))
	{
	Container::ConstVolPair p = cont->volPair(Volume::notDeleted); 
	for( Container::ConstVolIterator it = p.begin(); it != p.end(); ++it )
	    {
	    addIfNotThere( devices, it->device() );
	    getRecursiveUsingHelper(it->device(), devices);
	    }

	if (cont->isUsedBy())
	    {
	    const list<UsedBy> usedBy = cont->getUsedBy();
	    for( list<UsedBy>::const_iterator it = usedBy.begin(); 
	         it != usedBy.end(); ++it)
		{
		addIfNotThere( devices, it->device() );
		getRecursiveUsingHelper(it->device(), devices);
		}
	    }
	}
    else if (findVolume(device, vol))
	{
	if (vol->isUsedBy())
	    {
	    const list<UsedBy> usedBy = vol->getUsedBy();
	    for( list<UsedBy>::const_iterator it = usedBy.begin(); 
	         it != usedBy.end(); ++it)
		{
		addIfNotThere( devices, it->device() );
		getRecursiveUsingHelper(it->device(), devices);
		}
	    }
	CType typ = vol->cType();
	list<string> dl;
	switch( typ )
	    {
	    default:
		break;
	    case DISK:
		{
		const Partition* p = dynamic_cast<const Partition*>(&(*vol));
		if( p!=NULL && p->type()==EXTENDED )
		    {
		    const Disk* d = p->disk();
		    Disk::ConstPartPair pp = d->partPair(Partition::notDeleted);
		    for( Disk::ConstPartIter i=pp.begin(); i!=pp.end(); ++i )
			{
			if( i->type()==LOGICAL )
			    dl.push_back(i->device());
			}
		    }
		}
		break;
	    case DMRAID:
	    case DMMULTIPATH:
		{
		const DmPart* dp = dynamic_cast<const DmPart*>(&(*vol));
		const Partition* p = dp!=NULL ? dp->getPtr() : NULL;
		if( p!=NULL && p->type()==EXTENDED )
		    {
		    const Container* co = vol->getContainer();
		    const DmPartCo* d = dynamic_cast<const DmPartCo *>(co);
		    if( d!=NULL )
			{
			DmPartCo::ConstDmPartPair pp = d->dmpartPair(DmPart::notDeleted);
			for( DmPartCo::ConstDmPartIter i=pp.begin(); i!=pp.end(); ++i )
			    {
			    p = i->getPtr();
			    if( p!=NULL && p->type()==LOGICAL )
				dl.push_back(i->device());
			    }
			}
		    }
		}
		break;
	    case MDPART:
		{
		const MdPart* dp = dynamic_cast<const MdPart*>(&(*vol));
		const Partition* p = dp!=NULL ? dp->getPtr() : NULL;
		if( p!=NULL && p->type()==EXTENDED )
		    {
		    const Container* co = vol->getContainer();
		    const MdPartCo* d = dynamic_cast<const MdPartCo *>(co);
		    if( d!=NULL )
			{
			MdPartCo::ConstMdPartPair pp = d->mdpartPair(MdPart::notDeleted);
			for( MdPartCo::ConstMdPartIter i=pp.begin(); i!=pp.end(); ++i )
			    {
			    p = i->getPtr();
			    if( p!=NULL && p->type()==LOGICAL )
				dl.push_back(i->device());
			    }
			}
		    }
		}
		break;
	    }
	for( list<string>::const_iterator i = dl.begin(); i != dl.end(); ++i )
	    {
	    addIfNotThere( devices, *i );
	    getRecursiveUsingHelper(*i, devices);
	    }
	}
    else
	{
	ret = STORAGE_DEVICE_NOT_FOUND;
	}
    return ret;
    }


void Storage::setZeroNewPartitions(bool val)
{
    y2mil("val:" << val);
    zeroNewPartitions = val;
}

void Storage::setDefaultMountBy(MountByType val)
{
    y2mil("val:" << Volume::mbyTypeString(val));
    defaultMountBy = val;
}


void Storage::setDefaultFs(FsType val)
{
    y2mil("val:" << Volume::fsTypeString(val));
    defaultFs = val;
}


    void
    Storage::setEfiBoot(bool val)
    {
	assertInit();
	y2mil("val:" << val);
	efiboot = val;
    }


    bool
    Storage::getEfiBoot()
    {
	assertInit();
	return efiboot;
    }


void Storage::setRootPrefix(const string& root)
{
    y2mil("root:" << root);
    rootprefix = root;
}

string Storage::prependRoot(const string& mp) const
{
    string ret = mp;
    if (mp != "swap")
	ret.insert(0, rootprefix);
    return ret;
}

void Storage::setDetectMountedVolumes(bool val)
{
    y2mil("val:" << val);
    detectMounted = val;
}


string Storage::proc_arch = "i386";
bool Storage::is_ppc_mac = false;
bool Storage::is_ppc_pegasos = false;


    StorageInterface* createStorageInterface(const Environment& env)
    {
	return new Storage(env);
    }

    StorageInterface* createStorageInterfacePid(const Environment& env, pid_t& locker_pid)
    {
	try
	{
	    return new Storage(env);
	}
	catch (const LockException& e)
	{
	    locker_pid = e.getLockerPid();
	    return NULL;
	}
    }

    void destroyStorageInterface (StorageInterface* p)
    {
	delete p;
    }


int
Storage::createPartition( const string& disk, PartitionType type, unsigned long start,
			  unsigned long size, string& device )
    {
    int ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " type:" << type << " start:" << start << " size:" << size);
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	DiskIterator i = findDisk( disk );
	if( i != dEnd() )
	    {
	    done = true;
	    if (i->isUsedBy())
		ret = STORAGE_DISK_USED_BY;
	    else
		{
		ret = i->createPartition( type, start, size, device, true );
		if( ret==0 )
		    checkPwdBuf( device );
		}
	    }
	}
    if( ret==0 && !done )
	{
	DmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    if (i->isUsedBy())
		ret = STORAGE_DISK_USED_BY;
	    else
		{
		ret = i->createPartition( type, start, size, device, true );
		if( ret==0 )
		    checkPwdBuf( device );
		}
	    }
	}  
    if( ret==0 && !done )
        {
        MdPartCoIterator i = findMdPartCo( disk );
        if( i != mdpCoEnd() )
            {
            done = true;
            if (i->isUsedBy())
                ret = STORAGE_DISK_USED_BY;
            else
                {
                ret = i->createPartition( type, start, size, device, true );
		if( ret==0 )
		    checkPwdBuf( device );
                }
            }
        }
    if( !done && ret==0 )
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret << " device:" << (ret?"":device));
    return( ret );
    }

int
Storage::createPartitionKb( const string& disk, PartitionType type,
                            unsigned long long start,
			    unsigned long long sizeK, string& device )
    {
    int ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " type:" << type << " start:" << start << " sizeK:" << sizeK);
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	DiskIterator i = findDisk( disk );
	if( i != dEnd() )
	    {
	    done = true;
	    if (i->isUsedBy())
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
	}
    if( ret==0 && !done )
	{
	DmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    if (i->isUsedBy())
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
	}
    if( ret==0 && !done )
        {
        MdPartCoIterator i = findMdPartCo( disk );
        if( i != mdpCoEnd() )
            {
            done = true;
            if (i->isUsedBy())
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
        }
    if( ret==0 && !done )
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    y2mil("ret:" << ret << " device:" << (ret?"":device));
    return( ret );
    }

int
Storage::createPartitionAny( const string& disk, unsigned long long sizeK,
                             string& device )
    {
    int ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " sizeK:" << sizeK);
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	DiskIterator i = findDisk( disk );
	if( i != dEnd() )
	    {
	    done = true;
	    if (i->isUsedBy())
		ret = STORAGE_DISK_USED_BY;
	    else
		{
		unsigned long num_cyl = i->kbToCylinder( sizeK );
		ret = i->createPartition( num_cyl, device, true );
		}
	    }
	}
    if( ret==0 && !done )
	{
	DmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    if (i->isUsedBy())
		ret = STORAGE_DISK_USED_BY;
	    else
		{
		unsigned long num_cyl = i->kbToCylinder( sizeK );
		ret = i->createPartition( num_cyl, device, true );
		}
	    }
	}
    if( ret==0 && !done )
        {
        MdPartCoIterator i = findMdPartCo( disk );
        if( i != mdpCoEnd() )
            {
            done = true;
            if (i->isUsedBy())
                ret = STORAGE_DISK_USED_BY;
            else
                {
                unsigned long num_cyl = i->kbToCylinder( sizeK );
                ret = i->createPartition( num_cyl, device, true );
                }
            }
        }
    if( ret==0 && !done )
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    y2mil("ret:" << ret << " device:" << (ret?"":device));
    return( ret );
    }

int
Storage::nextFreePartition( const string& disk, PartitionType type,
                            unsigned& nr, string& device )
    {
    int ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " type:" << type);
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	done = true;
	ret = i->nextFreePartition( type, nr, device );
	}
    if( !done )
	{
	DmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    ret = i->nextFreePartition( type, nr, device );
	    }
	}
    if( !done )
        {
        MdPartCoIterator i = findMdPartCo( disk );
        if( i != mdpCoEnd() )
            {
            done = true;
            ret = i->nextFreePartition( type, nr, device );
            }
        }
    if( !done )
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    y2mil("ret:" << ret << " device:" << (ret?"":device));
    return( ret );
    }

int
Storage::createPartitionMax( const string& disk, PartitionType type,
                             string& device )
    {
    int ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " type:" << type);
    DiskIterator i = findDisk( disk );
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	DiskIterator i = findDisk( disk );
	if( i != dEnd() )
	    {
	    done = true;
	    if (i->isUsedBy())
		ret = STORAGE_DISK_USED_BY;
	    else
		{
		ret = i->createPartition( type, device );
		}
	    }
	}
    if( ret==0 && !done )
	{
	DmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    if (i->isUsedBy())
		ret = STORAGE_DISK_USED_BY;
	    else
		{
		ret = i->createPartition( type, device );
		}
	    }
	}
    if( ret==0 && !done )
        {
        MdPartCoIterator i = findMdPartCo( disk );
        if( i != mdpCoEnd() )
            {
            done = true;
            if (i->isUsedBy())
                ret = STORAGE_DISK_USED_BY;
            else
                {
                ret = i->createPartition( type, device );
                }
            }
        }
    if( ret==0 && !done )
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    y2mil("ret:" << ret << " device:" << (ret?"":device));
    return( ret );
    }

unsigned long long
Storage::cylinderToKb( const string& disk, unsigned long size )
    {
    unsigned long long ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " size:" << size);
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	done = true;
	ret = i->cylinderToKb( size );
	}
    if( !done )
	{
	DmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    ret = i->cylinderToKb( size );
	    }
	}
    if( !done )
    {
        MdPartCoIterator i = findMdPartCo( disk );
        if( i != mdpCoEnd() )
	{
            done = true;
            ret = i->cylinderToKb( size );
	}
    }
    y2mil("ret:" << ret);
    return( ret );
    }

unsigned long
Storage::kbToCylinder( const string& disk, unsigned long long sizeK )
    {
    unsigned long ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " sizeK:" << sizeK);
    DiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	done = true;
	ret = i->kbToCylinder( sizeK );
	}
    if( !done )
	{
	DmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    ret = i->kbToCylinder( sizeK );
	    }
	}
    if( !done )
    {
        MdPartCoIterator i = findMdPartCo( disk );
        if( i != mdpCoEnd() )
	{
            done = true;
            ret = i->kbToCylinder( sizeK );
	}
    }
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::removePartition( const string& partition )
    {
    int ret = 0;
    assertInit();
    y2mil("partition:" << partition);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol ) )
	{
	if( cont->type()==DISK )
	    {
	    Disk* disk = dynamic_cast<Disk *>(&(*cont));
	    if( disk!=NULL )
		{
		if (!vol->isUsedBy() || recursiveRemove)
		    {
		    if (vol->isUsedBy())
			ret = removeUsing( vol->device(), vol->getUsedBy() );
		    if( ret==0 )
			ret = disk->removePartition( vol->nr() );
		    }
		else
		    ret = STORAGE_REMOVE_USED_VOLUME;
		}
	    else
		{
		ret = STORAGE_REMOVE_PARTITION_INVALID_CONTAINER;
		}
	    }
	else if( cont->type()==DMRAID || cont->type()==DMMULTIPATH )
	    {
	    DmPartCo* disk = dynamic_cast<DmPartCo *>(&(*cont));
	    if( disk!=NULL )
		{
		if (!vol->isUsedBy() || recursiveRemove)
		    {
		    if (vol->isUsedBy())
			ret = removeUsing( vol->device(), vol->getUsedBy() );
		    if( ret==0 )
			ret = disk->removePartition( vol->nr() );
		    }
		else
		    ret = STORAGE_REMOVE_USED_VOLUME;
		}
	    else
		{
		ret = STORAGE_REMOVE_PARTITION_INVALID_CONTAINER;
		}
	    }
	else if( cont->type() == MDPART )
	  {
	  MdPartCo* disk = dynamic_cast<MdPartCo *>(&(*cont));
	  if( disk != NULL)
	    {
	    if (!vol->isUsedBy() || recursiveRemove)
	      {
		  if( vol->isUsedBy())
	        ret = removeUsing( vol->device(), vol->getUsedBy() );
	      if( ret==0 )
	        {
	        ret = disk->removePartition( vol->nr() );
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::updatePartitionArea( const string& partition, unsigned long start,
                              unsigned long size )
    {
    int ret = 0;
    assertInit();
    y2mil("partition:" << partition << " start:" << start << " size:" << size);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol ) )
	{
	if( cont->type()==DISK )
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
	else if( cont->type()==DMRAID || cont->type()==DMMULTIPATH )
	    {
	    DmPartCo* disk = dynamic_cast<DmPartCo *>(&(*cont));
	    if( disk!=NULL )
		{
		ret = disk->changePartitionArea( vol->nr(), start, size );
		}
	    else
		{
		ret = STORAGE_CHANGE_AREA_INVALID_CONTAINER;
		}
	    }
	else if( cont->type() == MDPART )
	  {
	  MdPartCo* disk = dynamic_cast<MdPartCo *>(&(*cont));
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
    y2mil("ret:" << ret);
    return( ret );
    }


int
Storage::freeCylindersAroundPartition(const string& partition, unsigned long& freeCylsBefore,
				      unsigned long& freeCylsAfter)
{
    int ret = 0;
    assertInit();
    y2mil("partition:" << partition);
    ConstVolIterator vol;
    ConstContIterator cont;
    if( findVolume( partition, cont, vol ) )
    {
	if( cont->type()==DISK )
	{
	    const Disk* disk = dynamic_cast<const Disk*>(&(*cont));
	    const Partition* p = dynamic_cast<const Partition*>(&(*vol));
	    if( disk!=NULL && p!=NULL )
	    {
		ret = disk->freeCylindersAroundPartition(p, freeCylsBefore, freeCylsAfter);
	    }
	    else
	    {
		ret = STORAGE_RESIZE_INVALID_CONTAINER;
	    }
	}
	else if( cont->type()==DMRAID || cont->type()==DMMULTIPATH )
	{
	    const DmPartCo* disk = dynamic_cast<const DmPartCo*>(&(*cont));
	    const DmPart* p = dynamic_cast<const DmPart*>(&(*vol));
	    if( disk!=NULL && p!=NULL )
	    {
		ret = disk->freeCylindersAroundPartition(p, freeCylsBefore, freeCylsAfter);
	    }
	    else
	    {
		ret = STORAGE_RESIZE_INVALID_CONTAINER;
	    }
	}
	else if ( cont->type() == MDPART )
	  {
          const MdPartCo* disk = dynamic_cast<const MdPartCo*>(&(*cont));
          const MdPart* p = dynamic_cast<const MdPart*>(&(*vol));
          if( disk!=NULL && p!=NULL )
          {
              ret = disk->freeCylindersAroundPartition(p, freeCylsBefore, freeCylsAfter);
          }
          else
          {
              ret = STORAGE_RESIZE_INVALID_CONTAINER;
          }
	  }
	else
	{
	    ret = STORAGE_RESIZE_INVALID_CONTAINER;
	}
    }
    else
    {
	ret = STORAGE_VOLUME_NOT_FOUND;
    }
    y2mil("ret:" << ret);
    return ret;
}


int
Storage::changePartitionId( const string& partition, unsigned id )
    {
    int ret = 0;
    assertInit();
    y2mil("partition:" << partition << " id:" << hex << id);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol ) )
	{
	if( cont->type()==DISK )
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
	else if( cont->type()==DMRAID || cont->type()==DMMULTIPATH )
	    {
	    DmPartCo* disk = dynamic_cast<DmPartCo *>(&(*cont));
	    if( disk!=NULL )
		{
		ret = disk->changePartitionId( vol->nr(), id );
		}
	    else
		{
		ret = STORAGE_CHANGE_PARTITION_ID_INVALID_CONTAINER;
		}
	    }
	else if ( cont->type()==MDPART  )
	  {
          MdPartCo* disk = dynamic_cast<MdPartCo *>(&(*cont));
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::resizePartition( const string& partition, unsigned long sizeCyl )
    {
    return( resizePartition( partition, sizeCyl, false ));
    }

int
Storage::resizePartitionNoFs( const string& partition, unsigned long sizeCyl )
    {
    return( resizePartition( partition, sizeCyl, true ));
    }

int
Storage::resizePartition( const string& partition, unsigned long sizeCyl,
                          bool ignoreFs )
    {
    int ret = 0;
    assertInit();
    y2mil("partition:" << partition << " newCyl:" << sizeCyl << " ignoreFs:" << ignoreFs);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol ) )
	{
	if( cont->type()==DISK )
	    {
	    Disk* disk = dynamic_cast<Disk *>(&(*cont));
	    Partition* p = dynamic_cast<Partition *>(&(*vol));
	    if( disk!=NULL && p!=NULL )
		{
		if( ignoreFs )
		    p->setIgnoreFs();
		ret = disk->resizePartition( p, sizeCyl );
		}
	    else
		{
		ret = STORAGE_RESIZE_INVALID_CONTAINER;
		}
	    }
	else if( cont->type()==DMRAID || cont->type()==DMMULTIPATH )
	    {
	    DmPartCo* disk = dynamic_cast<DmPartCo *>(&(*cont));
	    DmPart* p = dynamic_cast<DmPart *>(&(*vol));
	    if( disk!=NULL && p!=NULL )
		{
		if( ignoreFs )
		    p->setIgnoreFs();
		ret = disk->resizePartition( p, sizeCyl );
		}
	    else
		{
		ret = STORAGE_RESIZE_INVALID_CONTAINER;
		}
	    }
	else if( cont->type()== MDPART )
	  {
          MdPartCo* disk = dynamic_cast<MdPartCo *>(&(*cont));
          MdPart* p = dynamic_cast<MdPart *>(&(*vol));
          if( disk!=NULL && p!=NULL )
              {
              if( ignoreFs )
                  p->setIgnoreFs();
              ret = disk->resizePartition( p, sizeCyl );
              }
          else
              {
              ret = STORAGE_RESIZE_INVALID_CONTAINER;
              }
	  }
	else
	    {
	    ret = STORAGE_RESIZE_INVALID_CONTAINER;
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::forgetChangePartitionId( const string& partition )
    {
    int ret = 0;
    assertInit();
    y2mil("partition:" << partition);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol ) )
	{
	if( cont->type()==DISK )
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
	else if( cont->type()==DMRAID || cont->type()==DMMULTIPATH )
	    {
	    DmPartCo* disk = dynamic_cast<DmPartCo *>(&(*cont));
	    if( disk!=NULL )
		{
		ret = disk->forgetChangePartitionId( vol->nr() );
		}
	    else
		{
		ret = STORAGE_CHANGE_PARTITION_ID_INVALID_CONTAINER;
		}
	    }
	else if( cont->type() == MDPART )
	  {
          MdPartCo* disk = dynamic_cast<MdPartCo *>(&(*cont));
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
    y2mil("ret:" << ret);
    return( ret );
    }


    string
    Storage::getPartitionPrefix(const string& disk)
    {
	return Disk::partNaming(disk);
    }


    string
    Storage::getPartitionName(const string& disk, int partition_no)
    {
	return disk + Disk::partNaming(disk) + decString(partition_no);
    }


int
Storage::getUnusedPartitionSlots(const string& disk, list<PartitionSlotInfo>& slots)
{
    int ret = 0;
    slots.clear();
    assertInit();

    // TODO: don't have code below twice

    DiskIterator i1 = findDisk( disk );
    DmPartCoIterator i2 = findDmPartCo( disk );
    MdPartCoIterator i3 = findMdPartCo( disk );

    if (i1 != dEnd())
    {
	// maxPrimary() and maxLogical() include limits from partition table type and
	// minor number range

	bool primaryPossible = i1->numPrimary() + (i1->hasExtended() ? 1 : 0) < i1->maxPrimary();
	bool extendedPossible = primaryPossible && i1->extendedPossible() && !i1->hasExtended();
	bool logicalPossible = i1->hasExtended() && i1->numLogical() < (i1->maxLogical() - i1->maxPrimary());

	list<Region> regions;

	i1->getUnusedSpace(regions, false, false);
	for (list<Region>::const_iterator region=regions.begin(); region!=regions.end(); region++)
	{
	    PartitionSlotInfo slot;
	    slot.cylStart = region->start();
	    slot.cylSize = region->len();
	    slot.primarySlot = true;
	    slot.primaryPossible = primaryPossible;
	    slot.extendedSlot = true;
	    slot.extendedPossible = extendedPossible;
	    slot.logicalSlot = false;
	    slot.logicalPossible = false;
	    slots.push_back(slot);
	}

	i1->getUnusedSpace(regions, false, true);
	for (list<Region>::const_iterator region=regions.begin(); region!=regions.end(); region++)
	{
	    PartitionSlotInfo slot;
	    slot.cylStart = region->start();
	    slot.cylSize = region->len();
	    slot.primarySlot = false;
	    slot.primaryPossible = false;
	    slot.extendedSlot = false;
	    slot.extendedPossible = false;
	    slot.logicalSlot = true;
	    slot.logicalPossible = logicalPossible;
	    slots.push_back(slot);
	}
    }
    else if (i2 != dmpCoEnd())
    {
	// maxPrimary() and maxLogical() include limits from partition table type and
	// minor number range

	bool primaryPossible = i2->numPrimary() + (i2->hasExtended() ? 1 : 0) < i2->maxPrimary();
	bool extendedPossible = primaryPossible && i2->extendedPossible() && !i2->hasExtended();
	bool logicalPossible = i2->hasExtended() && i2->numLogical() < (i2->maxLogical() - i2->maxPrimary());

	list<Region> regions;

	i2->getUnusedSpace(regions, false, false);
	for (list<Region>::const_iterator region=regions.begin(); region!=regions.end(); region++)
	{
	    PartitionSlotInfo slot;
	    slot.cylStart = region->start();
	    slot.cylSize = region->len();
	    slot.primarySlot = true;
	    slot.primaryPossible = primaryPossible;
	    slot.extendedSlot = true;
	    slot.extendedPossible = extendedPossible;
	    slot.logicalSlot = false;
	    slot.logicalPossible = false;
	    slots.push_back(slot);
	}

	i2->getUnusedSpace(regions, false, true);
	for (list<Region>::const_iterator region=regions.begin(); region!=regions.end(); region++)
	{
	    PartitionSlotInfo slot;
	    slot.cylStart = region->start();
	    slot.cylSize = region->len();
	    slot.primarySlot = false;
	    slot.primaryPossible = false;
	    slot.extendedSlot = false;
	    slot.extendedPossible = false;
	    slot.logicalSlot = true;
	    slot.logicalPossible = logicalPossible;
	    slots.push_back(slot);
	}
    }
    else if (i3 != mdpCoEnd())
    {
        // maxPrimary() and maxLogical() include limits from partition table type and
        // minor number range

        bool primaryPossible = i3->numPrimary() + (i3->hasExtended() ? 1 : 0) < i3->maxPrimary();
        bool extendedPossible = primaryPossible && i3->extendedPossible() && !i3->hasExtended();
        bool logicalPossible = i3->hasExtended() && i3->numLogical() < (i3->maxLogical() - i3->maxPrimary());

        list<Region> regions;

        i3->getUnusedSpace(regions, false, false);
        for (list<Region>::const_iterator region=regions.begin(); region!=regions.end(); region++)
        {
            PartitionSlotInfo slot;
            slot.cylStart = region->start();
            slot.cylSize = region->len();
            slot.primarySlot = true;
            slot.primaryPossible = primaryPossible;
            slot.extendedSlot = true;
            slot.extendedPossible = extendedPossible;
            slot.logicalSlot = false;
            slot.logicalPossible = false;
            slots.push_back(slot);
        }

        i3->getUnusedSpace(regions, false, true);
        for (list<Region>::const_iterator region=regions.begin(); region!=regions.end(); region++)
        {
            PartitionSlotInfo slot;
            slot.cylStart = region->start();
            slot.cylSize = region->len();
            slot.primarySlot = false;
            slot.primaryPossible = false;
            slot.extendedSlot = false;
            slot.extendedPossible = false;
            slot.logicalSlot = true;
            slot.logicalPossible = logicalPossible;
            slots.push_back(slot);
        }
    }
    else
    {
	ret = STORAGE_DISK_NOT_FOUND;
    }
    y2mil("ret:" << ret);
    return ret;
}


int
Storage::destroyPartitionTable( const string& disk, const string& label )
    {
    int ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " label:" << label);

    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	DiskIterator i = findDisk( disk );
	if( i != dEnd() )
	    {
	    done = true;
	    ret = i->destroyPartitionTable( label );
	    }
	}
    if( ret==0 && !done )
	{
	DmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    ret = i->destroyPartitionTable( label );
	    }
	}
    if( ret==0 && !done )
        {
        MdPartCoIterator i = findMdPartCo( disk );
        if( i != mdpCoEnd() )
            {
            done = true;
            ret = i->destroyPartitionTable( label );
            }
        }
    if( ret==0 && !done )
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::initializeDisk( const string& disk, bool value )
    {
    int ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " value:" << value);

    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	DiskIterator i = findDisk( disk );
	if( i != dEnd() )
	    {
	    done = true;
	    ret = i->initializeDisk( value );
	    }
	}
    if( ret==0 && !done )
	{
	DmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    ret = DISK_INIT_NOT_POSSIBLE;
	    }
	}
    if( ret==0 && !done )
        {
        MdPartCoIterator i = findMdPartCo( disk );
        if( i != mdpCoEnd() )
            {
            done = true;
            ret = DISK_INIT_NOT_POSSIBLE;
            }
        }
    if( ret==0 && !done )
	{
	ret = STORAGE_DISK_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }


string
Storage::defaultDiskLabel()
{
    assertInit();
    return Disk::defaultLabel(efiBoot(), 0);
}

string
Storage::defaultDiskLabelSize(unsigned long long size_k)
{
    assertInit();
    return Disk::defaultLabel(efiBoot(), size_k);
}


int
Storage::changeFormatVolume( const string& device, bool format, FsType fs )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " format:" << format << " type:" << Volume::fsTypeString(fs));
    VolIterator vol;
    ContIterator cont;
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::changeLabelVolume( const string& device, const string& label )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " label:" << label);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::eraseLabelVolume( const string& device )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->eraseLabel();
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::changeMkfsOptVolume( const string& device, const string& opts )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " opts:" << opts);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::changeTunefsOptVolume( const string& device, const string& opts )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " opts:" << opts);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->setTunefsOption( opts );
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::changeDescText( const string& device, const string& txt )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " txt:" << txt);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->setDescText( txt );
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::changeMountPoint( const string& device, const string& mount )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " mount:" <<  mount);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::getMountPoint( const string& device, string& mount )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device);
    ConstVolIterator vol;
    ConstContIterator cont;
    if( findVolume( device, cont, vol ) )
	{
	mount = vol->getMount();
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::changeMountBy( const string& device, MountByType mby )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " mby:" << Volume::mbyTypeString(mby));
    VolIterator vol;
    ContIterator cont;
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::getMountBy( const string& device, MountByType& mby )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device);
    ConstVolIterator vol;
    ConstContIterator cont;
    if( findVolume( device, cont, vol ) )
	{
	mby = vol->getMountBy();
	}
    else
	{
	mby = defaultMountBy;
	pair<string,unsigned> dp = Disk::getDiskPartition(device);
	y2mil( "dp:" << dp );

	ConstDiskIterator i1 = findDisk(dp.first);
	ConstDmPartCoIterator i2 = findDmPartCo(dp.first);
	ConstMdPartCoIterator i3 = findMdPartCo(dp.first);
	if (i1 != dEnd())
	{ 
	    if ((mby == MOUNTBY_ID && i1->udevId().empty()) ||
		(mby == MOUNTBY_PATH && i1->udevPath().empty()))
		mby = MOUNTBY_DEVICE;
	}
	else if (i2 != dmpCoEnd())
	{
	    if ((mby == MOUNTBY_ID && i2->udevId().empty()) ||
		(mby == MOUNTBY_PATH && i2->udevPath().empty()))
		mby = MOUNTBY_DEVICE;
	}
	else if (i3 != mdpCoEnd())
	{
	    if ((mby == MOUNTBY_ID && i3->udevId().empty()) ||
		(mby == MOUNTBY_PATH && i3->udevPath().empty()))
		mby = MOUNTBY_DEVICE;
	}
	else
	{
	    ret = STORAGE_VOLUME_NOT_FOUND;
	}
	}
    y2mil("ret:" << ret << " mby:" << Volume::mbyTypeString(mby));
    return( ret );
    }

int
Storage::changeFstabOptions( const string& device, const string& options )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " options:" << options);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::getFstabOptions( const string& device, string& options )
{
    int ret = 0;
    assertInit();
    y2mil("device:" << device);
    ConstVolIterator vol;
    ConstContIterator cont;
    if( findVolume( device, cont, vol ) )
    {
	options = vol->getFstabOption();
    }
    else
    {
	ret = STORAGE_VOLUME_NOT_FOUND;
    }
    y2mil("ret:" << ret << " options:" << options);
    return( ret );
}

int
Storage::addFstabOptions( const string& device, const string& options )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " options:" << options);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
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
	ret = vol->changeFstabOptions( boost::join( opts, "," ) );
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::removeFstabOptions( const string& device, const string& options )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " options:" << options);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	list<string> l = splitString( options, "," );
	list<string> opts = splitString( vol->getFstabOption(), "," );
	for( list<string>::const_iterator i=l.begin(); i!=l.end(); i++ )
	    {
	    opts.remove_if(regex_matches(*i));
	    }
	ret = vol->changeFstabOptions( boost::join( opts, "," ) );
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::setCrypt( const string& device, bool val )
    {
    y2mil("device:" << device << " val:" << val);
    return( setCryptType( device, val, ENC_LUKS ));
    }
    
int
Storage::setCryptType( const string& device, bool val, EncryptType typ )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " val:" << val << " type:" << typ);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	ret = vol->setEncryption( val, typ );
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    if( !val )
	pwdBuf.erase(device);
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::getCrypt( const string& device, bool& val )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device);
    ConstVolIterator vol;
    ConstContIterator cont;
    if( findVolume( device, cont, vol ) )
	{
	val = vol->getEncryption();
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    y2mil("ret:" << ret << " val:" << val);
    return( ret );
    }

int
Storage::verifyCryptPassword( const string& device, const string& pwd )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " l:" << pwd.length());
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    y2mil("password:" << pwd);
#endif

    VolIterator vol;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, vol ) )
	{
	ret = vol->setCryptPwd( pwd );
	if( ret==0 && vol->detectEncryption()==ENC_UNKNOWN )
	    ret = VOLUME_CRYPT_NOT_DETECTED;
	vol->clearCryptPwd();
	}
    else
	{
	ret = verifyCryptFilePassword( device, pwd );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::verifyCryptFilePassword( const string& file, const string& pwd )
    {
    int ret = VOLUME_CRYPT_NOT_DETECTED;
    assertInit();
    y2mil("file:" << file << " l:" << pwd.length());
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    y2mil("password:" << pwd);
#endif

    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else
	{
	SystemInfo systeminfo;
	LoopCo* co = new LoopCo(this, false, systeminfo);
	if( co )
	    {
	    Loop* loop = new Loop( *co, file, true, 0, true );
	    if( loop && loop->setCryptPwd( pwd )==0 && 
		loop->detectEncryption()!=ENC_UNKNOWN )
		ret = 0;
	    if( loop )
		delete loop;
	    delete co;
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::setCryptPassword( const string& device, const string& pwd )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " l:" << pwd.length());
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    y2mil("password:" << pwd);
#endif

    VolIterator vol;
    map<string,string>::iterator i = pwdBuf.find(device);
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, vol ) )
	{
	ret = vol->setCryptPwd( pwd );
	if( i!=pwdBuf.end() )
	    pwdBuf.erase(i);
	}
    else
	{
	mapInsertOrReplace( pwdBuf, device, pwd );
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::forgetCryptPassword( const string& device )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device);

    VolIterator vol;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, vol ) )
	{
	vol->clearCryptPwd();
	}
    else
	{
	map<string,string>::iterator i = pwdBuf.find(device);
	if( i!=pwdBuf.end() )
	    pwdBuf.erase(i);
	else
	    ret = STORAGE_VOLUME_NOT_FOUND;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

bool
Storage::needCryptPassword( const string& device )
    {
    bool ret = true;
    bool volFound = false;
    assertInit();
    y2mil("device:" << device);

    ConstVolIterator vol;
    if( checkNormalFile(device) )
	{
	ConstLoopPair p = loopPair(Loop::notDeleted);
	ConstLoopIterator i = p.begin(); 
	while( i != p.end() && i->loopFile()!=device )
	    ++i;
	if( i != p.end() )
	    {
	    ret = i->needCryptPwd();
	    volFound = true;
	    }
	}
    else if( findVolume( device, vol ) )
	{
	ret = vol->needCryptPwd();
	volFound = true;
	}
    if( !volFound )
	{
	ret = pwdBuf.find( device )==pwdBuf.end();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::getCryptPassword( const string& device, string& pwd )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device);

    pwd.clear();
    ConstVolIterator vol;
    if( findVolume( device, vol ) )
	{
	pwd = vol->getCryptPwd();
	}
    else
	{
	map<string,string>::const_iterator i = pwdBuf.find(device);
	if( i!=pwdBuf.end() )
	    pwd = i->second;
	else
	    ret = STORAGE_VOLUME_NOT_FOUND;
	}
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    y2mil("password:" << pwd);
#endif
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::setIgnoreFstab( const string& device, bool val )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " val:" << val);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::getIgnoreFstab( const string& device, bool& val )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device);
    ConstVolIterator vol;
    ConstContIterator cont;
    if( findVolume( device, cont, vol ) )
	{
	val = vol->ignoreFstab();
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    y2mil("ret:" << ret << " val:" << val);
    return( ret );
    }

int
Storage::resizeVolume(const string& device, unsigned long long newSizeK)
    {
    return resizeVolume(device, newSizeK, false);
    }

int
Storage::resizeVolumeNoFs(const string& device, unsigned long long newSizeK)
    {
    return resizeVolume(device, newSizeK, true);
    }

int
Storage::resizeVolume(const string& device, unsigned long long newSizeK,
		      bool ignoreFs)
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " newSizeK:" << newSizeK << " ignoreFs:" << ignoreFs);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	y2mil( "vol:" << *vol );
	if( ignoreFs )
	    vol->setIgnoreFs();
	ret = cont->resizeVolume(&(*vol), newSizeK);
	eraseCachedFreeInfo(vol->device());
	y2mil( "vol:" << *vol );
	}
    else
	{
	ret = STORAGE_VOLUME_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::forgetResizeVolume( const string& device )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::removeVolume( const string& device )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device);

    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol ) )
	{
	if (!vol->isUsedBy() || recursiveRemove)
	    {
	    string vdev = vol->device();
	    if (vol->isUsedBy())
		ret = removeUsing( vdev, vol->getUsedBy() );
	    if( ret==0 )
		ret = cont->removeVolume( &(*vol) );
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
    y2mil("ret:" << ret);
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
    ConstLvmVgIterator i = findLvmVg( name );
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::removeLvmVg( const string& name )
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name);
    LvmVgIterator i = findLvmVg( name );
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	ret = i->removeVg();
	if( ret==0 && i->created() )
	    ret = removeContainer( &(*i) );
	}
    else
	{
	ret = STORAGE_LVM_VG_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::extendLvmVg( const string& name, const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil( "name:" << name << " devices:" << devs );
    LvmVgIterator i = findLvmVg( name );
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::shrinkLvmVg( const string& name, const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil( "name:" << name << " devices:" << devs );
    LvmVgIterator i = findLvmVg( name );
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::createLvmLv(const string& vg, const string& name,
		     unsigned long long sizeK, unsigned stripes,
		     string& device)
    {
    int ret = 0;
    assertInit();
    y2mil("vg:" << vg << " name:" << name << " sizeK:" << sizeK << " stripes:" << stripes);
    LvmVgIterator i = findLvmVg( vg );
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	ret = i->createLv(name, sizeK, stripes, device);
	if( ret==0 )
	    checkPwdBuf( device );
	}
    else
	{
	ret = STORAGE_LVM_VG_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret << " device:" << (ret?"":device));
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::removeLvmLv( const string& vg, const string& name )
    {
    int ret = 0;
    assertInit();
    y2mil("vg:" << vg << " name:" << name);
    LvmVgIterator i = findLvmVg( vg );
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::changeLvStripeCount( const string& vg, const string& name,
			      unsigned long stripe )
    {
    int ret = 0;
    assertInit();
    y2mil("vg:" << vg << " name:" << name << " stripe:" << stripe);
    LvmVgIterator i = findLvmVg( vg );
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	ret = i->changeStripe( name, stripe );
	}
    else
	{
	ret = STORAGE_LVM_VG_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::changeLvStripeSize( const string& vg, const string& name,
			     unsigned long long stripeSize )
    {
    int ret = 0;
    assertInit();
    y2mil("vg:" << vg << " name:" << name << " stripeSize:" << stripeSize);
    LvmVgIterator i = findLvmVg( vg );
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }


int
Storage::createLvmLvSnapshot(const string& vg, const string& origin,
			     const string& name, unsigned long long cowSizeK,
			     string& device)
{
    int ret = 0;
    device.erase();
    assertInit();
    y2mil("vg:" << vg << " origin:" << origin << " name:" << name << " cowSizeK:" << cowSizeK);
    LvmVgIterator i = findLvmVg(vg);
    if (readonly())
    {
	ret = STORAGE_CHANGE_READONLY;
    }
    else if (i != lvgEnd())
    {
	ret = i->createLvSnapshot(origin, name, cowSizeK, device);
    }
    else
    {
	ret = STORAGE_LVM_VG_NOT_FOUND;
    }
    if (ret == 0)
    {
	ret = checkCache();
    }
    y2mil("ret:" << ret << " device:" << device);
    return ret;
}


int
Storage::removeLvmLvSnapshot(const string& vg, const string& name)
{
    int ret = 0;
    assertInit();
    y2mil("vg:" << vg << " name:" << name);
    LvmVgIterator i = findLvmVg(vg);
    if (readonly())
    {
	ret = STORAGE_CHANGE_READONLY;
    }
    else if (i != lvgEnd())
    {
	ret = i->removeLvSnapshot(name);
    }
    else
    {
	ret = STORAGE_LVM_VG_NOT_FOUND;
    }
    if (ret == 0)
    {
	ret = checkCache();
    }
    y2mil("ret:" << ret);
    return ret;
}


int
Storage::getLvmLvSnapshotStateInfo(const string& vg, const string& name, 
				   LvmLvSnapshotStateInfo& info)
{
    int ret = 0;
    assertInit();
    y2mil("vg:" << vg << " name:" << name);
    LvmVgIterator i = findLvmVg(vg);
    if (i != lvgEnd())
    {
	ret = i->getLvSnapshotState(name, info);
    }
    else
    {
	ret = STORAGE_LVM_VG_NOT_FOUND;
    }
    y2mil("ret:" << ret);
    return ret;
}


int
Storage::nextFreeMd(int &nr, string &device)
{
    int ret = 0;
    assertInit();
    MdCo *md = NULL;
    list<int> mdNums;
    list<int> mdPartNums;

    nr = -1;
    mdNums.clear();
    mdPartNums.clear();
    if (haveMd(md))
	md->usedNumbers(mdNums);

    getMdPartMdNums(mdPartNums);

    mdPartNums.merge(mdNums);
    mdPartNums.sort();
    mdPartNums.unique();

    if (mdPartNums.size() > 0)
    {
	bool found;
	//FIXME: magic number
	for (int i = 0; i < 1000; ++i)
	{
	    found = false;
	    for (list<int>::const_iterator it = mdPartNums.begin(); it != mdPartNums.end(); ++it)
	    {
		if (i == *it )
		    found = true;
	    }
	    if (!found)
	    {
		// Number not found on the list.
		nr = i;
		break;
	    }
	}
    }
    else
    {
	nr = 0;
    }
    if( nr != -1 )
    {
	device = "/dev/md" + decString(nr);
	y2mil("ret:" << ret << " nr:" << nr << " device:" << device);
    }
    else
	ret = MD_UNKNOWN_NUMBER;
    return ret;
}


bool Storage::checkMdNumber(int num)
{
    assertInit();
    MdCo *md = NULL;
    list<int> mdNums;
    list<int> mdPartNums;

    mdNums.clear();
    mdPartNums.clear();
    if (haveMd(md))
	md->usedNumbers(mdNums);

    getMdPartMdNums(mdPartNums);

    mdPartNums.merge(mdNums);
    mdPartNums.sort();
    mdPartNums.unique();

    if (mdPartNums.size() == 0)
	return false;

    for (list<int>::const_iterator it=mdPartNums.begin(); it!=mdPartNums.end(); ++it)
    {
        if (num == *it )
	    return true;
    }

    return false;
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
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 && !Md::mdStringNum( name, num ))
	{
	ret = STORAGE_MD_INVALID_NAME;
	}
    if( ret==0 && checkMdNumber(num)==true )
        {
        ret = MD_DUPLICATE_NUMBER;
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
	if( ret==0 )
	    checkPwdBuf( Md::mdDevice(num) );
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::createMdAny( MdType rtype, const deque<string>& devs,
			  string& device )
    {
    int ret = 0;
    assertInit();
    y2mil( "MdType:" << Md::pName(rtype) << " devices:" << devs );
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    MdCo *md = NULL;
    bool have_md = true;
    int mdNum=0;
    unsigned num = 0;
    string tmpStr;
    if( ret==0 )
	{
	have_md = haveMd(md);
	if( !have_md )
	    md = new MdCo( this, false );
	if( md==NULL )
	    ret = STORAGE_MEMORY_EXHAUSTED;
	if( ret == 0 )
	  {
	  ret = nextFreeMd(mdNum,tmpStr);
	  if( ret == 0 )
	      num = (unsigned)mdNum;
	  }
	}
    if( ret==0 )
	{
	list<string> d;
	d.insert( d.end(), devs.begin(), devs.end() );
	ret = md->createMd( num, rtype, d );
	if( ret==0 )
	    checkPwdBuf( Md::mdDevice(num) );
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
    y2mil("ret:" << ret << " device:" << (ret==0?device:""));
    return( ret );
    }

int Storage::removeMd( const string& name, bool destroySb )
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name << " destroySb:" << destroySb);
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::extendMd( const string& name, const string& dev )
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name << " dev:" << dev);
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::shrinkMd( const string& name, const string& dev )
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name << " dev:" << dev);
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::changeMdType( const string& name, MdType rtype )
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name << " rtype:" << rtype);
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::changeMdChunk( const string& name, unsigned long chunk )
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name << " chunk:" << chunk);
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::changeMdParity( const string& name, MdParity ptype )
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name << " parity:" << ptype);
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::checkMd( const string& name )
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name);
    unsigned num = 0;
    MdCo *md = NULL;
    if( Md::mdStringNum( name, num ) && haveMd(md) )
	ret = md->checkMd(num);
    else
	ret = STORAGE_MD_NOT_FOUND;
    y2mil("ret:" << ret);
    return( ret );
    }


int Storage::getMdStateInfo(const string& name, MdStateInfo& info)
{
    int ret = 0;
    assertInit();
    y2mil("name:" << name);
    unsigned num = 0;
    if (ret == 0 && !Md::mdStringNum(name, num))
    {
	ret = STORAGE_MD_INVALID_NAME;
    }
    if (ret == 0)
    {
	MdCo *md = NULL;
	if (haveMd(md))
	    ret = md->getMdState(num, info);
	else
	    ret = STORAGE_MD_NOT_FOUND;
    }
    y2mil("ret:" << ret);
    return ret;
}


int
Storage::getMdPartCoStateInfo(const string& name, MdPartCoStateInfo& info)
{
    int ret = 0;

    ConstMdPartCoIterator i = findMdPartCo(name);
    if (i != mdpCoEnd())
    { 
	i->getMdPartCoState(info);
    }
    else
    {
	ret = STORAGE_MDPART_CO_NOT_FOUND;
    }

    return ret;
}


int
Storage::computeMdSize(MdType md_type, list<string> devices, unsigned long long& sizeK)
{
    int ret = 0;

    unsigned long long sumK = 0;
    unsigned long long smallestK = 0;

    for (list<string>::const_iterator i = devices.begin(); i != devices.end(); ++i)
    {
	const Volume* v = getVolume(*i);
	if (!v)
	{
	    ret = STORAGE_VOLUME_NOT_FOUND;
	    break;
	}

	sumK += v->sizeK();
	if (smallestK == 0)
	    smallestK = v->sizeK();
	else
	    smallestK = min(smallestK, v->sizeK());
    }

    if (ret == 0)
    {
	switch (md_type)
	{
	    case RAID0:
		if (devices.size() < 2)
		    ret = MD_TOO_FEW_DEVICES;
		else
		    sizeK = sumK;
		break;

	    case RAID1:
	    case MULTIPATH:
		if (devices.size() < 2)
		    ret = MD_TOO_FEW_DEVICES;
		else
		    sizeK = smallestK;
		break;

	    case RAID5:
		if (devices.size() < 3)
		    ret = MD_TOO_FEW_DEVICES;
		else
		    sizeK = smallestK*(devices.size()-1);
		break;

	    case RAID6:
		if (devices.size() < 4)
		    ret = MD_TOO_FEW_DEVICES;
		else
		    sizeK = smallestK*(devices.size()-2);
		break;

	    case RAID10:
		if (devices.size() < 2)
		    ret = MD_TOO_FEW_DEVICES;
		else
		    sizeK = smallestK*devices.size()/2;
		break;

	    default:
		break;
	}
    }

    y2mil("type:" << md_type << " smallest:" << smallestK << " sum:" << sumK << " size:" << sizeK);

    return ret;
}

//
// Removes Software RAIDs that are not IMSM RAIDs.
//
int Storage::removeMdPartCo(const string& devName, bool destroySb)
{
  y2mil("Called");
  int ret;
  MdPartCoIterator mdp = findMdPartCo(devName);
  if( mdp == mdpCoEnd()  )
    {
	y2war("Not found device: " << devName);
     return MDPART_DEVICE_NOT_FOUND;
    }
  MdPartCoInfo mdpInfo;
  mdp->getInfo(mdpInfo);
  if( mdpInfo.sb_ver == "imsm" || mdpInfo.sb_ver == "ddf" )
    {
     y2war("IMSM or DDF RAID cannot be removed");
     return MDPART_NO_REMOVE;
    }
  //Remove all: Md Partitions and RAID itself.
  ret = mdp->removeMdPart();
  if( ret==0 )
    {
    ret = checkCache();
    }
  y2mil("ret:" << ret);
  return ret;
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

int Storage::getMdPartMdNums(list<int>& mdPartNums)
    {
    mdPartNums.clear();
    CPair p = cPair();
    for (ConstContIterator i = p.begin(); i != p.end(); ++i)
       {
        if( i->type()==MDPART )
          {
          const MdPartCo* mdpart = static_cast<const MdPartCo*>(&(*i));
          mdPartNums.push_back(mdpart->nr());
          }
         }
    return 0;
    }

    bool
    Storage::haveDm(DmCo*& dm)
    {
	dm = NULL;
	CPair p = cPair();
	ContIterator i = p.begin();
	while (i != p.end() && i->type() != DM)
	    ++i;
	if (i != p.end())
	    dm = static_cast<DmCo*>(&(*i));
	return i != p.end();
    }


bool Storage::haveNfs( NfsCo*& co )
    {
    co = NULL;
    CPair p = cPair();
    ContIterator i = p.begin();
    while( i != p.end() && i->type()!=NFSC )
	++i;
    if( i != p.end() )
	co = static_cast<NfsCo*>(&(*i));
    return( i != p.end() );
    }

int 
Storage::addNfsDevice(const string& nfsDev, const string& opts, unsigned long long sizeK,
		      const string& mp, bool nfs4)
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << nfsDev << " sizeK:" << sizeK << " mp:" << mp << " nfs4:" << nfs4);
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    NfsCo *co = NULL;
    bool have = true;
    if( ret==0 )
	{
	have = haveNfs(co);
	if( !have )
	    co = new NfsCo( this );
	}
    if( ret==0 && co!=NULL )
	{
	if( sizeK==0 )
	    checkNfsDevice(nfsDev, opts, nfs4, sizeK);
	ret = co->addNfs(nfsDev, sizeK, mp, nfs4);
	}
    if( !have )
	{
	if( ret==0 )
	    addToList( co );
	else if( co!=NULL )
	    delete co;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }


int
Storage::checkNfsDevice(const string& nfsDev, const string& opts, bool nfs4, unsigned long long& sizeK)
    {
    int ret = 0;
    assertInit();
    NfsCo co( this );
    string mdir = tmpDir() + "/tmp-nfs-mp";
    mkdir(mdir.c_str(), 0777);
    ret = co.addNfs(nfsDev, 0, "", nfs4);
    if( !opts.empty() )
	co.vBegin()->setFstabOption( opts );
    if( instsys() )
	{
	SystemCmd c;
	string prog_name = "/sbin/rpcbind";

	//We don't have rpcbind (#423026, #427428) ...
	if ( !checkNormalFile(prog_name) )
	{
	    //... so let's try portmap instead
	    y2mil("No rpcbind found, trying portmap instead ...");
	    prog_name = "/sbin/portmap";
	}

	c.execute( prog_name );
	c.execute( "/usr/sbin/rpc.statd" );
	}
    if( ret==0 && (ret=co.vBegin()->mount( mdir ))==0 )
	{
	StatVfs vfsbuf;
	getStatVfs(mdir, vfsbuf);
	sizeK = vfsbuf.sizeK;
	ret = co.vBegin()->umount( mdir );
	}
    rmdir(mdir.c_str());
    y2mil( "name:" << nfsDev << " opts:" << opts << " nfs4:" << nfs4 << " ret:" << ret <<
           " sizeK:" << sizeK );
    return ret;
    }

int
Storage::createFileLoop( const string& lname, bool reuseExisting,
                         unsigned long long sizeK, const string& mp,
			 const string& pwd, string& device )
    {
    int ret = 0;
    assertInit();
    y2mil("lname:" << lname << " reuseExisting:" << reuseExisting << " sizeK:" << sizeK << " mp:" << mp);
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
    y2mil("pwd:" << pwd);
#endif
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    LoopCo *loop = NULL;
    Volume *vol = NULL;
    bool have_loop = true;
    if( ret==0 )
	{
	have_loop = haveLoop(loop);
	y2mil( "have_loop:" << have_loop );
	if( !have_loop )
	    {
	    SystemInfo systeminfo;
	    loop = new LoopCo(this, false, systeminfo);
	    }
	if( loop==NULL )
	    ret = STORAGE_MEMORY_EXHAUSTED;
	}
    if( ret==0 )
	{
	ret = loop->createLoop( lname, reuseExisting, sizeK, true, device );
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
    if( ret==0 && !mp.empty() )
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
    y2mil("ret:" << ret << " device:" << (ret==0?device:""));
    return( ret );
    }

int
Storage::modifyFileLoop( const string& device, const string& lname, 
                         bool reuseExisting, unsigned long long sizeK )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " lname:" << lname << " reuse:" << reuseExisting << " sizeK:" << sizeK);
    if (readonly())
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
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::removeFileLoop( const string& lname, bool removeFile )
    {
    int ret = 0;
    assertInit();
    y2mil("lname:" << lname << " removeFile:" << removeFile);
    if (readonly())
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
    y2mil("ret:" << ret);
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

int Storage::removeDmraid( const string& name )
    {
    int ret = 0;
    assertInit();
    DmraidCoIterator i = findDmraidCo( name );
    if (readonly())
    {
	ret = STORAGE_CHANGE_READONLY;
    }
    else if( i != dmrCoEnd() )
	{
	ret = i->removeDmPart();
	}
    else
	ret = STORAGE_DMRAID_CO_NOT_FOUND;
    return( ret );
    }

int Storage::checkCache()
    {
    int ret=0;
    if( !isCacheChanges() )
	ret = commit();
    return(ret);
    }


list<commitAction>
Storage::getCommitActions() const
{
    ConstContPair p = contPair();
    list<commitAction> ca;
    for (ConstContIterator it = p.begin(); it != p.end(); ++it)
    {
	list<commitAction> l;
	it->getCommitActions(l);
	ca.splice(ca.end(), l);
    }
    ca.sort();
    return ca;
}


void
Storage::getCommitInfos(list<CommitInfo>& infos) const
{
    static list<CommitInfo> s_infos; // workaround for broken ycp bindings
    s_infos.clear();

        const list<commitAction> ca = getCommitActions();
	for (list<commitAction>::const_iterator i = ca.begin(); i != ca.end(); ++i)
	{
	    CommitInfo info;
	    info.destructive = i->destructive;
	    info.text = i->description.text;
	    const Volume* v = i->vol();
	    if( v && !v->getDescText().empty() )
	    {
		info.text += ". ";
		info.text += v->getDescText();
	    }
	    s_infos.push_back(info);
	}

    infos = s_infos;
    y2mil("infos.size:" << infos.size());
}


void
Storage::dumpCommitInfos() const
{
    const list<commitAction> ca = getCommitActions();
    for (list<commitAction>::const_iterator it = ca.begin(); it != ca.end(); ++it)
    {
	string text = it->description.native;
	if (it->destructive)
	    text += " [destructive]";
	y2mil("ChangeText " << text);
    }
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
	    {
	    if( rhs->needShrink() == lhs->needShrink() )
		{
		if( rhs->cType()!=DISK || rhs->cType()!=DISK )
		    return( *rhs > *lhs );
		else
		    {
		    return( *static_cast<const Partition*>(rhs) >
		            *static_cast<const Partition*>(lhs) );
		    }
		}
	    else
		return( rhs->needShrink() );
	    }
	}
    else
	return( rhs->isMounted() );
    }

static bool sort_vol_create( const Volume* rhs, const Volume* lhs )
    {
    if( rhs->cType()==lhs->cType() )
	{
	if( rhs->cType()==LVM )
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
    else if( lhs->hasOrigMount() != rhs->hasOrigMount() )
	return( rhs->hasOrigMount() );
    else
	return( rhs->getMount()<lhs->getMount() );
    }


void
Storage::sortCommitLists(CommitStage stage, list<const Container*>& co,
			 list<const Volume*>& vl, list<commitAction>& todo) const
    {
    co.sort( (stage==DECREASE)?sort_cont_up:sort_cont_down );
    std::ostringstream b;
    classic(b);
    if( stage==DECREASE )
	{
	vl.reverse();
	vl.sort( sort_vol_delete );
	}
    else if( stage==INCREASE )
	vl.sort( sort_vol_create );
    else if( stage==MOUNT )
	vl.sort( sort_vol_mount );
    else
	vl.sort( sort_vol_normal );
    for( list<const Container*>::const_iterator i=co.begin(); i!=co.end(); ++i )
	todo.push_back(commitAction(stage, (*i)->type(), *i));
    for( list<const Volume*>::const_iterator i=vl.begin(); i!=vl.end(); ++i )
	todo.push_back(commitAction(stage, (*i)->cType(), *i));
    b.str("");
    b << "unsorted actions <";
    for (list<commitAction>::const_iterator i = todo.begin(); i != todo.end(); ++i)
	{
	if( i!=todo.begin() )
	    b << " ";
	if( i->container )
	    b << "C:" << i->co()->device();
	else
	    b << "V:" << i->vol()->device();
	}
    b << "> ";
    y2mil(b.str());
    b.str("");
    todo.sort();
    y2mil("stage:" << stage);
    b << "sorted co <";
    for( list<const Container*>::const_iterator i=co.begin(); i!=co.end(); ++i )
	{
	if( i!=co.begin() )
	    b << " ";
	b << (*i)->name();
	}
    b << "> ";
    y2mil(b.str());
    b.str("");
    b << "sorted vol <";
    for( list<const Volume*>::const_iterator i=vl.begin(); i!=vl.end(); ++i )
	{
	if( i!=vl.begin() )
	    b << " ";
	b << (*i)->device();
	}
    b << "> ";
    y2mil(b.str());
    b.str("");
    b << "sorted actions <";
    for (list<commitAction>::const_iterator i = todo.begin(); i != todo.end(); ++i)
	{
	if( i!=todo.begin() )
	    b << " ";
	if( i->container )
	    b << "C:" << i->co()->device();
	else
	    b << "V:" << i->vol()->device();
	}
    b << "> ";
    y2mil(b.str());
    }

void Storage::handleHald( bool stop )
    {
    y2mil( "stop:" << stop );
    int ret;
    if( stop )
	{
	hald_pid = 0;
	SystemCmd c( "ps ax | grep -w /usr/sbin/hald | grep -v grep" );
	if( c.numLines()>0 )
	    {
	    extractNthWord( 0, c.getLine(0) ) >> hald_pid;
	    y2mil( "hald_pid:" << hald_pid );
	    }
	if( hald_pid>0 )
	    {
	    ret = kill( hald_pid, SIGSTOP );
	    y2mil( "ret kill:" << ret << " pid:" << hald_pid );
	    }
	}
    else if( !stop && hald_pid>0 )
	{
	ret = kill( hald_pid, SIGCONT );
	y2mil( "ret kill:" << ret << " pid:" << hald_pid );
	}
    }


static bool notLoop( const Container& c ) { return( c.type()!=LOOP ); }
static bool fstabAdded( const Volume& v ) { return( v.fstabAdded()); }

int Storage::commit()
    {
    assertInit();
    lastAction.clear();
    extendedError.clear();
    dumpCommitInfos();
    CPair p = cPair( notLoop );
    int ret = 0;
    y2mil("empty:" << p.empty());
    if( !p.empty() )
	{
	ret = commitPair( p, notLoop );
	}
    p = cPair( isLoop );
    y2mil("empty:" << p.empty());
    if( ret==0 && !p.empty() )
	{
	ret = commitPair( p, isLoop );
	}
    VPair vp = vPair( fstabAdded );
    for( VolIterator i=vp.begin(); i!=vp.end(); ++i )
	i->setFstabAdded(false);
    if( ret!=0 )
	dumpObjectList();
    y2mil("ret:" << ret);
    return( ret );
    }


string
Storage::getErrorString(int error) const
{
    switch (error)
    {
	case VOLUME_UMOUNT_FAILED:
	    return _("Unmount failed.").text;

	default:
	    return "";
    }
}


bool
Storage::ignoreError(int error, list<commitAction>::const_iterator ca) const
{
    bool ret = commitErrorPopupCb(error, lastAction, extendedError);

    if (ret)
	y2mil("user decided to continue after commit error");

    return ret;
}


int
Storage::commitPair( CPair& p, bool (* fnc)( const Container& ) )
    {
    int ret = 0;
    y2mil("p.length:" << p.length());

    typedef array<CommitStage, 4> Stages;
    const Stages stages = { { DECREASE, INCREASE, FORMAT, MOUNT } };

    for (Stages::const_iterator stage = stages.begin(); stage != stages.end(); ++stage)
    {
	list<const Container*> colist;
	list<const Volume*> vlist;

	if (ret == 0)
	{
	    for (ContIterator i = p.begin(); i != p.end(); ++i)
		i->getToCommit(*stage, colist, vlist);
	}

	bool new_pair = false;
	list<commitAction> todo;
	sortCommitLists(*stage, colist, vlist, todo);
	list<commitAction>::iterator ac = todo.begin();
	while( ret==0 && ac != todo.end() )
	    {
	    bool cont = ac->container;
	    CType type = ac->type;
	    Container *co = cont ? const_cast<Container*>(ac->co()) : 
	                           const_cast<Container*>(ac->vol()->getContainer());
	    if( cont )
		{
		bool cont_removed = co->deleted() && (type == LVM || type == MDPART);
		ret = co->commitChanges(*stage);
		cont_removed = cont_removed && ret==0;
		if( cont_removed )
		    {
		    removeContainer( co );
		    new_pair = true;
		    }
		}
	    else
		{
		ret = co->commitChanges(*stage, const_cast<Volume*>(ac->vol()));
		}
	    if( ret!=0 )
		{
		y2mil("err at " << *ac);
		if (ignoreError(ret, ac))
		    ret = 0;
		}
	    ++ac;
	    }
	y2mil("stage:" << *stage << " new_pair:" << new_pair);
	if( new_pair )
	    {
	    p = cPair( fnc );
	    new_pair = false;
	    }
	if( !todo.empty() )
	    {
	    SystemCmd c;
	    c.execute(DMSETUPBIN " ls");
	    c.execute(DMSETUPBIN " table");
	    c.execute(DMSETUPBIN " info");
	    logProcData();
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }


static bool isDmContainer( const Container& co )
{
    return co.type()==DM || co.type()==LVM || co.type()==DMRAID || co.type()==DMMULTIPATH;
}


bool Storage::removeDmMapsTo( const string& dev )
    {
    bool ret = false;
    y2mil("dev:" << dev);
    VPair vp = vPair( isDmContainer );
    for( VolIterator v=vp.begin(); v!=vp.end(); ++v )
	{
	Dm * dm = dynamic_cast<Dm *>(&(*v));
	if (dm != NULL)
	    {
	    y2mil( "dm:" << *dm );
	    if( dm->mapsTo( dev ) )
		{
		dm->removeTable();
		if( !dm->created() )
		    {
		    dm->setSilent();
		    dm->setDeleted();
		    }
		ret = true;
		}
	    }
	else
	    y2war("not a Dm descendant " << v->device());
	}
    ConstVolIterator v;
    ConstDiskIterator d;
    if( findVolume( dev, v ))
	{
	v->triggerUdevUpdate();
	}
    else if( (d=findDisk( dev ))!=dEnd())
	{
	d->triggerUdevUpdate();
	}
    waitForDevice();
    y2mil("ret:" << ret);
    return( ret );
    }

void Storage::updateDmEmptyPeMap()
    {
    VPair vp = vPair( isDmContainer );
    for( VolIterator v=vp.begin(); v!=vp.end(); ++v )
	{
	Dm * dm = dynamic_cast<Dm *>(&(*v));
	if( dm!=NULL )
	    {
	    if( dm->getPeMap().empty() )
		{
		y2mil( "dm:" << *dm );
		dm->getTableInfo();
		dm->updateMajorMinor();
		}
	    }
	else
	    y2war("not a Dm descendant " << v->device());
	}
    }

bool Storage::checkDmMapsTo( const string& dev )
    {
    bool ret = false;
    y2mil("dev:" << dev);
    VPair vp = vPair( isDmContainer );
    ConstVolIterator v=vp.begin(); 
    while( !ret && v!=vp.end() )
	{
	const Dm* dm = dynamic_cast<const Dm*>(&(*v));
	if( dm!=NULL )
	    ret = ret && dm->mapsTo( dev );
	++v;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

void Storage::changeDeviceName( const string& old, const string& nw )
    {
    y2mil( "old:" << old << " new:" << nw );
    CPair p = cPair();
    ContIterator ci = p.begin();
    while( ci!=p.end() )
	{
	ci->changeDeviceName( old, nw );
	++ci;
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
Storage::getContVolInfo(const string& device, ContVolInfo& info)
    {
    int ret = STORAGE_VOLUME_NOT_FOUND;
    string dev = device;
    ConstContIterator c;
    ConstVolIterator v;
    info.type = CUNKNOWN;
    assertInit();
    if( findVolume( dev, c, v ))
	{
	ret = 0;
	info.type = c->type();
	info.cname = c->device();
	info.vname = v->name();
	info.numeric = v->isNumeric();
	if( info.numeric )
	    info.nr = v->nr();
	else
	    info.nr = 0;
	}
    else 
	{
	DiskIterator d;
	DmraidCoIterator r;
	DmmultipathCoIterator m;
	MdPartCoIterator md;
	std::pair<string,unsigned> p = Disk::getDiskPartition( dev );
	if( p.first=="/dev/md" )
	    {
	    ret = 0;
	    info.cname = p.first;
	    info.vname = undevDevice(device);
	    info.type = MD;
	    info.numeric = true;
	    info.nr = p.second;
	    }
	else if( p.first=="/dev/loop" )
	    {
	    ret = 0;
	    info.cname = p.first;
	    info.vname = undevDevice(device);
	    info.type = LOOP;
	    info.numeric = true;
	    info.nr = p.second;
	    }
	else if( p.first=="/dev/dm-" )
	    {
	    ret = 0;
	    info.cname = p.first;
	    info.vname = undevDevice(device);
	    info.type = DM;
	    info.numeric = true;
	    info.nr = p.second;
	    }
	else if( (d=findDisk(p.first))!=dEnd() )
	    {
	    ret = 0;
	    info.cname = d->device();
	    info.vname = dev.substr( dev.find_last_of('/')+1 );
	    info.type = DISK;
	    info.numeric = true;
	    info.nr = p.second;
	    }
	else if( (r=findDmraidCo(p.first))!=dmrCoEnd() )
	    {
	    ret = 0;
	    info.cname = r->device();
	    info.vname = dev.substr( dev.find_last_of('/')+1 );
	    info.type = DMRAID;
	    info.numeric = true;
	    info.nr = p.second;
	    }
        else if( (md=findMdPartCo(p.first))!=mdpCoEnd() )
            {
            info.cname = md->device();
            info.vname = dev.substr( dev.find_last_of('/')+1 );
            info.type = MDPART;
            info.numeric = true;
            info.nr = p.second;
            }
	else if( (m=findDmmultipathCo(p.first))!=dmmCoEnd() )
	    {
	    ret = 0;
	    info.cname = m->device();
	    info.vname = dev.substr( dev.find_last_of('/')+1 );
	    info.type = DMMULTIPATH;
	    info.numeric = true;
	    info.nr = p.second;
	    }
	else if( dev.find("/dev/disk/by-uuid/")==0 ||
	         dev.find("/dev/disk/by-label/")==0 ||
	         dev.find("UUID=")==0 || dev.find("LABEL=")==0 )
	    {
	    if( dev[0] == '/' )
		{
		bool uuid = dev.find( "/by-uuid/" )!=string::npos;
		dev.erase( 0, dev.find_last_of('/')+1 );
		dev = (uuid?"UUID=":"LABEL=")+dev;
		}
	    if( findVolume(dev, v) )
		{
		ret = 0;
		info.type = v->cType();
		info.numeric = v->isNumeric();
		if( info.numeric )
		    info.nr = v->nr();
		info.vname = v->name();
		info.cname = v->getContainer()->name();
		}
	    }
	else if( (dev.find("/dev/disk/by-id/")==0 &&
	          (d=findDiskId(p.first))!=dEnd()) ||
		 (dev.find("/dev/disk/by-path/")==0 &&
		  (d=findDiskPath(p.first))!=dEnd()) )
	    {
	    ret = 0;
	    info.type = DISK;
	    info.numeric = true;
	    info.nr = p.second;
	    info.cname = d->device();
	    if( p.second>0 )
		info.vname = Disk::getPartName( d->name(), p.second );
	    else
		info.vname = d->name();
	    if( info.vname.find('/')!=string::npos )
		info.vname.erase( 0, info.vname.find_last_of('/')+1 );
	    }
	else if( splitString( dev, "/" ).size()==3 && !Disk::needP( dev ) )
	    {
	    ret = 0;
	    info.type = LVM;
	    info.numeric = false;
	    info.vname = dev.substr( dev.find_last_of('/')+1 );
	    info.cname = dev.substr( 0, dev.find_last_of('/') );
	    }
	else
	    {
	    ret = 0;
	    info.cname = p.first;
	    info.vname = dev.substr( dev.find_last_of('/')+1 );
	    info.numeric = true;
	    info.nr = p.second;
	    }
	}
    y2mil( "dev:" << dev << " ret:" << ret << " cn:" << info.cname << 
           " vn:" << info.vname );
    if( info.numeric )
	y2mil( "nr:" << info.nr );
    return( ret );
    }

int
Storage::getVolume( const string& device, VolumeInfo& info )
    {
    int ret = 0;
    ConstVolIterator v;
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
    ConstDiskIterator i = findDisk( disk );
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
    ConstDiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	i->Container::getInfo(cinfo);
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
    ConstDiskIterator i = findDisk(disk);
    if (i != dEnd())
    {
	// TODO: those partitions shouldn't be detected at all
	if (!i->isUsedBy())
	{
	    Disk::ConstPartPair p = i->partPair(Disk::notDeleted);
	    for (Disk::ConstPartIter i2 = p.begin(); i2 != p.end(); ++i2)
	    {
		plist.push_back(PartitionInfo());
		i2->getInfo(plist.back());
	    }
	}
    }
    else
	ret = STORAGE_DISK_NOT_FOUND;
    return ret;
    }

int Storage::getLvmVgInfo( const string& name, LvmVgInfo& info )
    {
    int ret = 0;
    assertInit();
    ConstLvmVgIterator i = findLvmVg( name );
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
    ConstLvmVgIterator i = findLvmVg( name );
    if( i != lvgEnd() )
	{
	i->Container::getInfo(cinfo);
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
    ConstLvmVgIterator i = findLvmVg( name );
    if( i != lvgEnd() )
	{
	LvmVg::ConstLvmLvPair p = i->lvmLvPair(LvmVg::lvNotDeleted);
	for( LvmVg::ConstLvmLvIter i2 = p.begin(); i2 != p.end(); ++i2)
	    {
	    plist.push_back( LvmLvInfo() );
	    i2->getInfo( plist.back() );
	    }
	}
    else
	ret = STORAGE_LVM_VG_NOT_FOUND;
    return( ret );
    }


int Storage::getDmraidCoInfo( const string& name, DmraidCoInfo& info )
    {
    int ret = 0;
    assertInit();
    ConstDmraidCoIterator i = findDmraidCo( name );
    if( i != dmrCoEnd() )
	{
	i->getInfo( info );
	}
    else
	ret = STORAGE_DMRAID_CO_NOT_FOUND;
    return( ret );
    }

int Storage::getContDmraidCoInfo( const string& name, ContainerInfo& cinfo,
			          DmraidCoInfo& info )
    {
    int ret = 0;
    assertInit();
    ConstDmraidCoIterator i = findDmraidCo( name );
    if( i != dmrCoEnd() )
	{
	i->Container::getInfo(cinfo);
	i->getInfo( info );
	}
    else
	ret = STORAGE_DMRAID_CO_NOT_FOUND;
    return( ret );
    }


int
Storage::getDmmultipathCoInfo( const string& name, DmmultipathCoInfo& info )
{
    int ret = 0;
    assertInit();
    ConstDmmultipathCoIterator i = findDmmultipathCo( name );
    if( i != dmmCoEnd() )
    {
	i->getInfo( info );
    }
    else
	ret = STORAGE_DMMULTIPATH_CO_NOT_FOUND;
    return( ret );
}

int
Storage::getContDmmultipathCoInfo( const string& name, ContainerInfo& cinfo,
				   DmmultipathCoInfo& info )
{
    int ret = 0;
    assertInit();
    ConstDmmultipathCoIterator i = findDmmultipathCo( name );
    if( i != dmmCoEnd() )
    {
	i->Container::getInfo(cinfo);
	i->getInfo( info );
    }
    else
	ret = STORAGE_DMMULTIPATH_CO_NOT_FOUND;
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
	}
    return( ret );
    }

int Storage::getMdPartCoInfo( const string& name, MdPartCoInfo& info)
{
  int ret = 0;
  assertInit();
  ConstMdPartCoIterator i = findMdPartCo( name );
  if( i != mdpCoEnd() )
      {
      i->getInfo( info );
      }
  else
      ret = STORAGE_MDPART_CO_NOT_FOUND;
  return( ret );
}

int Storage::getContMdPartCoInfo( const string& name, ContainerInfo& cinfo,
                                 MdPartCoInfo& info)
{
  int ret = 0;
  assertInit();
  ConstMdPartCoIterator i = findMdPartCo( name );
  if( i != mdpCoEnd() )
      {
      i->Container::getInfo(cinfo);
      i->getInfo( info );
      }
  else
      ret = STORAGE_MDPART_CO_NOT_FOUND;
  return( ret );

}


int Storage::getMdPartInfo( const string& device, deque<MdPartInfo>& plist )
{
  int ret = 0;
  plist.clear();
  assertInit();
  MdPartCoIterator it = findMdPartCo(device);

  if( it != mdpCoEnd() )
    {
    MdPartCo::MdPartPair p = it->mdpartPair(MdPart::notDeleted);

    for( MdPartCo::MdPartIter i2 = p.begin(); i2 != p.end(); ++i2 )
      {
      plist.push_back( MdPartInfo() );
      i2->getInfo( plist.back() );
      }
    }
  else
    ret = STORAGE_MDPART_CO_NOT_FOUND;
  return( ret );
}


int Storage::getNfsInfo( deque<storage::NfsInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    ConstNfsPair p = nfsPair(Nfs::notDeleted);
    for( ConstNfsIterator i = p.begin(); i != p.end(); ++i )
	{
	plist.push_back( NfsInfo() );
	i->getInfo( plist.back() );
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
	}
    return( ret );
    }

int Storage::getDmraidInfo( const string& name,
                            deque<storage::DmraidInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    DmraidCoIterator i = findDmraidCo( name );
    if( i != dmrCoEnd() )
	{
	DmraidCo::DmraidPair p = i->dmraidPair(DmraidCo::raidNotDeleted);
	for( DmraidCo::DmraidIter i2 = p.begin(); i2 != p.end(); ++i2 )
	    {
	    plist.push_back( DmraidInfo() );
	    i2->getInfo( plist.back() );
	    }
	}
    else
	ret = STORAGE_DMRAID_CO_NOT_FOUND;
    return( ret );
    }


int
Storage::getDmmultipathInfo( const string& name,
			     deque<storage::DmmultipathInfo>& plist )
{
    int ret = 0;
    plist.clear();
    assertInit();
    ConstDmmultipathCoIterator i = findDmmultipathCo( name );
    if( i != dmmCoEnd() )
    {
	DmmultipathCo::ConstDmmultipathPair p = i->dmmultipathPair(DmmultipathCo::multipathNotDeleted);
	for( DmmultipathCo::ConstDmmultipathIter i2 = p.begin(); i2 != p.end(); ++i2 )
	{
	    plist.push_back( DmmultipathInfo() );
	    i2->getInfo( plist.back() );
	}
    }
    else
	ret = STORAGE_DMMULTIPATH_CO_NOT_FOUND;
    return( ret );
}


list<string> Storage::getAllUsedFs() const 
{
    set<FsType> fs;
    ConstVolPair p = volPair( Volume::notDeleted );
    for( ConstVolIterator v=p.begin(); v!=p.end(); ++v )
    {
	FsType t = v->getFs();
	if (t!=FSUNKNOWN && t!=FSNONE)
	    fs.insert(t);
    }
    list<string> ret;
    for( set<FsType>::const_iterator i=fs.begin(); i!=fs.end(); ++i )
    {
	ret.push_back(Volume::fsTypeString(*i));
    }
    y2mil( "ret:" << ret );
    return ret;
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

    static FsCapabilitiesX ext3Caps (true, true, true, false, true, true,
				     true, 16, 10*1024);

    static FsCapabilitiesX ext4Caps (true, true, true, false, true, true,
				     true, 16, 32*1024);

    static FsCapabilitiesX btrfsCaps (false, false, false, false, true, false,
				      false, 0, 256*1024);

    static FsCapabilitiesX xfsCaps (true, true, false, false, true, true,
				    false, 12, 40*1024);

    static FsCapabilitiesX ntfsCaps (true, false, true, false, false, false,
				     false, 0, 10*1024);

    static FsCapabilitiesX fatCaps (true, false, true, false, false, false,
				    false, 0, 16);

    static FsCapabilitiesX swapCaps (true, false, true, false, true, true,
				     false, 16, 64);

    static FsCapabilitiesX jfsCaps (false, false, false, false, true, true,
				    false, 16, 16*1024);

    static FsCapabilitiesX hfsCaps (false, false, true, false, false, false,
				    false, 0, 10*1024);

    static FsCapabilitiesX hfspCaps(false, false, true, false, false, false,
				    false, 0, 10*1024);

    static FsCapabilitiesX nfsCaps (false, false, false, false, false, false,
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

	case EXT4:
	    fscapabilities = ext4Caps;
	    return true;

	case BTRFS:
	    fscapabilities = btrfsCaps;
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

	case HFSPLUS:
	    fscapabilities = hfspCaps;
	    return true;

	case NFS:
	    fscapabilities = nfsCaps;
	    return true;

	case SWAP:
	    fscapabilities = swapCaps;
	    return true;

	default:
	    return false;
    }
}


bool
Storage::getDlabelCapabilities(const string& dlabel, DlabelCapabilities& dlabelcapabilities) const
{
    return Disk::getDlabelCapabilities(dlabel, dlabelcapabilities);
}


void Storage::removeDmTableTo( const Volume& vol )
    {
    if( vol.cType()==DISK || vol.cType()==MD )
	{
	removeDmTable( Dm::devToTable(vol.device()));
	y2mil( "dev:" << vol.device() );
	removeDmMapsTo( vol.device() );
	if( vol.cType()==DISK )
	    {
	    removeDmMapsTo( vol.getContainer()->device() );
	    if( vol.getContainer()->majorNr()>0 )
		{
		string cmd = DMSETUPBIN " table | grep -w ";
		cmd += decString(vol.getContainer()->majorNr()) + ":" +
		       decString(vol.getContainer()->minorNr());
		cmd += " | sed s/:.*// | uniq";
		SystemCmd c( cmd );
		unsigned line=0;
		while( line<c.numLines() )
		    {
		    removeDmTable( c.getLine(line) );
		    line++;
		    }
		}
	    }
	}
    }
    
void Storage::removeDmTableTo( const string& device )
    {
    y2mil( "dev:" << device );
    VolIterator vol;
    if( findVolume( device, vol ))
	removeDmTableTo( *vol );
    }
    
bool Storage::removeDmTable( const string& table )
    {
    SystemCmd c(DMSETUPBIN " table " + quote(table));
    bool ret = false;
    if( c.retcode()==0 )
	{
	c.execute(DMSETUPBIN " info " + quote(table));
	c.execute(DMSETUPBIN " remove " + quote(table));
	waitForDevice();
	ret = c.retcode()==0;
	c.execute(DMSETUPBIN " table | grep " + quote(table));
	logProcData();
	}
    y2mil("ret:" << ret);
    return( ret );
    }


void
Storage::logCo(const Container* c) const
{
    std::ostringstream b;
    classic(b);
    c->print( b );
    y2mil( "log co:" << b.str() );
    for( Container::ConstPlainIterator i=c->begin(); i!=c->end(); ++i )
    {
	b.str("");
	(*i)->print( b );
	y2mil( "log vo:" << b.str() );
    }
}


    void
    Storage::logProcData(const string& str) const
    {
	y2mil("begin:" << str);

	if (!testmode())
	{
	AsciiFile("/proc/partitions").logContent();
	AsciiFile("/proc/mdstat").logContent();
	AsciiFile("/proc/mounts").logContent();
	AsciiFile("/proc/swaps").logContent();
	}

	y2mil("end" << str);
    }

bool Storage::findVolume( const string& device, ConstContIterator& c,
                          ConstVolIterator& v )
    {
    ContIterator ct;
    VolIterator vt;
    bool ret = findVolume( device, ct, vt );
    if( ret )
	{
	c = ct;
	v = vt;
	}
    return( ret );
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
    y2mil("device:" << device << " ret:" << ret << " c->device:" << (ret?c->device():"NULL") << 
	  " v->device:" << (ret?v->device():"NULL"));
    return( ret );
    }

bool Storage::findDm( const string& device, const Dm*& dm )
    {
    bool ret = false;
    ConstDmPair p = dmPair();
    ConstDmIterator i = p.begin();
    while( !ret && i!=p.end() )
	{
	ret = i->device()==device ||
	      find( i->altNames().begin(), i->altNames().end(), device )!=
		  i->altNames().end();
	if( !ret )
	    ++i;
	}
    if( ret )
	{
	dm = &(*i);
	y2mil( "dm:" << *dm );
	}
    y2mil( "device:" << device << " ret:" << ret );
    return( ret );
    }

bool Storage::findDmUsing( const string& device, const Dm*& dm )
    {
    bool ret = false;
    ConstDmPair p = dmPair();
    ConstDmIterator i = p.begin();
    while( !ret && i!=p.end() )
	{
	ret = i->usingPe(device);
	if( !ret )
	    ++i;
	}
    if( ret )
	{
	dm = &(*i);
	y2mil( "dm:" << *dm );
	}
    y2mil( "device:" << device << " ret:" << ret );
    return( ret );
    }

bool Storage::removeDm( const string& device )
    {
    const Dm* dm = 0;
    if( findDm( device, dm ))
	{
	ContIterator c;
	if( findContainer( dm->getContainer()->device(), c ))
	    {
	    c->removeFromList( const_cast<Dm*>(dm) );
	    if( c->isEmpty() )
		removeContainer( &(*c) );
	    }
	}
    y2mil( "device:" << device << " ret:" << (dm!=0)  );
    return( dm!=0 );
    }

bool Storage::findContainer( const string& device, ConstContIterator& c )
    {
    ContIterator tmp;
    bool ret = findContainer( device, tmp );
    if( ret )
	c = tmp;
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

bool Storage::findVolume( const string& device, ConstVolIterator& v, bool also_del )
    {
    VolIterator tmp;
    bool ret = findVolume( device, tmp, also_del );
    if( ret )
	v = tmp;
    return( ret );
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
	y2mil("label:" << label << " uuid:" << uuid);
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
	if( !p.empty() && v==p.end() && d.find("/dev/loop")==0 )
	    {
	    v = p.begin();
	    while( v!=p.end() && v->loopDevice()!=d )
		++v;
	    }
	if( !p.empty() && v==p.end() && d.find("/dev/mapper/cr_")==0 )
	    {
	    v = p.begin();
	    while( v!=p.end() && v->dmcryptDevice()!=d )
		++v;
	    }
	if( !p.empty() && v==p.end() )
	    {
	    d.replace( 0, 5, "/dev/mapper/" );
	    v = p.begin();
	    while( v!=p.end() && v->device()!=d )
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

bool Storage::findVolume( const string& device, Volume const * &vol )
    {
    bool ret = false;
    vol = NULL;
    ConstVolIterator v;
    if( findVolume( device, v ))
	{
	vol = &(*v);
	ret = true;
	}
    y2mil( "device:" << device << " ret:" << ret );
    if( vol )
	y2mil( "vol:" << *vol );
    return( ret );
    }

string Storage::findNormalDevice( const string& device )
    {
    string ret;
    ConstVolIterator v;
    if( findVolume( device, v ))
	ret = v->device();
    y2mil( "device:" << device << " ret:" << ret );
    return( ret );
    }


    Device*
    Storage::findDevice(const string& dev)
    {
	VolIterator v;
	if (findVolume(dev, v))
	    return &*v;

	DiskIterator i = findDisk(dev);
	if (i != dEnd())
	    return &*i;

	return NULL;
    }


    void
    Storage::clearUsedBy(const string& dev)
    {
	Device* tmp = findDevice(dev);
	if (tmp)
	{
	    tmp->clearUsedBy();
	    y2mil("dev:" << dev);
	}
	else
	{
	    y2mil("dev:" << dev << " failed");
	}
    }


    void
    Storage::setUsedBy(const string& dev, UsedByType type, const string& device)
    {
	Device* tmp = findDevice(dev);
	if (tmp)
	{
	    tmp->setUsedBy(type, device);
	    y2mil("dev:" << dev << " type:" << type << " device:" << device);
	}
	else
	{
	    danglingUsedBy[dev].clear();
	    danglingUsedBy[dev].push_back(UsedBy(type, device));
	    y2mil("setting type:" << type << " device:" << device <<
		  " for dev:" << dev << " to dangling usedby");
	}
    }


    void
    Storage::addUsedBy(const string& dev, UsedByType type, const string& device)
    {
	Device* tmp = findDevice(dev);
	if (tmp)
	{
	    tmp->addUsedBy(type, device);
	    y2mil("dev:" << dev << " type:" << type << " device:" << device);
	}
	else
	{
	    danglingUsedBy[dev].push_back(UsedBy(type, device));
	    y2mil("adding type:" << type << " device:" << device <<
		  " for dev:" << dev << " to dangling usedby");
	}
    }


    void
    Storage::removeUsedBy(const string& dev, UsedByType type, const string& device)
    {
	Device* tmp = findDevice(dev);
	if (tmp)
	{
	    tmp->removeUsedBy(type, device); 
	    y2mil("dev:" << dev << " type:" << type << " device:" << device);
	}
	else
	{
	    y2mil("dev:" << dev << " type:" << type << " device:" << device << " failed");
	}
    }


    bool
    Storage::isUsedBy(const string& dev)
    {
	bool ret = false;

	Device* tmp = findDevice(dev);
	if (tmp)
	{
	    ret = tmp->isUsedBy();
	}

	y2mil("dev:" << dev << " ret:" << ret);
	return ret;
    }


    bool
    Storage::isUsedBy(const string& dev, UsedByType type)
    {
	bool ret = false;

	Device* tmp = findDevice(dev);
	if (tmp)
	{
	    ret = tmp->isUsedBy(type);
	}

	y2mil("dev:" << dev << " type:" << type << " ret:" << ret);
	return ret;
    }


    void
    Storage::fetchDanglingUsedBy(const string& dev, list<UsedBy>& uby)
    {
	map<string, list<UsedBy>>::iterator pos = danglingUsedBy.find(dev);
	if (pos != danglingUsedBy.end())
	{
	    uby.splice(uby.end(), pos->second);
	    danglingUsedBy.erase(pos);
	    y2mil("dev:" << dev << " usedby:" << uby);
	}
	else
	{
	    y2mil("dev:" << dev << " not found");
	}
    }


void Storage::progressBarCb(const string& id, unsigned cur, unsigned max) const
    {
    y2mil("PROGRESS BAR id:" << id << " cur:" << cur << " max:" << max);
    CallbackProgressBar cb = getCallbackProgressBarTheOne();
    if( cb )
	(*cb)( id, cur, max );
    }

void Storage::showInfoCb(const Text& info)
    {
    y2mil("INSTALL INFO info:" << info.native);
    CallbackShowInstallInfo cb = getCallbackShowInstallInfoTheOne();
    lastAction = info;
    if( cb )
	(*cb)( info.text );
    }

void Storage::infoPopupCb(const Text& info) const
    {
    y2mil("INFO POPUP info:" << info.native);
    CallbackInfoPopup cb = getCallbackInfoPopupTheOne();
    if( cb )
	(*cb)( info.text );
    }

void Storage::addInfoPopupText(const string& disk, const Text& txt)
    {
    y2mil( "d:" << disk << " txt:" << txt.native );
    infoPopupTxts.push_back( make_pair(disk,txt) );
    }

bool Storage::yesnoPopupCb(const Text& info) const
    {
    y2mil("YESNO POPUP info:" << info.native);
    CallbackYesNoPopup cb = getCallbackYesNoPopupTheOne();
    if( cb )
	return (*cb)( info.text );
    else
	return true;
    }


    bool
    Storage::commitErrorPopupCb(int error, const Text& last_action, const string& extended_message) const
    {
	y2mil("COMMIT ERROR POPUP error:" << error << " last_action:" << last_action.native <<
	      " extended_message:" << extended_message);
	CallbackCommitErrorPopup cb = getCallbackCommitErrorPopupTheOne();
	if (cb)
	    return (*cb)(error, last_action.text, extended_message);
	else
	    return false;
    }


    bool
    Storage::passwordPopupCb(const string& device, int attempts, string& password) const
    {
	y2mil("PASSWORD POPUP device:" << device << " attempts:" << attempts);
	CallbackPasswordPopup cb = getCallbackPasswordPopupTheOne();
	if (cb)
	    return (*cb)(device, attempts, password);
	else
	    return false;
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

Storage::DiskIterator Storage::findDiskId( const string& id )
    {
    assertInit();
    string val = id;
    if( val.find( '/' )!=string::npos )
	val.erase( 0, val.find_last_of('/')+1 );
    DiskPair p = dPair();
    DiskIterator ret=p.begin();
    bool found = false;
    while( ret != p.end() && !found )
	{
	const std::list<string>& ul( ret->udevId() );
	found = find(ul.begin(),ul.end(),val) != ul.end();
	if( !found )
	    ++ret;
	}
    y2mil( "ret:" << (ret!=p.end()?ret->device():"NULL") );
    return( ret );
    }

Storage::DiskIterator Storage::findDiskPath( const string& path )
    {
    assertInit();
    string val = path;
    if( val.find( '/' )!=string::npos )
	val.erase( 0, val.find_last_of('/')+1 );
    DiskPair p = dPair();
    DiskIterator ret=p.begin();
    while( ret != p.end() && ret->udevPath()!=val )
	++ret;
    y2mil( "ret:" << (ret!=p.end()?ret->device():"NULL") );
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

Storage::DmraidCoIterator Storage::findDmraidCo( const string& name )
    {
    assertInit();
    DmraidCoPair p = dmrCoPair();
    DmraidCoIterator ret=p.begin();
    string tname = DmPartCo::undevName(name);
    while( ret!=p.end() && (ret->deleted() || ret->name()!=tname))
	++ret;
    return( ret );
    }

Storage::DmmultipathCoIterator Storage::findDmmultipathCo( const string& name )
    {
    assertInit();
    DmmultipathCoPair p = dmmCoPair();
    DmmultipathCoIterator ret=p.begin();
    string tname = DmPartCo::undevName(name);
    while( ret!=p.end() && (ret->deleted() || ret->name()!=tname))
	++ret;
    return( ret );
    }

Storage::DmPartCoIterator Storage::findDmPartCo( const string& name )
    {
    assertInit();
    DmPartCoPair p = dmpCoPair();
    DmPartCoIterator ret=p.begin();
    string tname = DmPartCo::undevName(name);
    while( ret!=p.end() && (ret->deleted() || ret->name()!=tname))
	++ret;
    return( ret );
    }

Storage::MdPartCoIterator Storage::findMdPartCo( const string& name )
{
  assertInit();
  MdPartCoPair p = mdpCoPair();
  MdPartCoIterator ret=p.begin();
  string tname = MdPartCo::undevName(name);
  while( ret!=p.end() && (ret->deleted() || ret->name()!=tname))
    ++ret;
  return( ret );
}

bool Storage::knownDevice( const string& dev, bool disks_allowed )
    {
    bool ret=true;
    ConstVolIterator v;
    if( !findVolume( dev, v ) )
	{
	ret = disks_allowed && findDisk( dev )!=dEnd();
	}
    y2mil("dev:" << dev << " ret:" << ret);
    return( ret );
    }

bool Storage::setDmcryptData( const string& dev, const string& dm, 
                              unsigned dmnum, unsigned long long siz,
			      storage::EncryptType typ )
    {
    y2mil("dev:" << dev << " dm:" << dm << " dmnum:" << dmnum << " sizeK:" << siz);
    bool ret=false;
    VolIterator v;
    if( dm.find("/temporary-cryptsetup-")==string::npos && 
        findVolume( dev, v ) )
	{
	v->setDmcryptDevEnc( dm, typ, siz!=0 );
	v->replaceAltName( "/dev/dm-", Dm::dmDeviceName(dmnum) );
	v->setSize( siz );
	ret = true;
	}
    y2mil( "ret:" << ret );
    return( ret );
    }


bool
Storage::deletedDevice(const string& dev) const
{
    ConstVolPair p = volPair(Volume::isDeleted);
    ConstVolIterator v = p.begin();
    while( v!=p.end() && v->device()!=dev )
	++v;
    bool ret = v!=p.end();
    y2mil("dev:" << dev << " ret:" << ret);
    return ret;
}


bool Storage::isDisk( const string& dev )
    {
    return( findDisk( dev ) != dEnd() );
    }


const Volume*
Storage::getVolume( const string& dev )
    {
    const Volume* ret=NULL;
    ConstVolIterator v;
    if( findVolume( dev, v ) )
	{
	ret = &(*v);
	}
    y2mil("dev:" << dev << " ret:" << (ret?ret->device():"NULL"));
    return( ret );
    }

bool Storage::canUseDevice( const string& dev, bool disks_allowed )
    {
    bool ret=true;
    ConstVolIterator v;
    if( !findVolume( dev, v ) )
	{
	if( disks_allowed )
	    {
	    DiskIterator i = findDisk( dev );
	    ret = i != dEnd() && !i->isUsedBy() && i->numPartitions() == 0;
	    }
	else
	    ret = false;
	}
    else
	{
	ret = v->canUseDevice();
	}
    y2mil("dev:" << dev << " ret:" << ret);
    return( ret );
    }


string
Storage::deviceByNumber(const string& majmin) const
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
	if( v!=p.end() )
	    ret = v->device();
	if( ret.empty() )
	    {
	    ConstDiskPair d = diskPair();
	    ConstDiskIterator di = d.begin();
	    while( di!=d.end() && (ma!=di->majorNr() || mi!=di->minorNr()))
		++di;
	    if( di!=d.end() )
		ret = di->device();
	    }
	if( ret.empty() && ma==Dm::dmMajor())
	    {
	    ConstDmraidCoPair d = dmraidCoPair();
	    ConstDmraidCoIterator di = d.begin();
	    while( di!=d.end() && mi!=di->minorNr() )
		++di;
	    if( di!=d.end() )
		ret = di->device();
	    }
	}
    y2mil("majmin:" << majmin << " ret:" << ret);
    return ret;
}


unsigned long long Storage::deviceSize( const string& dev )
    {
    unsigned long long ret=0;
    ConstVolIterator v;
    if( !findVolume( dev, v ) )
	{
	DiskIterator i = findDisk( dev );
	if( i!=dEnd() )
	    ret = i->sizeK();
	}
    else
	ret = v->sizeK();
    y2mil("dev:" << dev << " ret:" << ret);
    return( ret );
    }


    void
    Storage::addToList(Container* e)
    {
	pointerIntoSortedList<Container>(cont, e);
    }


int Storage::removeContainer( Container* val )
    {
    y2mil("name:" << val->name());
    int ret = 0;
    CIter i=cont.begin();
    while( i!=cont.end() && *i!=val )
	++i;
    if( i!=cont.end() )
	{
	delete *i;
	cont.erase( i );
	}
    else
	{
	ret = STORAGE_CONTAINER_NOT_FOUND;
	}
    y2mil("ret:" << ret);
    return( ret );
    }


    int
    Storage::removeUsing(const string& device, const list<UsedBy>& usedby)
    {
	y2mil("device:" << device << " usedby:" << usedby);

	int ret = 0;

	// iterators of usedby are invalidated during remove
	const list<UsedBy> tmp(usedby);

	for (list<UsedBy>::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
	{
	    switch (it->type())
	    {
		case UB_MD:
		    ret = removeVolume(it->device());
		    break;
		case UB_DM:
		    ret = removeVolume(it->device());
		    break;
		case UB_LVM:
		    ret = removeLvmVg(it->device().substr(5));
		    break;
		case UB_DMRAID:
		    break;
		case UB_DMMULTIPATH:
		    break;
		case UB_MDPART:
		    ret = removeMdPartCo(it->device(), true);
		    break;
		default:
		    ret = STORAGE_REMOVE_USING_UNKNOWN_TYPE;
		    break;
	    }

	    if (ret != 0)
		break;
	}

	y2mil("ret:" << ret);
	return ret;
    }


void Storage::rootMounted()
    {
    root_mounted = true;
    if( !root().empty() )
	{
	string d = root() + "/etc";
	if (!checkDir(d))
	    createPath(d);

	bool have_mds = false;

	MdCo* md;
	if (haveMd(md))
	    have_mds = true;

	MdPartCoPair p = mdpCoPair();
	if (!p.empty())
	    have_mds = true;

	if (have_mds)
	{
	    delete raidtab;
	    raidtab = new EtcRaidtab(root());

	    if (haveMd(md))
		md->syncRaidtab();

	    for (MdPartCoIterator it = p.begin(); it != p.end(); ++it)
		it->syncRaidtab();
	}

	if( instsys() )
	    {
	    string path = root()+"/etc/fstab";
	    unlink( path.c_str() );
	    }
	int ret = fstab->changeRootPrefix( root()+"/etc" );
	if( ret!=0 )
	    y2err("changeRootPrefix returns " << ret);
	}
    }

bool
Storage::checkDeviceMounted(const string& device, list<string>& mps)
    {
    bool ret = false;
    assertInit();
    y2mil("device:" << device);
    ConstVolIterator vol;
	ProcMounts mounts;
    if( findVolume( device, vol ) )
	{
	    mps = mounts.getAllMounts(vol->mountDevice());
	    mps.splice(mps.end(), mounts.getAllMounts(vol->altNames()));
	}
    else
	{
	    mps = mounts.getAllMounts(device);
	}
    ret = !mps.empty();
    y2mil("ret:" << ret << " mps:" << mps);
    return ret;
    }


bool
Storage::umountDevice( const string& device )
    {
    bool ret = false;
    assertInit();
    y2mil("device:" << device);
    VolIterator vol;
    if( !readonly() && findVolume( device, vol ) )
	{
	if( vol->umount()==0 )
	    {
	    vol->crUnsetup();
	    ret = true;
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

bool
Storage::mountDev( const string& device, const string& mp, bool ro,
                   const string& opts )
    {
    bool ret = true;
    assertInit();
    y2mil("device:" << device << " mp:" << mp << " ro:" << ro << " opts:" << opts);
    VolIterator vol;
    if( !readonly() && findVolume( device, vol ) )
	{
	if( vol->needCrsetup() )
	    {
	    ret = vol->doCrsetup()==0;
	    }
	if( ret )
	    {
	    string save = vol->getFstabOption();
	    vol->setFstabOption( opts );
	    ret = vol->mount( mp, ro )==0;
	    vol->setFstabOption( save );
	    }
	if( !ret )
	    vol->crUnsetup();
	}
    else
	ret = false;
    y2mil("ret:" << ret);
    return( ret );
    }


bool
Storage::readFstab( const string& dir, deque<VolumeInfo>& infos )
{
    static deque<VolumeInfo> s_infos; // workaround for broken ycp bindings
    static Regex disk_part( "^/dev/[sh]d[a-z]+[0-9]+$" );
    s_infos.clear();
    bool ret = false;
    ConstVolIterator vol;
    assertInit();
    y2mil("dir:" << dir);
    EtcFstab fstab(dir, true);
    const list<FstabEntry> le = fstab.getEntries();
    for( list<FstabEntry>::const_iterator i=le.begin(); i!=le.end(); ++i )
    {
	y2mil( "entry:" << *i );
	if( disk_part.match( i->dentry ) )
	{
	    VolumeInfo info;
	    info.create = info.format = info.resize = false;
	    info.sizeK = info.origSizeK = 0;
	    info.minor = info.major = 0;
	    info.device = i->dentry;
	    info.mount = i->mount;
	    info.mount_by = MOUNTBY_DEVICE;
	    info.fs = Volume::toFsType( i->fs );
	    info.fstab_options = boost::join( i->opts, "," );
	    s_infos.push_back(info);
	}
	else if( findVolume( i->dentry, vol ) )
	{
	    VolumeInfo info;
	    vol->getInfo( info );
	    vol->mergeFstabInfo( info, *i );
	    y2mil( "volume:" << *vol );
	    s_infos.push_back(info);
	}
    }
    infos = s_infos;
    ret = !infos.empty();
    y2mil("ret:" << ret);
    return ret;
}


bool
Storage::getFreeInfo(const string& device, bool get_resize, ResizeInfo& resize_info,
		     bool get_content, ContentInfo& content_info, bool use_cache)
    {
    bool ret = false;
    assertInit();

    resize_info = ResizeInfo();
    content_info = ContentInfo();

    if (testmode())
	use_cache = true;

    y2mil("device:" << device << " use_cache:" << use_cache);

    VolIterator vol;
    if( findVolume( device, vol ) )
	{
	if (vol->getFs() == FSUNKNOWN || vol->getFs() == FSNONE || vol->getFs() == SWAP)
	{
	    ret = false;
	}
	else if (vol->isUsedBy())
	{
	    ret = false;
	}
	else if (use_cache && getCachedFreeInfo(vol->device(), get_resize, resize_info,
						get_content, content_info))
	{
	    ret = true;
	}
	else if (testmode())
	{
	    ret = false;
	}
	else
	    {
	    bool needUmount = false;
	    string mp;
	    if( !vol->isMounted() )
		{
		removeDmTableTo( *vol );
		string mdir = tmpDir() + "/tmp-free-mp";
		unlink( mdir.c_str() );
		rmdir( mdir.c_str() );
		string opts = vol->getFstabOption();
		if( vol->getFs()==NTFS )
		    {
		    if( !opts.empty() )
			opts += ",";
		    opts += "show_sys_files";
		    }
		if( mkdir( mdir.c_str(), 0700 )==0 &&
		    mountDev( device, mdir, true, opts ) )
		    {
		    needUmount = true;
		    mp = mdir;
		    }
		}
	    else
		mp = vol->getMount();

	    if( !mp.empty() )
		{
		ret = true;

		bool resize_cached = false;
		bool content_cached = false;

		if (get_resize || vol->getFs() != NTFS)
		{
		    resize_cached = true;
		    resize_info = FreeInfo::detectResizeInfo(mp, *vol);
		}

		if (get_content || true)
		{
		    content_cached = true;
		    content_info = FreeInfo::detectContentInfo(mp, *vol);
		}

		setCachedFreeInfo(vol->device(), resize_cached, resize_info, content_cached,
				  content_info);
		}

	    if( needUmount )
		{
		umountDevice( device );
		rmdir( mp.c_str() );
		}

	    if( vol->needCrsetup() && vol->doCrsetup() )
		{
		ret = vol->mount( mp )==0;
		if( !ret )
		    vol->crUnsetup();
		}
	    }
	}

    y2mil("device:" << device << " ret:" << ret);
    if (ret && get_resize)
	y2mil("resize_info " << resize_info);
    if (ret && get_content)
	y2mil("content_info " << content_info);

    return ret;
    }


void
Storage::setCachedFreeInfo(const string& device, bool resize_cached, const ResizeInfo& resize_info,
			   bool content_cached, const ContentInfo& content_info)
{
    map<string, FreeInfo>::iterator it = free_infos.find(device);
    if (it != free_infos.end())
	it->second.update(resize_cached, resize_info, content_cached, content_info);
    else
	free_infos.insert(it, make_pair(device, FreeInfo(resize_cached, resize_info,
							 content_cached, content_info)));
}


bool
Storage::getCachedFreeInfo(const string& device, bool get_resize, ResizeInfo& resize_info,
			   bool get_content, ContentInfo& content_info) const
{
    bool ret = false;

    map<string, FreeInfo>::const_iterator it = free_infos.find(device);
    if (it != free_infos.end())
    {
	ret = true;

	if (get_resize)
	{
	    if (it->second.resize_cached)
		resize_info = it->second.resize_info;
	    else
		ret = false;
	}

	if (get_content)
	{
	    if (it->second.content_cached)
		content_info = it->second.content_info;
	    else
		ret = false;
	}
    }

    y2mil("device:" << device << " ret:" << ret);
    return ret;
}


void
Storage::eraseCachedFreeInfo(const string& device)
{
    free_infos.erase(device);
}


void Storage::checkPwdBuf( const string& device )
    {
    if( !pwdBuf.empty() )
	{
	map<string,string>::iterator i=pwdBuf.find(device);
	if( i!=pwdBuf.end() )
	    {
	    VolIterator vol;
	    if( findVolume( device, vol ) )
		vol->setCryptPwd( i->second );
	    pwdBuf.erase(i);
	    }
	}
    }


    void
    Storage::logFreeInfo(const string& Dir) const
    {
	string fname(Dir + "/free.info.tmp");
	ofstream file(fname.c_str());
	classic(file);

	for (map<string, FreeInfo>::const_iterator it = free_infos.begin(); it != free_infos.end(); ++it)
	{
	    file << it->first;

	    file << " resize_cached=" << it->second.resize_cached;
	    if (it->second.resize_cached)
	    {
		const ResizeInfo& resize_info = it->second.resize_info;
		file << " df_free=" << resize_info.df_freeK << " resize_free=" << resize_info.resize_freeK
		     << " used=" << resize_info.usedK << " resize_ok=" << resize_info.resize_ok;
	    }

	    file << " content_cached=" << it->second.content_cached;
	    if (it->second.content_cached)
	    {
		const ContentInfo& content_info = it->second.content_info;
		file << " windows=" << content_info.windows << " efi=" << content_info.efi << " home="
		     << content_info.home;
	    }

	    file << endl;
	}

	file.close();
	handleLogFile(fname);
    }


    void
    Storage::readFreeInfo(const string& file)
    {
	AsciiFile f(file);
	const vector<string>& lines = f.lines();
	for (vector<string>::const_iterator line = lines.begin(); line != lines.end(); ++line)
	{
	    list<string> l = splitString(*line);

	    if (l.size() >= 2)
	    {
		string device = l.front();
		l.erase(l.begin());

		const map<string, string> m = makeMap(l);
		map<string, string>::const_iterator i;

		bool resize_cached = false;
		ResizeInfo resize_info;

		bool content_cached = false;
		ContentInfo content_info;

		i = m.find("resize_cached");
		if (i != m.end())
		    i->second >> resize_cached;
		i = m.find("df_free");
		if (i != m.end())
		    i->second >> resize_info.df_freeK;
		i = m.find("resize_free");
		if (i != m.end())
		    i->second >> resize_info.resize_freeK;
		i = m.find("used");
		if (i != m.end())
		    i->second >> resize_info.usedK;
		i = m.find("resize_ok");
		if (i != m.end())
		    i->second >> resize_info.resize_ok;

		i = m.find("content_cached");
		if (i != m.end())
		    i->second >> content_cached;
		i = m.find("windows");
		if (i != m.end())
		    i->second >> content_info.windows;
		i = m.find("efi");
		if (i != m.end())
		    i->second >> content_info.efi;
		i = m.find("home");
		if (i != m.end())
		    i->second >> content_info.home;

		mapInsertOrReplace(free_infos, device, FreeInfo(resize_cached, resize_info,
								content_cached, content_info));
	    }
	}
    }


    void
    Storage::logArchInfo(const string& Dir) const
    {
	string fname(Dir + "/arch.info.tmp");
	ofstream file(fname.c_str());
	classic(file);

	file << "Arch: " << arch() << endl;

	file.close();
	handleLogFile(fname);
    }


    void
    Storage::readArchInfo(const string& file)
    {
	AsciiFile f(file);
	const vector<string>& lines = f.lines();
	vector<string>::const_iterator it;

	if ((it = find_if(lines, string_starts_with("Arch:"))) != lines.end())
	    proc_arch = extractNthWord(1, *it);
    }


int
Storage::createBackupState( const string& name )
    {
    int ret = readonly()?STORAGE_CHANGE_READONLY:0;
    assertInit();
    y2mil("name:" << name);
    if (ret == 0 && name.empty())
	ret = STORAGE_INVALID_BACKUP_STATE_NAME;
    if( ret==0 )
	{
	if (checkBackupState(name))
	    removeBackupState(name);
	CCont tmp;
	for (CCIter i = cont.begin(); i != cont.end(); ++i)
	    tmp.push_back((*i)->getCopy());
	backups.insert(make_pair(name, tmp));
	}
    y2mil( "states:" << backupStates() );
    y2mil("ret:" << ret);
    if( ret==0 )
	y2mil("comp:" << equalBackupStates(name, "", true));
    return( ret );
    }

int
Storage::removeBackupState( const string& name )
    {
    int ret = readonly()?STORAGE_CHANGE_READONLY:0;
    assertInit();
    y2mil("name:" << name);
    if( ret==0 )
	{
	if( !name.empty() )
	    {
	    map<string,CCont>::iterator i = backups.find( name );
	    if( i!=backups.end())
		{
		clearPointerList(i->second);
		backups.erase(i);
		}
	    else
		ret = STORAGE_BACKUP_STATE_NOT_FOUND;
	    }
	else
	    deleteBackups();
	}
    y2mil( "states:" << backupStates() );
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::restoreBackupState( const string& name )
    {
    int ret = readonly()?STORAGE_CHANGE_READONLY:0;
    assertInit();
    y2mil("name:" << name);
    if( ret==0 )
	{
	map<string, CCont>::const_iterator b = backups.find(name);
	if( b!=backups.end())
	    {
	    clearPointerList(cont);
	    for (CCIter i = b->second.begin(); i != b->second.end(); ++i)
		cont.push_back( (*i)->getCopy() );
	    }
	else
	    ret = STORAGE_BACKUP_STATE_NOT_FOUND;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

bool
Storage::checkBackupState( const string& name ) const
    {
    bool ret = false;
    y2mil("name:" << name);
    ret = backups.find(name) != backups.end();
    y2mil("ret:" << ret);
    return ret;
    }


bool
Storage::equalBackupStates(const string& lhs, const string& rhs,
			   bool verbose_log) const
{
    y2mil("lhs:" << lhs << " rhs:" << rhs << " verbose:" << verbose_log);
    const CCont* l = NULL;
    const CCont* r = NULL;
    if( lhs.empty() )
	l = &cont;
    else
	{
	map<string, CCont>::const_iterator i = backups.find(lhs);
	if( i!=backups.end() )
	    l = &i->second;
	}
    if( rhs.empty() )
	r = &cont;
    else
	{
	map<string, CCont>::const_iterator i = backups.find(rhs);
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
	    j = find_if(r->begin(), r->end(), bind2nd(deref_equal_to<Container>(), *i));
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
	    j = find_if(l->begin(), l->end(), bind2nd(deref_equal_to<Container>(), *i));
	    if( j==l->end() )
		{
		ret = false;
		if( verbose_log )
		    y2mil( "container <--" << (**i) );
		}
	    ++i;
	    }
	}
    y2mil("ret:" << ret);
    return ret;
}


string
Storage::backupStates() const
    {
    string ret;
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
Storage::activateHld(bool val)
{
    y2mil("val:" << val);
    if (val)
    {
        if (getImsmDriver() == IMSM_MDADM)
          {
          MdPartCo::activate(val, tmpDir());
          Dm::activate(val);
          }
        else
          {
          Dm::activate(val);
          MdPartCo::activate(val, tmpDir());
          }
    }
    LvmVg::activate(val);
    if (!val)
    {
	Dm::activate(val);
	MdPartCo::activate(val, tmpDir());
    }
}


void
Storage::activateMultipath(bool val)
{
    y2mil("val:" << val);
    DmmultipathCo::activate(val);
}


int Storage::addFstabEntry( const string& device, const string& mount,
                            const string& vfs, const string& options,
			    unsigned freq, unsigned passno )
    {
    int ret = readonly()?STORAGE_CHANGE_READONLY:0;
    assertInit();
    y2mil("device:" << device << " mount:" << mount << " vfs:" << vfs << " opts:" << options <<
	  " freq:" << freq << " passno:" << passno);
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
    y2mil("ret:" << ret);
    return( ret );
    }

void Storage::setExtError( const string& txt )
    {
    extendedError = txt;
    }


void
Storage::waitForDevice()
{ 
    string cmd(UDEVADM " settle --timeout=20");
    y2mil("calling prog:" << cmd);
    SystemCmd c(cmd);
    y2mil("returned prog:" << cmd << " retcode:" << c.retcode());
}


int
Storage::waitForDevice(const string& device)
{
    int ret = 0;
    waitForDevice();
    bool exist = access(device.c_str(), R_OK)==0;
    y2mil("device:" << device << " exist:" << exist);
    if (!exist)
    {
	for (int count = 0; count < 500; count++)
	{
	    usleep(10000);
	    exist = access(device.c_str(), R_OK) == 0;
	    if (exist)
		break;
	}
	y2mil("device:" << device << " exist:" << exist);
    }
    if (!exist)
	ret = STORAGE_DEVICE_NODE_NOT_FOUND;
    y2mil("ret:" << ret);
    return ret;
}


int
Storage::zeroDevice(const string& device, unsigned long long sizeK, bool random,
		    unsigned long long startK, unsigned long long endK)
{
    y2mil("device:" << device << " sizeK:" << sizeK << " random:" << random <<
	  " startK:" << startK << " endK:" << endK);

    int ret = 0;

    const string source = (random ? "/dev/urandom" : "/dev/zero");

    SystemCmd c;
    string cmd;

    startK = min(startK, sizeK);
    cmd = DDBIN " if=" + source + " of=" + quote(device) + " bs=1k count=" + decString(startK);
    if (c.execute(cmd) != 0)
	ret = STORAGE_ZERO_DEVICE_FAILED;

    endK = min(endK, sizeK);
    cmd = DDBIN " if=" + source + " of=" + quote(device) + " seek=" + decString(sizeK - endK) +
	" bs=1k count=" + decString(endK);
    if (c.execute(cmd) != 0)
	ret = STORAGE_ZERO_DEVICE_FAILED;

    y2mil("ret:" << ret);
    return ret;
}


std::ostream& operator<<(std::ostream& s, const Storage& v)
    {
    v.printInfo(s);
    return(s);
    }


    // workaround for broken YCP bindings
    CallbackProgressBar progress_bar_cb_ycp = NULL;
    CallbackShowInstallInfo install_info_cb_ycp = NULL;
    CallbackInfoPopup info_popup_cb_ycp = NULL;
    CallbackYesNoPopup yesno_popup_cb_ycp = NULL;
    CallbackCommitErrorPopup commit_error_popup_cb_ycp = NULL;
    CallbackPasswordPopup password_popup_cb_ycp = NULL;

}
