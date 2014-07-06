/*
 * Copyright (c) [2004-2014] Novell, Inc.
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
#include <sys/mount.h>
#include <pwd.h>
#include <signal.h>
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
#include "storage/SystemInfo/SystemInfo.h"
#include "storage/EtcFstab.h"
#include "storage/EtcMdadm.h"
#include "storage/AsciiFile.h"
#include "storage/StorageDefines.h"
#include "storage/gen_md5sum.h"


namespace storage
{
    using namespace std;

void
initDefaultLogger( const string& logdir )
    {
    string path(logdir);
    string file("libstorage");
    if (geteuid ())
        {
	struct passwd* pw = getpwuid (geteuid());
	if (pw)
            {
	    path = pw->pw_dir;
	    file = ".libstorage";
            }
        }
    createLogger(path, file);
    }

    std::ostream& operator<<(std::ostream& s, const Environment& env)
    {
	return s << "readonly:" << env.readonly << " testmode:" << env.testmode 
		 << " autodetect:" << env.autodetect << " instsys:" << env.instsys
		 << " logdir:" << env.logdir << " testdir:" << env.testdir;
    }


Storage::Storage(const Environment& env)
    : env(env), lock(readonly(), testmode()), cache(true), initialized(false),
      recursiveRemove(false), zeroNewPartitions(false),
      partAlignment(ALIGN_OPTIMAL), defaultMountBy(MOUNTBY_ID),
      defaultFs(BTRFS), defaultSubvolName(""), detectMounted(true),
      root_mounted(!instsys()), rootprefix(),
      multipath_autostart(MPAS_UNDECIDED)
{
    y2mil("constructed Storage with " << env);
    y2mil("libstorage version " VERSION);
    y2mil("libstorage source md5sum:" << GetSourceMd5() );
    y2mil("libstorage source moddate:" << GetSourceDate() );

    atexit( clean_tmpdir );

    max_log_num = 5;
    const char* tenv = getenv("Y2MAXLOGNUM");
    if (tenv)
	string(tenv) >> max_log_num;
    y2mil("max_log_num:" << max_log_num);

    progress_bar_cb = NULL;
    install_info_cb = NULL;
    info_popup_cb = NULL;
    yesno_popup_cb = NULL;
    commit_error_popup_cb = NULL;
    password_popup_cb = NULL;

    SystemCmd::setTestmode(testmode());

    tenv = getenv("LIBSTORAGE_MULTIPATH_AUTOSTART");
    if (tenv)
    {
	if (!toValue(tenv, multipath_autostart, false))
	    y2war("unknown multipath autostart '" << tenv << "' in environment");
    }
    y2mil("multipath_autostart:" << toString(multipath_autostart));

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

    tempdir = "/tmp/libstorage-XXXXXX";
    if (!mkdtemp(tempdir))
	{
	y2err("tmpdir creation " << tempdir << " failed. Aborting...");
	exit(EXIT_FAILURE);
	}
    tmp_dirs.push_back(tempdir);

    bindtextdomain("libstorage", "/usr/share/locale");
    bind_textdomain_codeset("libstorage", "UTF-8");

    if (access(SYSCONFIGFILE, R_OK) == 0)
    {
	SysconfigFile sc(SYSCONFIGFILE);

	string val;
	if (sc.getValue("DEVICE_NAMES", val))
	{
	    boost::to_lower(val, locale::classic());
	    MountByType mount_by_type;
	    if (!toValue(val, mount_by_type, false))
		y2war("unknown default mount-by method '" << val << "' in " SYSCONFIGFILE);
	    else
		setDefaultMountBy(mount_by_type);
	}

	if (sc.getValue("DEFAULT_FS", val))
	{
	    boost::to_lower(val, locale::classic());
	    if (val == "reiser") val = "reiserfs";
	    FsType fs_type;
	    if (!toValue(val, fs_type, false))
		y2war("unknown default filesystem '" << val << "' in " SYSCONFIGFILE);
	    else if (fs_type != EXT2 && fs_type != EXT3 && fs_type != EXT4 &&
		     fs_type != REISERFS && fs_type != XFS && fs_type != BTRFS)
		y2war("unallowed default filesystem '" << val << "' in " SYSCONFIGFILE);
	    else
		setDefaultFs(fs_type);
	}

	if (sc.getValue("PARTITION_ALIGN", val))
	{
	    boost::to_lower(val, locale::classic());
	    PartAlign part_align;
	    if (!toValue(val, part_align, false))
		y2war("unknown partition alignment '" << val << "' in " SYSCONFIGFILE);
	    else
		setPartitionAlignment(part_align);
	}
    }

    if (testmode())
    {
	string t = testdir() + "/arch.info";
	if (access(t.c_str(), R_OK) == 0)
	    readArchInfo(t);
    }
    else if (autodetect())
    {
	archinfo.probe();
	y2mil(archinfo);
    }

    if( !testmode() )
        checkBinPaths(archinfo, instsys());

    if (instsys())
    {
	decideMultipath();

	if (multipath_autostart == MPAS_ON)
	    DmmultipathCo::activate(true);
    }

    detectObjects();

    for (auto const &i : infoPopupTxts)
	{
	if (!isUsedBy(i.first))
	    infoPopupCb(i.second);
	else
	    y2mil("suppressing cb:" << i.second.native);
	}
    logProcData();
    }


void Storage::dumpObjectList()
{
    assertInit();
    ostringstream buf;
    prepareLogStream(buf);
    printInfo(buf);
    y2mil("DETECTED OBJECTS BEGIN");
    string s(buf.str());
    const char* pos1 = s.c_str();
    const char* pos2;
    while( pos1!=NULL && *pos1!=0 )
        {
        pos2 = strchr( pos1, '\n' );
        if( pos2 )
            {
            y2mil( string(pos1,pos2-pos1) );
            pos1=pos2+1;
            }
        else
            {
            y2mil( string(pos1) );
            pos1=pos2;
            }
        }
    y2mil("DETECTED OBJECTS END");
}


void Storage::detectObjects()
{
	if (instsys())
	{
	    MdPartCo::activate(true, tmpDir());
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
    detectMds(systeminfo);
    detectDm(systeminfo, true);
    detectLvmVgs(systeminfo);
    detectDm(systeminfo, false);

    if (testmode())
	{
 	rootprefix = testdir();
	fstab.reset(new EtcFstab(rootprefix + "/etc"));
	mdadm.reset(new EtcMdadm(this, rootprefix));
	}
    else
    {
	fstab.reset(new EtcFstab("/etc", isRootMounted()));
	if (!instsys())
	    mdadm.reset(new EtcMdadm(this, root()));
    }
    
    detectLoops(systeminfo);
    if( !instsys() )
	{
	detectNfs(*fstab, systeminfo);
	detectTmpfs(*fstab, systeminfo);
	}

    if (!danglingUsedBy.empty())
	y2err("dangling used-by left after detection: " << danglingUsedBy);
    
    LvmVgPair p = lvgPair();
    y2mil( "p length:" << p.length() );
    for (auto &i : p)
	i.normalizeDmDevices();

    if (testmode())
        {
	for (auto &i : make_range(vBegin(), vEnd()))
	    i.getFstabData(*fstab);

	string t = testdir() + "/free.info";
	if( access( t.c_str(), R_OK )==0 )
	    readFreeInfo(t);
	}
    else
	{
	detectFsData(vBegin(), vEnd(), systeminfo);
	logContainersAndVolumes(logdir());
	}
    detectBtrfs(systeminfo);

    dumpObjectList();
    }


    void
    Storage::decideMultipath()
    {
	y2mil("decideMultipath");

	if (getenv("LIBSTORAGE_NO_DMMULTIPATH") != NULL)
	    return;

	if (multipath_autostart == MPAS_UNDECIDED)
	{
	    SystemCmd c(MODPROBEBIN " dm-multipath");

	    CmdMultipath cmdmultipath(true);
	    if (cmdmultipath.looksLikeRealMultipath())
	    {
		// popup text
		Text txt = _("The system seems to have multipath hardware.\n"
			     "Do you want to activate multipath?");

		multipath_autostart = yesnoPopupCb(txt) ? MPAS_ON : MPAS_OFF;
	    }
	}
    }


void Storage::deleteBackups()
    {
    for (auto &i : backups)
        clearPointerList(i.second);
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
    else
	{
	list<string>::iterator i = find( tmp_dirs.begin(), tmp_dirs.end(),
	                                 tempdir );
	if( i!=tmp_dirs.end() )
	    tmp_dirs.erase(i);
	}

    y2mil("destructed Storage");
    }

void Storage::rescanEverything()
    {
    y2mil("rescan everything");

    if (instsys())
    {
	bool dmmultipath_active = DmmultipathCo::isActive();
	if (!dmmultipath_active)
	{
	    SystemInfo systeminfo;
	    DmmultipathCo::getMultipaths(systeminfo);
	    dmmultipath_active = DmmultipathCo::isActive();
	}

	LvmVg::activate(false);
	MdCo::activate(false, tmpDir());
	MdPartCo::activate(false, tmpDir());
	DmraidCo::activate(false);
	DmmultipathCo::activate(false);
	Dm::activate(false);

	if (dmmultipath_active)
	{
	    DmmultipathCo::activate(true);
	}
    }

    clearPointerList(cont);
    detectObjects();
    }

bool Storage::rescanCryptedObjects()
    {
    bool ret = false;
    LvmVg::activate(false);
    LvmVg::activate(true);
    SystemInfo systeminfo;
    for (auto const &i : LvmVg::getVgs(systeminfo))
	{
        LvmVgIterator vg = findLvmVg(i);
	if( vg==lvgEnd() || vg->hasUnkownPv() )
	    {
            if( vg!=lvgEnd() )
                removeContainer( &(*vg) );
	    LvmVg* v = new LvmVg(this, i, "/dev/" + i, systeminfo);
	    addToList( v );
	    v->normalizeDmDevices();
	    v->checkConsistency();
	    for (auto &ii : make_range(v->lvmLvBegin(), v->lvmLvEnd()))
		ii.updateFsData();
	    ret = true;
	    }
	}
    if( ret )
	dumpObjectList();
    y2mil( "ret:" << ret );
    return( ret );
    }


    string
    Storage::bootMount() const
    {
	if (archinfo.is_efiboot())
	    return "/boot/efi";
	else
	    return "/boot";
    }


void
    Storage::detectDisks(SystemInfo& systeminfo)
    {
    if (testmode())
	{
	    for (auto const &i : glob(testdir() + "/disk_*.info", GLOB_NOSORT))
	    {
		XmlFile file(i);
		const xmlNode* root = file.getRootElement();
		const xmlNode* disk = getChildNode(root, "disk");
		if (disk)
		    addToList(new Disk(this, disk));
	    }
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
    else if (autodetect() && getenv("LIBSTORAGE_NO_MDPARTRAID") == NULL)
    {
        auto const l = MdPartCo::filterMdPartCo(
            MdPartCo::getMdRaids(systeminfo),
            systeminfo,
            instsys());
        for (auto const &i : l)
            addToList(new MdPartCo(this, i, "/dev/" + i, systeminfo));
    }
}


void Storage::detectMds(SystemInfo& systeminfo)
    {
    if (testmode())
	{
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_MDRAID") == NULL)
	{
	MdCo* v = new MdCo(this, systeminfo);
	if( !v->isEmpty() )
	    addToList( v );
	else
	    delete v;
	}
    }

void Storage::detectBtrfs(SystemInfo& systeminfo)
    {
    if (testmode())
	{
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_BTRFS") == NULL)
	{
	BtrfsCo* v = new BtrfsCo(this, systeminfo);
	if( !v->isEmpty() )
	    {
	    for (auto const &i : v->btrfsPair())
		setBtrfsUsedBy(&i);
	    addToList( v );
	    }
	else
	    delete v;
	}
    }

void Storage::setBtrfsUsedBy( const Btrfs* bt )
    {
    const list<string>& devs = bt->getDevices(true);
    y2mil( "devs:" << devs << " to uuid:" << bt->getUuid() );
    for (auto const &d : devs)
	{
	VolIterator v;
	if (findVolume(d, v, false, true))
	    {
	    v->setUsedByUuid( UB_BTRFS, bt->getUuid() );
	    }
	}
    }

    void
    Storage::detectLoops(SystemInfo& systeminfo)
    {
    if (testmode())
	{
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_LOOP") == NULL)
	{
	    LoopCo* v = new LoopCo(this, systeminfo);
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
Storage::detectTmpfs(const EtcFstab& fstab, SystemInfo& systeminfo)
    {
    if (testmode())
	{
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_TMPFS") == NULL)
	{
	TmpfsCo* v = new TmpfsCo(this, fstab, systeminfo);
	logCo( v );
	if( !v->isEmpty() )
	    addToList( v );
	else
	    delete v;
	}
    }

void
Storage::detectLvmVgs(SystemInfo& systeminfo)
    {
    if (testmode())
	{
	    for (auto const &i : glob(testdir() + "/lvmvg_*.info", GLOB_NOSORT))
	    {
		XmlFile file(i);
		const xmlNode* root = file.getRootElement();
		const xmlNode* volume_group = getChildNode(root, "volume_group");
		if (volume_group)
		    addToList(new LvmVg(this, volume_group));
	    }
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_LVM") == NULL)
	{
	for (auto const &i : LvmVg::getVgs(systeminfo))
	    {
		LvmVg* v = new LvmVg(this, i, "/dev/" + i, systeminfo);
		addToList( v );
		v->checkConsistency();
	    }
	}
    }


void
    Storage::detectDmraid(SystemInfo& systeminfo)
{
    if (testmode())
    {
    }
    else if (autodetect() && getenv("LIBSTORAGE_NO_DMRAID") == NULL)
    {
	for (auto const &i : DmraidCo::getRaids(systeminfo))
	    {
		    DmraidCo* v = new DmraidCo(this, i, "/dev/mapper/" + i, systeminfo);
		    addToList( v );
	    }
	}
    }


void
    Storage::detectDmmultipath(SystemInfo& systeminfo)
{
    if (testmode())
    {
    }
    else if (autodetect() && getenv("LIBSTORAGE_NO_DMMULTIPATH") == NULL)
    {
	for (auto const &i : DmmultipathCo::getMultipaths(systeminfo))
	    {
		    DmmultipathCo* v = new DmmultipathCo(this, i, "/dev/mapper/" + i, systeminfo);
		    addToList( v );
	    }
	}
    }


void
    Storage::detectDm(SystemInfo& systeminfo, bool only_crypt)
    {
    if (testmode())
	{
	}
    else if (autodetect() && getenv("LIBSTORAGE_NO_DM") == NULL)
	{
	    DmCo* v = NULL;
	    if (haveDm(v))
	    {
		v->second(systeminfo, only_crypt);
	    }
	    else
	    {
		v = new DmCo(this, systeminfo, only_crypt);
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
Storage::initDisk(list<DiskData>& dl, SystemInfo& systeminfo)
    {
    y2mil( "dl: " << dl );
    for (auto &i : dl)
	{
	DiskData& data(i);
	data.dev = Disk::sysfsToDev(data.name);
	y2mil("name sysfs:" << data.name << " parted:" << data.dev);
	Disk * d = NULL;
	switch( data.typ )
	    {
	    case DiskData::DISK:
		d = new Disk(this, data.dev, "/dev/" + data.dev, data.s, systeminfo);
		break;
	    case DiskData::DASD:
		d = new Dasd(this, data.dev, "/dev/" + data.dev, data.s, systeminfo);
		break;
	    case DiskData::XEN:
		{
		string::size_type p = data.dev.find_last_not_of( "0123456789" );
		int nr = -1;
		data.dev.substr( p+1 ) >> nr;
		data.dev.erase( p+1 );
		y2mil( "data dev:" << data.dev << " nr:" << nr );
		if( nr>=0 )
		    {
		    list<DiskData>::iterator j = dl.begin();
		    while( j!=dl.end() && j->dev!=data.dev )
			++j;
		    if( j!=dl.end() && j->d )
			j->d->addPartition( (unsigned)nr, data.s, systeminfo );
		    else
			d = new Disk(this, data.dev, "/dev/" + data.dev, (unsigned) nr, data.s, systeminfo);
		    }
		break;
		}
	    }
	if( d && 
	    (d->getSysfsInfo()||data.typ==DiskData::XEN) &&
	    (data.typ == DiskData::XEN || d->detect(systeminfo)))
	    {
	    data.d = d;
	    }
	else
	    {
	    delete d;
	    }
	}
    y2mil( "dl: " << dl );
    }


    list<pair<string, Disk::SysfsInfo>>
    Storage::getDiskList(SystemInfo& systeminfo)
    {
	list<pair<string, Disk::SysfsInfo>> dlist;

	try
	{
	    for (auto const &dn : systeminfo.getDir(SYSFSDIR))
	    {
		// we do not treat mds as disks although they can be partitioned since kernel 2.6.28
		if (boost::starts_with(dn, "md") || boost::starts_with(dn, "loop"))
		    continue;

		Disk::SysfsInfo sysfsinfo;
		if (!Disk::getSysfsInfo(SYSFSDIR "/" + dn, sysfsinfo))
		    continue;

		if ((sysfsinfo.range > 1 && ( sysfsinfo.size > 0 || dn.find("dasd") == 0)) ||
		    (sysfsinfo.range == 1 && sysfsinfo.size > 0 && sysfsinfo.vbd))
		{
		    dlist.push_back(make_pair(dn, sysfsinfo));
		}
	    }
	}
	catch (const runtime_error& e)
	{
	    y2err("failed to get disks, " << e.what());
	}

	return dlist;
    }


void Storage::autodetectDisks(SystemInfo& systeminfo)
    {
    list<pair<string, Disk::SysfsInfo>> dlist = getDiskList(systeminfo);
    list< pair< string, Disk::SysfsInfo > >::const_iterator i = dlist.begin();
    list<DiskData> dl;
    while( i!=dlist.end() )
	{
	if (i->second.range > 1 && (i->second.size > 0 || boost::starts_with(i->first, "dasd")))
	    {
	    DiskData::DTyp t = boost::starts_with(i->first, "dasd") ? DiskData::DASD : DiskData::DISK;

	    if (t == DiskData::DASD)
		{
		const Dasdview& dasdview = systeminfo.getDasdview("/dev/" + i->first);
		if (dasdview.getDasdType() == Dasd::DASDTYPE_FBA)
		    t = DiskData::DISK;
		}

	    dl.push_back(DiskData(i->first, t, i->second.size / 2));
	    }
	else if( i->second.range == 1 && i->second.size > 0 && i->second.vbd )
	    {
	    dl.push_back(DiskData(i->first, DiskData::XEN, i->second.size / 2));
	    }
	++i;
	}
    initDisk(dl, systeminfo);
    const UdevMap& by_path = systeminfo.getUdevMap("/dev/disk/by-path");
    const UdevMap& by_id = systeminfo.getUdevMap("/dev/disk/by-id");
    for (auto const &i : dl)
	{
	if (i.d)
	    {
	    string tmp1;
	    UdevMap::const_iterator it1 = by_path.find(i.dev);
	    if (it1 != by_path.end())
		tmp1 = it1->second.front();

	    list<string> tmp2;
	    UdevMap::const_iterator it2 = by_id.find(i.dev);
	    if (it2 != by_id.end())
		tmp2 = it2->second;

	    i.d->setUdevData(tmp1, tmp2);
	    addToList(i.d);
	    }
	}
    }


    void
    Storage::detectFsData(const VolIterator& begin, const VolIterator& end, SystemInfo& systeminfo)
    {
    y2mil("detectFsData begin");
    SystemCmd Losetup(LOSETUPBIN " -a");
    for (auto &i : make_range(begin, end))
	{
	const LvmLv* l;
	if (!i.isUsedBy() && (i.getContainer() == NULL || !i.getContainer()->isUsedBy()))
	    {
	    i.getLoopData(Losetup);
	    i.getFsData(systeminfo.getBlkid());
	    if (i.cType() == LVM && (l = dynamic_cast<const LvmLv*>(&i)) != NULL)
		{
		if (l->isPool())
		    i.setFs(FSNONE);
		}
	    y2mil("detect:" << i);
	    }
	}
    for (auto &i : make_range(begin, end))
	{
	if (!i.isUsedBy() && (i.getContainer() == NULL || !i.getContainer()->isUsedBy()))
	    {
	    i.getMountData(systeminfo.getProcMounts(), !detectMounted);
	    i.getFstabData(*fstab);
	    y2mil("detect:" << i);
	    if (i.getFs() == FSUNKNOWN && i.getEncryption() == ENC_NONE)
		{
		Blkid::Entry e;
		if (i.findBlkid(systeminfo.getBlkid(), e) && e.is_luks)
		    i.initEncryption(ENC_LUKS);
		}
	    }
	}
    y2mil("detectFsData end");
    }

void
Storage::printInfo(ostream& str) const
{
    for (auto const &i : contPair())
    {
	i.print(str);
	str << endl;

	for (auto const &j : i.volPair())
	{
	    j.print(str);
	    str << endl;
	}
    }
}


    void
    Storage::logContainersAndVolumes(const string& Dir) const
    {
	if (max_log_num > 0 && checkDir(Dir))
	{
	    for (auto &i : cont)
		i->logData(Dir);

	    logFreeInfo(Dir);
	    logArchInfo(Dir);
	}
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
    string bname(name, 0, name.rfind('.'));
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
Storage::getRecursiveUsing(const list<string>& devices, bool itself, list<string>& using_devices)
{
    y2mil("devices:" << devices);
    assertInit();
    int ret = 0;
    using_devices.clear();

    for (auto const &it : devices)
    {
	ret = getRecursiveUsingHelper(it, itself, using_devices);
	if (ret != 0)
	    break;
    }

    y2mil("ret:" << ret << " using_devices:" << using_devices);
    return ret;
}


int
Storage::getRecursiveUsingHelper(const string& device, bool itself, list<string>& using_devices)
    {
    int ret = 0;
    ConstContIterator cont;
    ConstVolIterator vol;

    if (findVolume(device, vol))
	{
	if (itself && find(using_devices.begin(), using_devices.end(),
			   vol->device()) == using_devices.end())
	    using_devices.push_back(vol->device());

	if (vol->isUsedBy())
	    {
	    for (auto const &it : vol->getUsedBy())
		{
		addIfNotThere(using_devices, it.device());
		getRecursiveUsingHelper(it.device(), itself, using_devices);
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
		    for (auto const &i : p->disk()->partPair(Partition::notDeleted))
			if (i.type() == LOGICAL)
			    dl.push_back(i.device());
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
			for (auto const &i : d->dmpartPair(DmPart::notDeleted))
			    {
			    auto const *p = i.getPtr();
			    if( p!=NULL && p->type()==LOGICAL )
				dl.push_back(i.device());
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
			for (auto const &i : d->mdpartPair(MdPart::notDeleted))
			    {
			    p = i.getPtr();
			    if( p!=NULL && p->type()==LOGICAL )
				dl.push_back(i.device());
			    }
			}
		    }
		}
		break;
	    }
	for (auto const &i : dl)
	    {
	    addIfNotThere(using_devices, i);
	    getRecursiveUsingHelper(i, itself, using_devices);
	    }
	}
    else if (findContainer(device, cont))
	{
	if (itself && find(using_devices.begin(), using_devices.end(),
			   cont->device()) == using_devices.end())
	    using_devices.push_back(cont->device());

	for (auto const &it : cont->volPair(Volume::notDeleted))
	    {
	    addIfNotThere(using_devices, it.device());
	    getRecursiveUsingHelper(it.device(), itself, using_devices);
	    }

	if (cont->isUsedBy())
	    {
	    for (auto const &it : cont->getUsedBy())
		{
		addIfNotThere(using_devices, it.device());
		getRecursiveUsingHelper(it.device(), itself, using_devices);
		}
	    }
	}
    else
	{
	ret = STORAGE_DEVICE_NOT_FOUND;
	}
    return ret;
    }


int
Storage::getRecursiveUsedBy(const list<string>& devices, bool itself, list<string>& usedby_devices)
{
    y2mil("devices:" << devices);
    assertInit();
    int ret = 0;
    usedby_devices.clear();

    for (auto const &it : devices)
    {
	ret = getRecursiveUsedByHelper(it, itself, usedby_devices);
	if (ret != 0)
	    break;
    }

    y2mil("ret:" << ret << " usedby_devices:" << usedby_devices);
    return ret;
}


int
Storage::getRecursiveUsedByHelper(const string& device, bool itself, list<string>& usedby_devices)
{
    int ret = 0;

    const Device* p = findDevice(device, true);
    if (p)
    {
	if (itself && find(usedby_devices.begin(), usedby_devices.end(),
			   p->device()) == usedby_devices.end())
	    usedby_devices.push_back(p->device());

	for (auto const &it : p->getUsing())
	{
	    if (find(usedby_devices.begin(), usedby_devices.end(), it) == usedby_devices.end())
	    {
		usedby_devices.push_back(it);
		getRecursiveUsedByHelper(it, itself, usedby_devices);
	    }
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

void Storage::setPartitionAlignment( PartAlign val )
    {
    y2mil("val:" << toString(val));
    partAlignment = val;
    }

void Storage::setDefaultMountBy(MountByType val)
{
    y2mil("val:" << toString(val));
    defaultMountBy = val;
}


void Storage::setDefaultFs(FsType val)
{
    y2mil("val:" << toString(val));
    defaultFs = val;
}

void Storage::setDefaultSubvolName( const string& val )
{
    y2mil("old:\"" << defaultSubvolName << "\" val:\"" << val << "\"" );
    defaultSubvolName = val;
}


    bool
    Storage::getEfiBoot()
    {
	assertInit();
	return archinfo.is_efiboot();
    }


    bool
    Storage::hasIScsiDisks() const
    {
	ConstDiskPair dp = diskPair(Disk::isIScsi);
	bool ret = !dp.empty();
	y2mil("ret:" << ret);
	return ret;
    }


void Storage::setRootPrefix(const string& root)
{
    y2mil("root:" << root);
    rootprefix = root;
}

string Storage::prependRoot(const string& mp) const
{
    if (mp == "swap" || rootprefix.empty() )
	return mp;

    if (mp == "/")
	return rootprefix;
    else
	return rootprefix + mp;
}

void Storage::setDetectMountedVolumes(bool val)
{
    y2mil("val:" << val);
    detectMounted = val;
}


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
    Storage::createPartition(const string& disk, PartitionType type, const RegionInfo& cylRegion,
			     string& device)
    {
    int ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " type:" << toString(type) << " cylRegion:" << (Region) cylRegion);
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
		ret = i->createPartition(type, cylRegion.start, cylRegion.len, device, true);
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
		ret = i->createPartition(type, cylRegion.start, cylRegion.len, device, true);
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
                ret = i->createPartition(type, cylRegion.start, cylRegion.len, device, true);
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
    Storage::createPartitionKb(const string& disk, PartitionType type,
			       const RegionInfo& kRegion, string& device)
    {
    int ret = 0;
    bool done = false;
    assertInit();
    y2mil("disk:" << disk << " type:" << toString(type) << " kRegion:" << (Region) kRegion);
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
		unsigned long num_cyl = i->kbToCylinder(kRegion.len);
		unsigned long long tmp_start = kRegion.start;
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
		unsigned long num_cyl = i->kbToCylinder(kRegion.len);
		unsigned long long tmp_start = kRegion.start;
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
                unsigned long num_cyl = i->kbToCylinder(kRegion.len);
                unsigned long long tmp_start = kRegion.start;
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
    y2mil("disk:" << disk << " type:" << toString(type));
    ConstDiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	done = true;
	ret = i->nextFreePartition( type, nr, device );
	}
    if( !done )
	{
	ConstDmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    ret = i->nextFreePartition( type, nr, device );
	    }
	}
    if( !done )
        {
        ConstMdPartCoIterator i = findMdPartCo( disk );
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
    y2mil("disk:" << disk << " type:" << toString(type));
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
    ConstDiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	done = true;
	ret = i->cylinderToKb( size );
	}
    if( !done )
	{
	ConstDmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    ret = i->cylinderToKb( size );
	    }
	}
    if( !done )
    {
        ConstMdPartCoIterator i = findMdPartCo( disk );
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
    ConstDiskIterator i = findDisk( disk );
    if( i != dEnd() )
	{
	done = true;
	ret = i->kbToCylinder( sizeK );
	}
    if( !done )
	{
	ConstDmPartCoIterator i = findDmPartCo( disk );
	if( i != dmpCoEnd() )
	    {
	    done = true;
	    ret = i->kbToCylinder( sizeK );
	    }
	}
    if( !done )
    {
        ConstMdPartCoIterator i = findMdPartCo( disk );
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
    else if( findVolume( partition, cont, vol, true ) )
	{
	if( cont->type()==DISK )
	    {
	    Disk* disk = dynamic_cast<Disk *>(&(*cont));
	    if( disk!=NULL )
		{
		if( canRemove( *vol ) )
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
		if( canRemove( *vol ) )
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
	    if( canRemove( *vol ) )
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
    Storage::updatePartitionArea(const string& partition, const RegionInfo& cylRegion)
    {
	return updatePartitionArea(partition, (Region) cylRegion, false);
    }


    int
    Storage::updatePartitionArea(const string& partition, const Region& cylRegion, bool noBtrfs)
    {
    int ret = 0;
    assertInit();
    y2mil("partition:" << partition << " cylRegion:" << cylRegion << " noBtrfs:" << noBtrfs);
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol, noBtrfs ) )
	{
	if( cont->type()==DISK )
	    {
	    Disk* disk = dynamic_cast<Disk *>(&(*cont));
	    if( disk!=NULL )
		{
		ret = disk->changePartitionArea(vol->nr(), cylRegion);
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
		ret = disk->changePartitionArea(vol->nr(), cylRegion);
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
		ret = disk->changePartitionArea(vol->nr(), cylRegion);
		}
	    else
		{
		ret = STORAGE_CHANGE_AREA_INVALID_CONTAINER;
		}
	    }
	else if( cont->type()==BTRFSC )
	    {
	    BtrfsCo* bco = dynamic_cast<BtrfsCo *>(&(*cont));
	    Btrfs* b = dynamic_cast<Btrfs *>(&(*vol));
	    if( bco!=NULL && b!=NULL )
		{
		list<string> devs = b->getDevices();
		if( devs.size()==1 )
		    {
		    ret = updatePartitionArea(devs.front(), cylRegion, true);
		    if( ret==0 && findVolume( devs.front(), vol, false, true ) )
			{
			b->setSize( vol->sizeK() );
			y2mil( "vol:" << *vol );
			y2mil( "b:" << *b );
			}
		    }
		else 
		    ret = VOLUME_ALREADY_IN_USE;
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
    if( findVolume( partition, cont, vol, true ) )
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
    else if( findVolume( partition, cont, vol, true ) )
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
    return( resizePartition( partition, sizeCyl, ignoreFs, false ));
    }

int
Storage::resizePartition( const string& partition, unsigned long sizeCyl,
			  bool ignoreFs, bool noBtrfs )
    {
    int ret = 0;
    assertInit();
    y2mil("partition:" << partition << " newCyl:" << sizeCyl << " ignoreFs:" << ignoreFs <<
          "noBtrfs:" << noBtrfs );
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( partition, cont, vol, noBtrfs ) )
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
	else if( cont->type()==MDPART )
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
	else if( cont->type()==BTRFSC )
	    {
	    BtrfsCo* bco = dynamic_cast<BtrfsCo *>(&(*cont));
	    Btrfs* b = dynamic_cast<Btrfs *>(&(*vol));
	    if( bco!=NULL && b!=NULL )
		{
		list<string> devs = b->getDevices();
		if( devs.size()==1 )
		    {
		    ret = resizePartition( devs.front(), sizeCyl, ignoreFs, true );
		    if( ret==0 && findVolume( devs.front(), vol, false, true ) )
			{
			b->setSize( vol->sizeK() );
			y2mil( "vol:" << *vol );
			y2mil( "b:" << *b );
			}
		    }
		else 
		    ret = VOLUME_ALREADY_IN_USE;
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
    else if( findVolume( partition, cont, vol, true ) )
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

    ConstDiskIterator i1 = findDisk( disk );
    ConstDmPartCoIterator i2 = findDmPartCo( disk );
    ConstMdPartCoIterator i3 = findMdPartCo( disk );

    if (i1 != dEnd())
    {
	slots = i1->getUnusedPartitionSlots();
    }
    else if (i2 != dmpCoEnd())
    {
	slots = i2->getUnusedPartitionSlots();
    }
    else if (i3 != mdpCoEnd())
    {
	slots = i3->getUnusedPartitionSlots();
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
Storage::defaultDiskLabel(const string& device)
{
    assertInit();

    ConstDiskIterator i1 = findDisk(device);
    if (i1 != dEnd())
	return i1->defaultLabel();

    ConstDmPartCoIterator i2 = findDmPartCo(device);
    if (i2 != dmpCoEnd())
	return i2->disk->defaultLabel();

    ConstMdPartCoIterator i3 = findMdPartCo(device);
    if (i3 != mdpCoEnd())
	return i3->disk->defaultLabel();

    return "";			// FIXME
}


int
Storage::changeFormatVolume( const string& device, bool format, FsType fs )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " format:" << format << " fs_type:" << toString(fs));
    VolIterator vol;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, vol ) )
	{
	BtrfsCo *co = NULL;
	FsType tmpfs = format?fs:vol->detectedFs();
	y2mil("tmpfs:" << toString(tmpfs) << " fs:" << toString(fs) << " det:" <<
	      toString(vol->detectedFs()));
	y2mil( "ctype:" << vol->cType() << " BTRFS:" << BTRFSC );

	if( (tmpfs==BTRFS && vol->cType()==BTRFSC) ||
	    (tmpfs!=BTRFS && vol->cType()!=BTRFSC) )
	    ret = vol->setFormat( format, fs );
	else if( tmpfs==BTRFS )
	    {
	    // put volume to format into BTRFS-Container and set used by on plain volume
	    bool have_co = haveBtrfs(co);
	    if( !have_co )
		co = new BtrfsCo(this);
	    if( co==NULL )
		ret = STORAGE_MEMORY_EXHAUSTED;
	    else if( !have_co )
		addToList( co );
	    if( ret==0 )
		{
		ret = vol->setFormat( format, fs );
		if( ret==0 )
		    {
		    string uuid;
		    co->addFromVolume( *vol, uuid );
		    y2mil( "vol before:" << *vol );
		    vol->setUsedByUuid( UB_BTRFS, uuid );
		    y2mil( "vol after :" << *vol );
		    }
		}
	    }
	else if( tmpfs!=BTRFS )
	    {
	    // remove volume from BTRFS-Container and unset used by on plain volume
	    if(haveBtrfs(co))
		{
		string mp = vol->getMount();
		bool mounted = vol->isMounted();
                bool mby_uuid = vol->getMountBy()==MOUNTBY_UUID;
                y2mil( "mounted btrfs:" <<  mounted );
		co->eraseVolume( &(*vol) );
		if( findVolume( device, vol ) && vol->cType()!=BTRFSC )
		    {
		    vol->updateFsData();
		    vol->clearUsedBy();
		    vol->changeMount( mp );
                    if( mounted && !mp.empty() )
                        {
                        vol->setMount(mp);
			vol->setMounted();
                        }
                    if( mby_uuid )
                        vol->changeMountBy( vol->defaultMountBy() );
		    ret = vol->setFormat( format, fs );
                    y2mil( "mounted plain:" <<  vol->isMounted() );
		    }
		else
		    y2war( "base volume for " << device << " not found" );
		}
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
    y2mil("device:" << device << " mby:" << toString(mby));
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
    y2mil("ret:" << ret << " mby:" << toString(mby));
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
	list<string> opts = splitString( vol->getFstabOption(), "," );
	for (auto const &i : splitString(options, ","))
	    {
	    if (find(opts.begin(), opts.end(), i) == opts.end())
		opts.push_back(i);
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
	list<string> opts = splitString( vol->getFstabOption(), "," );
	for (auto const &i : splitString(options, ","))
	    opts.remove_if(regex_matches(i));
	ret = vol->changeFstabOptions(boost::join(opts, ","));
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
    y2mil("device:" << device << " val:" << val << " enc_type:" << toString(typ));
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol, true ) )
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
    if( findVolume( device, cont, vol, true ) )
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
Storage::verifyCryptPassword( const string& device, const string& pwd,
                              bool erase )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " l:" << pwd.length() << " erase:" << erase );
#ifdef DEBUG_CRYPT_PASSWORD
    y2mil("pwd:" << pwd);
#endif

    VolIterator vol;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, vol, false, true ) )
	{
	ret = vol->setCryptPwd( pwd );
	if( ret==0 && vol->detectEncryption()==ENC_UNKNOWN )
	    ret = VOLUME_CRYPT_NOT_DETECTED;
	if( erase || ret != 0 )
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
#ifdef DEBUG_CRYPT_PASSWORD
    y2mil("pwd:" << pwd);
#endif

    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else
	{
	LoopCo* co = new LoopCo(this);
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
#ifdef DEBUG_CRYPT_PASSWORD
    y2mil("pwd:" << pwd);
#endif

    VolIterator vol;
    map<string,string>::iterator i = pwdBuf.find(device);
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, vol, false, true ) )
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
#ifdef DEBUG_CRYPT_PASSWORD
    y2mil("pwd:" << pwd);
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
    return( resizeVolume( device, newSizeK, ignoreFs, false ));
    }

int
Storage::resizeVolume(const string& device, unsigned long long newSizeK,
		      bool ignoreFs, bool noBtrfs )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " newSizeK:" << newSizeK << 
          " ignoreFs:" << ignoreFs << " noBtrfs:" << noBtrfs );
    VolIterator vol;
    ContIterator cont;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( findVolume( device, cont, vol, noBtrfs ) )
	{
	y2mil( "vol:" << *vol );
	if( ignoreFs )
	    vol->setIgnoreFs();
	if( cont->type()!=BTRFSC )
	    ret = cont->resizeVolume(&(*vol), newSizeK);
	else
	    {
	    VolIterator r_vol;
	    ContIterator r_cont;
	    BtrfsCo* co=NULL;
	    if( findVolume( device, r_cont, r_vol, true ) )
		{
		if( haveBtrfs(co) )
		    {
		    ret = co->resizeVolume( &(*vol), &(*r_cont), &(*r_vol),
		                            newSizeK );
		    y2mil( "vol:" << *vol );
		    y2mil( "rvo:" << *r_vol );
		    }
		else
		    ret = STORAGE_BTRFS_CO_NOT_FOUND;
		}
	    else
		ret = STORAGE_VOLUME_NOT_FOUND;
	    }
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
    else if( findVolume( device, cont, vol, true ) )
	{
	if( canRemove( *vol ) )
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
	LvmVg* v = new LvmVg(this, name, "/dev/" + name, lvm1);
	ret = v->setPeSize( peSizeK );
	if( ret==0 && !devs.empty() )
	    {
	    list<string> d(devs.begin(), devs.end());
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
	list<string> d(devs.begin(), devs.end());
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
	list<string> d(devs.begin(), devs.end());
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
Storage::createLvmLvPool( const string& vg, const string& name,
                          unsigned long long sizeK, string& device )
    {
    int ret = 0;
    assertInit();
    y2mil("vg:" << vg << " name:" << name << " sizeK:" << sizeK);
    LvmVgIterator i = findLvmVg( vg );
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	ret = i->createPool(name, sizeK, device);
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
    y2mil("ret:" << ret);
    return ret;
    }


int 
Storage::createLvmLvThin( const string& vg, const string& name,
                          const string& pool, unsigned long long sizeK,
                          string& device )
    {
    int ret = 0;
    assertInit();
    y2mil("vg:" << vg << " name:" << name << " pool:" << pool <<
          " sizeK:" << sizeK);
    LvmVgIterator i = findLvmVg( vg );
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	ret = i->createThin(name, pool, sizeK, device);
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
    y2mil("ret:" << ret);
    return ret;
    }


int 
Storage::changeLvChunkSize( const string& vg, const string& name,
                            unsigned long long chunkSizeK )
    {
    int ret = 0;
    assertInit();
    y2mil("vg:" << vg << " name:" << name << " chunkSize:" << chunkSizeK);
    LvmVgIterator i = findLvmVg( vg );
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( i != lvgEnd() )
	{
	ret = i->changeChunkSize( name, chunkSizeK );
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
    return ret;
    }

int
Storage::nextFreeMd(unsigned& nr, string &device)
{
    int ret = 0;
    assertInit();
    MdCo *md = NULL;

    list<unsigned> nums;

    if (haveMd(md))
	nums = md->usedNumbers();

    nums.splice(nums.end(), getMdPartMdNums());

	bool found_free = false;

	//FIXME: magic number
	for (unsigned i = 0; i < 1000; ++i)
	{
	    if (find(nums.begin(), nums.end(), i) == nums.end())
	    {
		found_free = true;
		nr = i;
		break;
	    }
	}

    if (found_free)
    {
	device = "/dev/md" + decString(nr);
	y2mil("nr:" << nr << " device:" << device);
    }
    else
	ret = MD_UNKNOWN_NUMBER;

    y2mil("ret:" << ret);
    return ret;
}


bool
Storage::checkMdNumber(unsigned num)
{
    assertInit();
    MdCo *md = NULL;

    list<unsigned> nums;

    if (haveMd(md))
	nums = md->usedNumbers();

    nums.splice(nums.end(), getMdPartMdNums());

    return find(nums.begin(), nums.end(), num) == nums.end();
}


int
Storage::createMd(const string& name, MdType rtype, const list<string>& devs,
		  const list<string>& spares)
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name << " MdType:" << toString(rtype) << " devices:" << devs << " spares:" << spares);
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    MdCo *md = NULL;
    bool have_md = true;
    if( ret==0 )
	{
	have_md = haveMd(md);
	if( !have_md )
	    md = new MdCo(this);
	}
    if( ret==0 && md!=NULL )
	{
	ret = md->createMd(name, rtype, normalizeDevices(devs), normalizeDevices(spares));
	}
    if( !have_md )
	{
	if( ret==0 )
	    addToList( md );
	else if( md!=NULL )
	    delete md;
	}
    if( ret==0 )
	checkPwdBuf( name );
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::createMdAny(MdType rtype, const list<string>& devs, const list<string>& spares,
			 string& device)
    {
    int ret = 0;
    assertInit();
    y2mil("MdType:" << toString(rtype) << " devices:" << devs << " spares:" << spares);
    if (readonly())
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
	    md = new MdCo(this);
	if( md==NULL )
	    ret = STORAGE_MEMORY_EXHAUSTED;
	if( ret == 0 )
	  {
	  ret = nextFreeMd(num, device);
	  }
	}
    if( ret==0 )
	{
	ret = md->createMd(device, rtype, normalizeDevices(devs), normalizeDevices(spares));
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
	checkPwdBuf( device );
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
    if( ret==0 )
	{
	MdCo *md = NULL;
	if( haveMd(md) )
	    ret = md->removeMd( name, destroySb );
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

int Storage::extendMd(const string& name, const list<string>& devs, const list<string>& spares)
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name << " devs:" << devs << " spares:" << spares);
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	MdCo *md = NULL;
	if( haveMd(md) )
	    ret = md->extendMd(name, normalizeDevices(devs), normalizeDevices(spares));
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

int Storage::updateMd(const string& name, const list<string>& devs, const list<string>& spares)
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name << " devs:" << devs << " spares:" << spares);
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	MdCo *md = NULL;
	if( haveMd(md) )
	    ret = md->updateMd(name, normalizeDevices(devs), normalizeDevices(spares));
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

int Storage::shrinkMd(const string& name, const list<string>& devs, const list<string>& spares)
    {
    int ret = 0;
    assertInit();
    y2mil("name:" << name << " devs:" << devs << " spares:" << spares);
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	MdCo *md = NULL;
	if( haveMd(md) )
	    ret = md->shrinkMd(name, normalizeDevices(devs), normalizeDevices(spares));
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
    y2mil("name:" << name << " rtype:" << toString(rtype));
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	MdCo *md = NULL;
	if( haveMd(md) )
	    ret = md->changeMdType( name, rtype );
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
    if( ret==0 )
	{
	MdCo *md = NULL;
	if( haveMd(md) )
	    ret = md->changeMdChunk( name, chunk );
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
    y2mil("name:" << name << " parity:" << toString(ptype));
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	MdCo *md = NULL;
	if( haveMd(md) )
	    ret = md->changeMdParity( name, ptype );
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
    MdCo *md = NULL;
    if( haveMd(md) )
	ret = md->checkMd(name);
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
    if (ret == 0)
    {
	MdCo *md = NULL;
	if (haveMd(md))
	    ret = md->getMdState(name, info);
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
	ret = i->getMdPartCoState(info);
    }
    else
    {
	ret = STORAGE_MDPART_CO_NOT_FOUND;
    }

    return ret;
}


int
Storage::computeMdSize(MdType md_type, const list<string>& devices, const list<string>& spares,
		       unsigned long long& sizeK)
{
    int ret = 0;

    unsigned long long sumK = 0;
    unsigned long long smallestK = 0;

    for (auto const &i : devices)
    {
	const Volume* v = getVolume(i);
	if (!v)
	{
	    ret = STORAGE_VOLUME_NOT_FOUND;
	    break;
	}

	sumK += v->sizeK();
	smallestK = min(smallestK, v->sizeK());
    }

    for (auto const &i : spares)
    {
	const Volume* v = getVolume(i);
	if (!v)
	{
	    ret = STORAGE_VOLUME_NOT_FOUND;
	    break;
	}

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

    y2mil("type:" << toString(md_type) << " smallest:" << smallestK << " sum:" << sumK <<
	  " size:" << sizeK);

    return ret;
}

list<int>
Storage::getMdAllowedParity( MdType md_type, unsigned devices )
    {
    list<int> ret;
    if( md_type==RAID5 || md_type==RAID6 )
	{
	ret.push_back(PAR_DEFAULT);
	ret.push_back(LEFT_ASYMMETRIC);
	ret.push_back(LEFT_SYMMETRIC);
	ret.push_back(RIGHT_ASYMMETRIC);
	ret.push_back(RIGHT_SYMMETRIC);
	ret.push_back(PAR_FIRST);
	ret.push_back(PAR_LAST);
	if( md_type==RAID6 )
	    {
	    ret.push_back(LEFT_ASYMMETRIC_6);
	    ret.push_back(LEFT_SYMMETRIC_6);
	    ret.push_back(RIGHT_ASYMMETRIC_6);
	    ret.push_back(RIGHT_SYMMETRIC_6);
	    ret.push_back(PAR_FIRST_6);
	    }
	}
    else if( md_type==RAID10 )
	{
	ret.push_back(PAR_DEFAULT);
	ret.push_back(PAR_NEAR_2);
	ret.push_back(PAR_OFFSET_2);
	ret.push_back(PAR_FAR_2);
	if( devices>2 )
	    {
	    ret.push_back(PAR_NEAR_3);
	    ret.push_back(PAR_OFFSET_3);
	    ret.push_back(PAR_FAR_3);
	    }
	}

    y2mil("type:" << toString(md_type) << " ret:" << ret );
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
    CPair p = cPair(isMd);
    ContIterator i = p.begin();
    if( i != p.end() )
	md = static_cast<MdCo*>(&(*i));
    return( i != p.end() );
    }


    list<unsigned>
    Storage::getMdPartMdNums() const
    {
	list<unsigned> nums;

	for (auto const &i : mdpartCoPair(MdPartCo::notDeleted))
	    nums.push_back(i.nr());

	return nums;
    }


    bool
    Storage::haveDm(DmCo*& dm)
    {
	dm = NULL;
	CPair p = cPair(isDm);
	ContIterator i = p.begin();
	if (i != p.end())
	    dm = static_cast<DmCo*>(&(*i));
	return i != p.end();
    }


bool Storage::haveNfs( NfsCo*& co )
    {
    co = NULL;
    CPair p = cPair(isNfs);
    ContIterator i = p.begin();
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
	ret = co->addNfs(nfsDev, sizeK, opts, mp, nfs4);
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
    ret = co.addNfs(nfsDev, 0, opts, "", nfs4);
    if( instsys() )
	{
	string prog_name = RPCBINDBIN;

	//We don't have rpcbind (#423026, #427428) ...
	if ( !checkNormalFile(prog_name) )
	{
	    //... so let's try portmap instead
	    y2mil("No rpcbind found, trying portmap instead ...");
	    prog_name = PORTMAPBIN;
	}

	SystemCmd c;
	c.execute( prog_name );
	c.execute(RPCSTATDBIN);
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
#ifdef DEBUG_CRYPT_PASSWORD
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
	    loop = new LoopCo(this);
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
    CPair p = cPair(isLoop);
    ContIterator i = p.begin();
    if( i != p.end() )
	loop = static_cast<LoopCo*>(&(*i));
    return( i != p.end() );
    }

bool Storage::haveBtrfs( BtrfsCo*& co )
    {
    co = NULL;
    CPair p = cPair(isBtrfs);
    ContIterator i = p.begin();
    if( i != p.end() )
	co = static_cast<BtrfsCo*>(&(*i));
    return( i != p.end() );
    }

bool Storage::haveTmpfs( TmpfsCo*& co )
    {
    co = NULL;
    CPair p = cPair(isTmpfs);
    ContIterator i = p.begin();
    if( i != p.end() )
	co = static_cast<TmpfsCo*>(&(*i));
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

bool Storage::existSubvolume( const string& device, const string& name )
    {
    int ret = false;
    assertInit();
    y2mil("device:" << device << " name:" << name);
    BtrfsCo* co;
    if( haveBtrfs(co) )
	{
	ret = co->existSubvolume( device, name );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::createSubvolume( const string& device, const string& name )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " name:" << name);
    BtrfsCo* co;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( haveBtrfs(co) )
	{
	ret = co->createSubvolume( device, name );
	}
    else
	{
	ret = STORAGE_BTRFS_CO_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::removeSubvolume( const string& device, const string& name )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " name:" << name);
    BtrfsCo* co;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( haveBtrfs(co) )
	{
	ret = co->removeSubvolume( device, name );
	}
    else
	{
	ret = STORAGE_BTRFS_CO_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::extendBtrfsVolume( const string& device, const string& dev )
    {
    deque<string> d;
    d.push_back(dev);
    return( extendBtrfsVolume(device,d));
    }

int Storage::extendBtrfsVolume( const string& device, const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " devices:" << devs );
    BtrfsCo* co;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( devs.empty() )
	{
	ret = BTRFS_LIST_EMPTY;
	}
    else if( haveBtrfs(co) )
	{
	list<string> d(devs.begin(), devs.end());
	ret = co->extendVolume( device, d );
	}
    else
	{
	ret = STORAGE_BTRFS_CO_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::shrinkBtrfsVolume( const string& device, const string& dev )
    {
    deque<string> d;
    d.push_back(dev);
    return( shrinkBtrfsVolume(device,d));
    }

int Storage::shrinkBtrfsVolume( const string& device, const deque<string>& devs )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << "devices:" << devs );
    BtrfsCo* co;
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    else if( devs.empty() )
	{
	ret = BTRFS_LIST_EMPTY;
	}
    else if( haveBtrfs(co) )
	{
	list<string> d(devs.begin(), devs.end());
	ret = co->shrinkVolume( device, d );
	}
    else
	{
	ret = STORAGE_BTRFS_CO_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::addTmpfsMount( const string& mp, const string& opts )
    {
    int ret = 0;
    assertInit();
    y2mil("mount:" << mp << " opts:" << opts );
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    TmpfsCo *co = NULL;
    bool have = true;
    if( ret==0 )
	{
	have = haveTmpfs(co);
	if( !have )
	    co = new TmpfsCo( this );
	}
    if( ret==0 && co!=NULL )
	{
	ret = co->addTmpfs(mp, opts);
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

int Storage::removeTmpfsMount( const string& mp )
    {
    int ret = 0;
    assertInit();
    y2mil("mount:" << mp );
    if (readonly())
	{
	ret = STORAGE_CHANGE_READONLY;
	}
    TmpfsCo *co = NULL;
    if( ret==0 )
	{
	haveTmpfs(co);
	}
    if( ret==0 && co!=NULL )
	{
	ret = co->removeTmpfs(mp);
	}
    else if( ret==0 )
	{
	ret = STORAGE_TMPFS_CO_NOT_FOUND;
	}
    if( ret==0 )
	{
	ret = checkCache();
	}
    y2mil("ret:" << ret);
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
    list<commitAction> ca;
    for (auto const &it : contPair())
    {
	list<commitAction> l;
	it.getCommitActions(l);
	ca.splice(ca.end(), l);
    }
    ca.sort();
    return ca;
}


void
Storage::getCommitInfos(list<CommitInfo>& infos) const
{
    infos.clear();

        for (auto const &i : getCommitActions())
	{
	    CommitInfo info;
	    info.destructive = i.destructive;
	    info.text = i.description.text;
	    const Volume* v = i.vol();
	    if( v && !v->getDescText().empty() )
	    {
		info.text += ". ";
		info.text += v->getDescText();
	    }
	    infos.push_back(info);
	}

    y2mil("infos.size:" << infos.size());
}


void
Storage::dumpCommitInfos() const
{
    for (auto const &it : getCommitActions())
    {
	string text = it.description.native;
	if (it.destructive)
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
    for (auto const &i : co)
	todo.push_back(commitAction(stage, i->type(), i));
    for (auto const &i : vl)
	todo.push_back(commitAction(stage, i->cType(), i));
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
    for (auto &i : vPair(fstabAdded))
	i.setFstabAdded(false);
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

    typedef array<CommitStage, 5> Stages;
    const Stages stages = { { DECREASE, INCREASE, FORMAT, SUBVOL, MOUNT } };

    for (auto const &stage : stages)
    {
	list<const Container*> colist;
	list<const Volume*> vlist;

	if (ret == 0)
	{
	    for (auto &i : p)
		i.getToCommit(stage, colist, vlist);
	}

	bool new_pair = false;
	list<commitAction> todo;
	sortCommitLists(stage, colist, vlist, todo);
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
		ret = co->commitChanges(stage);
		cont_removed = cont_removed && ret==0;
		if( cont_removed )
		    {
		    removeContainer( co );
		    new_pair = true;
		    }
		}
	    else
		{
		ret = co->commitChanges(stage, const_cast<Volume*>(ac->vol()));
		}
	    if( ret!=0 )
		{
		y2mil("err at " << *ac);
		if (ignoreError(ret, ac))
		    ret = 0;
		}
	    ++ac;
	    }
	y2mil("stage:" << stage << " new_pair:" << new_pair);
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


bool Storage::removeDmMapsTo( const string& dev )
    {
    bool ret = false;
    y2mil("dev:" << dev);
    for (auto &v : vPair(isDmContainer))
	{
	Dm *dm = dynamic_cast<Dm *>(&v);
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
	    y2war("not a Dm descendant " << v.device());
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

bool Storage::usedDmName( const string& nm, const Volume* volp ) const
    {
    bool ret = false;
    y2mil("nm:" << nm);
    ConstVolPair vp = volPair();
    ConstVolIterator v=vp.begin(); 
    while( !ret && v!=vp.end() )
	{
        ret = v->dmcryptDevice()==nm || (v->cType()==DM&&v->device()==nm);
	ret = ret && &(*v)!=volp;
        if( !ret )
            ++v;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

void Storage::changeDeviceName( const string& old, const string& nw )
    {
    y2mil( "old:" << old << " new:" << nw );
    for (auto &ci : cPair())
	ci.changeDeviceName(old, nw);
    }


void
Storage::getDiskList( bool (* CheckFnc)( const Disk& ), std::list<Disk*>& dl )
    {
    dl.clear();
    for (auto &i : dPair(CheckFnc))
	{
	y2mil("disk:" << i.device());
	dl.push_back(&i);
	}
    }


void
Storage::getContainers( deque<ContainerInfo>& infos )
    {
    infos.clear();
    assertInit();
    for (auto const &i : contPair(Container::notDeleted))
	{
	y2mil("co:" << i);
	infos.push_back(ContainerInfo());
	i.getInfo(infos.back());
	}
    }

void
Storage::getVolumes( deque<VolumeInfo>& infos )
    {
    infos.clear ();
    assertInit();
    for (auto const &i : volPair(Volume::notDeleted))
	{
	infos.push_back(VolumeInfo());
	i.getInfo(infos.back());
	}
    }

int
Storage::getContVolInfo(const string& device, ContVolInfo& info)
    {
    int ret = STORAGE_VOLUME_NOT_FOUND;
    ConstContIterator c;
    ConstVolIterator v;
    info.ctype = CUNKNOWN;
    assertInit();
    if (findVolume(device, c, v))
	{
	ret = 0;
	if( c->type()==BTRFSC )
	    {
	    const Btrfs * b = dynamic_cast<const Btrfs *>(&(*v));
	    if( b!=NULL && b->getDevices(true).size()==1 )
		{
		findVolume(b->device(), c, v, true);
		}
	    }
	info.ctype = c->type();
	info.cname = c->name();
	info.cdevice = c->device();
	info.vname = v->name();
	info.vdevice = v->device();
	if( v->isNumeric() )
	    info.num = v->nr();
	}
    else if (findContainer(device, c))
    {
	ret = 0;
	info.ctype = c->type();
	info.cname = c->name();
	info.cdevice = c->device();
	info.vname = "";
	info.vdevice = "";
    }
    y2mil("device:" << device << " ret:" << ret << " cname:" << info.cname <<
	  " vname:" << info.vname);
    return ret;
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
	    for (auto const &i2 : i->partPair(Partition::notDeleted))
	    {
		plist.push_back(PartitionInfo());
		i2.getInfo(plist.back());
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
	for (auto const &i2 : i->lvmLvPair(LvmLv::notDeleted))
	    {
	    plist.push_back(LvmLvInfo());
	    i2.getInfo(plist.back());
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
    for (auto const &i : mdPair(Md::notDeleted))
        {
        plist.push_back(MdInfo());
        i.getInfo(plist.back());
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
    for (auto &i2 : it->mdpartPair(MdPart::notDeleted))
      {
      plist.push_back(MdPartInfo());
      i2.getInfo(plist.back());
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
    for (auto const &i : nfsPair(Nfs::notDeleted))
        {
        plist.push_back(NfsInfo());
        i.getInfo(plist.back());
        }
    return( ret );
    }

int Storage::getLoopInfo( deque<storage::LoopInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    for (auto const &i : loopPair(Loop::notDeleted))
        {
        plist.push_back(LoopInfo());
        i.getInfo(plist.back());
        }
    return( ret );
    }

int Storage::getDmInfo( deque<storage::DmInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    for (auto const &i : dmPair(Dm::notDeleted))
        {
        plist.push_back(DmInfo());
        i.getInfo(plist.back());
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
	for (auto const &i2 : i->dmraidPair(Dmraid::notDeleted))
	    {
	    plist.push_back(DmraidInfo());
	    i2.getInfo(plist.back());
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
        for (auto const &i2 : i->dmmultipathPair(Dmmultipath::notDeleted))
        {
            plist.push_back(DmmultipathInfo());
            i2.getInfo(plist.back());
        }
    }
    else
	ret = STORAGE_DMMULTIPATH_CO_NOT_FOUND;
    return( ret );
}

int Storage::getBtrfsInfo( deque<storage::BtrfsInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    for (auto const &i : btrfsPair(Btrfs::notDeleted))
        {
        plist.push_back(BtrfsInfo());
        i.getInfo(plist.back());
        }
    return( ret );
    }

int Storage::getTmpfsInfo( deque<storage::TmpfsInfo>& plist )
    {
    int ret = 0;
    plist.clear();
    assertInit();
    for (auto const &i : tmpfsPair(Tmpfs::notDeleted))
        {
        plist.push_back(TmpfsInfo());
        i.getInfo(plist.back());
        }
    return( ret );
    }

list<string> Storage::getAllUsedFs() const 
{
    set<FsType> fs;
    for (auto const &v : volPair(Volume::notDeleted))
    {
        FsType t = v.getFs();
        if (t != FSUNKNOWN && t != FSNONE)
            fs.insert(t);
    }
    list<string> ret;
    for (auto const &i : fs)
    {
        ret.push_back(toString(i));
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
				     true, 16, 100);

    static FsCapabilitiesX ext3Caps (true, true, true, false, true, true,
				     true, 16, 10*1024);

    static FsCapabilitiesX ext4Caps (true, true, true, false, true, true,
				     true, 16, 32*1024);

    static FsCapabilitiesX btrfsCaps (true, true, true, true, true, true,
				      true, 256, 256*1024);

    static FsCapabilitiesX xfsCaps (true, true, false, false, true, true,
				    false, 12, 40*1024);

    static FsCapabilitiesX ntfsCaps (true, false, true, false, true, true,
				     false, 32, 10*1024);

    static FsCapabilitiesX fatCaps (true, false, true, false, true, true,
				    false, 11, 64);

    static FsCapabilitiesX swapCaps (true, false, true, false, true, true,
				     false, 15, 64);

    static FsCapabilitiesX jfsCaps (false, false, false, false, true, true,
				    false, 16, 16*1024);

    static FsCapabilitiesX hfsCaps (false, false, false, false, false, false,
				    false, 0, 10*1024);

    static FsCapabilitiesX hfspCaps(false, false, false, false, false, false,
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

void Storage::removeDmTableTo( unsigned long mjr, unsigned long mnr )
    {
    y2mil( "mjr:" << mjr << " mnr:" << mnr );
    string cmd = DMSETUPBIN " table | " GREPBIN " -w ";
    cmd += decString(mjr) + ":" + decString(mnr);
    cmd += " | sed s/:.*// | uniq";
    SystemCmd c( cmd );
    unsigned line=0;
    while( line<c.numLines() )
	{
	removeDmTable( c.getLine(line) );
	line++;
	}
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
		removeDmTableTo( vol.getContainer()->majorNr(),
		                 vol.getContainer()->minorNr() );
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
	c.execute(DMSETUPBIN " table | " GREPBIN " " + quote(table));
	logProcData();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Storage::renameCryptDm( const string& device, const string& new_name )
    {
    y2mil( "device:" << device << " new_name:" << new_name );
    int ret = 0;
    VolIterator vol;
    if( findVolume( device, vol ))
	{
        y2mil( "vol:" << *vol );
        if( vol->dmcrypt() )
            {
            string dmnew =  new_name;
            string::size_type pos = dmnew.find_last_of("/");
            if( pos!=string::npos )
                dmnew.erase( 0, pos+1 );
            string dmold =  vol->dmcryptDevice();
            pos = dmold.find_last_of("/");
            if( pos!=string::npos )
                dmold.erase( 0, pos+1 );
            y2mil( "dmold:" << dmold << " dmnew:" << dmnew );
            if( dmnew!=dmold )
                {
                if( vol->dmcryptActive() )
                    {
                    SystemCmd c(DMSETUPBIN " rename " + dmold + " " + dmnew );
                    if( c.retcode()!=0 )
                        ret = STORAGE_DM_RENAME_FAILED;
                    }
                if( ret==0 )
                    vol->setDmcryptDev( "/dev/mapper/"+dmnew, vol->dmcryptActive() );
                }
            else
                y2mil( "dmnames already equal" );
            }
        else
            ret = STORAGE_VOLUME_NOT_ENCRYPTED;
	}
    else
	ret = STORAGE_VOLUME_NOT_FOUND;
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
    for (auto const &i : *c)
    {
        b.str("");
        i->print(b);
        y2mil("log vo:" << b.str());
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
                          ConstVolIterator& v, bool no_btrfs )
    {
    ContIterator ct;
    VolIterator vt;
    bool ret = findVolume( device, ct, vt, no_btrfs );
    if( ret )
	{
	c = ct;
	v = vt;
	}
    return( ret );
    }

bool Storage::findVolume( const string& device, ContIterator& c,
                          VolIterator& v, bool no_btrfs )
    {
    bool ret = false;
    if( findVolume( device, v, false, no_btrfs ))
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

int Storage::unaccessDev( const string& device )
    {
    int ret = 0;
    VolIterator v;
    if( findVolume(device, v) )
	{
	v->setSilent(true);
	ret = v->unaccessVol();
	v->setSilent(false);
	}
    else
	ret = STORAGE_VOLUME_NOT_FOUND;
    y2mil( "device:" << device << " ret:" << ret );
    return( ret );
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
    while (c != cp.end() && !c->sameDevice(device))
	++c;
    return( c!=cp.end() );
    }

bool Storage::findVolume( const string& device, ConstVolIterator& v, bool also_del, bool no_btrfsc )
    {
    VolIterator tmp;
    bool ret = findVolume( device, tmp, also_del, no_btrfsc );
    if( ret )
	v = tmp;
    return( ret );
    }

bool Storage::findVolume( const string& device, VolIterator& v, bool also_del, 
                          bool no_btrfsc )
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
    bool found = false;
    list<VPair> pl;
    if( !no_btrfsc )
	pl.push_back( vPair( also_del?NULL:Volume::notDeleted, isBtrfs ));
    pl.push_back( vPair( also_del?NULL:Volume::notDeleted, isNotBtrfs ));
    list<VPair>::iterator li = pl.begin();
    while( li != pl.end() && !found )
	{
	v = li->begin();
	if( label.empty() && uuid.empty() )
	    {
	    while( v!=li->end() && v->device()!=d )
		{
		const list<string>& al( v->altNames() );
		if( find( al.begin(), al.end(), d )!=al.end() )
		    break;
		++v;
		}
	    if( !li->empty() && v==li->end() && d.find("/dev/loop")==0 )
		{
		v = li->begin();
		while( v!=li->end() && v->loopDevice()!=d )
		    ++v;
		}
	    if( !li->empty() && v==li->end() && d.find("/dev/mapper/")==0 )
		{
		v = li->begin();
		while( v!=li->end() && v->dmcryptDevice()!=d )
		    ++v;
		}
	    if( !li->empty() && v==li->end() )
		{
		string tmp(d);
		tmp.replace( 0, 5, "/dev/mapper/" );
		v = li->begin();
		while( v!=li->end() && v->device()!=tmp )
		    ++v;
		}
	    if( v==li->end() )
		{
		unsigned long mjr, mnr;
		mjr = mnr = 0;
		const Device* dev = NULL;
		if( !testmode() && getMajorMinor( d, mjr, mnr, true ) &&
		    (dev=deviceByNumber( mjr, mnr ))!=NULL )
		    {
		    v = li->begin();
		    while( v!=li->end() && v->device()!=dev->device() )
			++v;
		    if( v!=li->end() )
			{
			y2war( "found over major/minor:" << d << " is:" << v->device() );
			y2mil( "vol:" << *v );
			}
		    }
		}
	    }
	else if( !label.empty() )
	    {
	    while( v!=li->end() && v->getLabel()!=label )
		++v;
	    }
	else if( !uuid.empty() )
	    {
	    while( v!=li->end() && v->getUuid()!=uuid )
		++v;
	    }
	found = v!=li->end();
	++li;
	}
    return( found );
    }

bool Storage::findVolume( const string& device, Volume const * &vol, 
                          bool no_btrfsc )
    {
    bool ret = false;
    vol = NULL;
    ConstVolIterator v;
    if( findVolume( device, v, false, no_btrfsc ))
	{
	vol = &(*v);
	ret = true;
	}
    y2mil( "device:" << device << " ret:" << ret );
    if( vol )
	y2mil( "vol:" << *vol );
    return( ret );
    }

bool Storage::findUuid( const string& uuid, Volume const * &vol )
    {
    bool ret = false;
    vol = NULL;
    ConstVolIterator v;
    if( findVolume( "UUID="+uuid, v, false, false ))
	{
	vol = &(*v);
	ret = true;
	}
    y2mil( "uuid:" << uuid << " ret:" << ret );
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


bool
Storage::findDevice( const string& dev, const Device* &vol, 
                     bool search_by_minor )
    {
    vol = findDevice(dev);
    unsigned long mj = 0, mi = 0;
    if( vol==NULL && search_by_minor && getMajorMinor( dev, mj, mi ) && mi!=0 )
	{
	vol = deviceByNumber( mj, mi );
	}
    return( vol!=NULL );
    }

    Device*
    Storage::findDevice(const string& dev, bool no_btrfsc)
    {
	VolIterator v;
	if (findVolume(dev, v, false, no_btrfsc))
	    return &*v;

	ContIterator c;
	if (findContainer(dev, c))
	    return &*c;

	return NULL;
    }


    void
    Storage::clearUsedBy(const string& dev)
    {
	Device* tmp = findDevice(dev,true);
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
    Storage::clearUsedBy(const list<string>& devs)
    {
        for (auto const &it : devs)
            clearUsedBy(it);
    }


    void
    Storage::setUsedBy(const string& dev, UsedByType type, const string& device)
    {
	Device* tmp = findDevice(dev,true);
	if (tmp)
	{
	    tmp->setUsedBy(type, device);
	    y2mil("dev:" << dev << " type:" << toString(type) << " device:" << device);
	}
	else
	{
	    danglingUsedBy[dev].clear();
	    danglingUsedBy[dev].push_back(UsedBy(type, device));
	    y2mil("setting type:" << toString(type) << " device:" << device <<
		  " for dev:" << dev << " to dangling usedby");
	}
    }

void
Storage::setUsedByBtrfs(const string& dev, const string& uuid)
    {
    y2mil( "dev:" << dev << " uuid:" << uuid );
    Device* tmp = findDevice(dev,true);
    if (tmp)
	{
	tmp->setUsedBy(UB_BTRFS, uuid);
	}
    }

bool 
Storage::canRemove( const Volume& vol ) const
    {
    return( recursiveRemove || !vol.isUsedBy() || 
            isUsedBySingleBtrfs( vol ) );
    }

bool 
Storage::isUsedBySingleBtrfs( const Volume& vol ) const
    {
    return( isUsedBySingleBtrfs( vol, NULL ));
    }

bool 
Storage::isUsedBySingleBtrfs( const Volume& vol, const Volume** btrfs ) const
    {
    const list<UsedBy>& ub = vol.getUsedBy();
    bool ret = ub.size()==1 && ub.front().type()==UB_BTRFS;
    if( ret )
	{
	ConstBtrfsPair p = btrfsPair(Btrfs::notDeleted);
	ConstBtrfsIterator i = p.begin();
	while( i!=p.end() && i->getUuid()!=ub.front().device() )
	    ++i;
	ret = i!=p.end() && i->getDevices(true).size()<=1;
	if( btrfs!=NULL )
	    *btrfs = &(*i);
	}
    y2mil( "dev:" << vol.device() << " ret:" << ret );
    return( ret );
    }


    void
    Storage::setUsedBy(const list<string>& devs, UsedByType type, const string& device)
    {
        for (auto const &it : devs)
            setUsedBy(it, type, device);
    }


    void
    Storage::addUsedBy(const string& dev, UsedByType type, const string& device)
    {
	Device* tmp = findDevice(dev);
	if (tmp)
	{
	    tmp->addUsedBy(type, device);
	    y2mil("dev:" << dev << " type:" << toString(type) << " device:" << device);
	}
	else
	{
	    danglingUsedBy[dev].push_back(UsedBy(type, device));
	    y2mil("adding type:" << toString(type) << " device:" << device <<
		  " for dev:" << dev << " to dangling usedby");
	}
    }


    void
    Storage::addUsedBy(const list<string>& devs, UsedByType type, const string& device)
    {
        for (auto const &it : devs)
            addUsedBy(it, type, device);
    }


    void
    Storage::removeUsedBy(const string& dev, UsedByType type, const string& device)
    {
	Device* tmp = findDevice(dev);
	if (tmp)
	{
	    tmp->removeUsedBy(type, device); 
	    y2mil("dev:" << dev << " type:" << toString(type) << " device:" << device);
	}
	else
	{
	    y2mil("dev:" << dev << " type:" << toString(type) << " device:" << device <<
		  " failed");
	}
    }


    void
    Storage::removeUsedBy(const list<string>& devs, UsedByType type, const string& device)
    {
        for (auto const &it : devs)
            removeUsedBy(it, type, device);
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

	y2mil("dev:" << dev << " type:" << toString(type) << " ret:" << ret);
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

void Storage::showInfoCb(const Text& info, bool quiet )
    {
    y2mil("INSTALL INFO info:" << info.native);
    y2mil("quiet:" << quiet);
    CallbackShowInstallInfo cb = getCallbackShowInstallInfoTheOne();
    lastAction = info;
    if( cb && !quiet )
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
	v->addDmCryptNames(dmnum);
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

const Device*
Storage::deviceByNumber( unsigned long maj, unsigned long min ) const
    {
    const Device* ret=NULL;
    ConstVolPair p = volPair( Volume::notDeleted );
    ConstVolIterator v = p.begin();
    while( v!=p.end() && (maj!=v->majorNr() || min!=v->minorNr()))
       {
       ++v;
       }
    if( v!=p.end() )
	ret = &*v;
    if( !ret )
	{
	ConstContPair c = contPair(Container::DeviceUsable);
	ConstContIterator ci = c.begin();
	while( ci!=c.end() && (maj!=ci->majorNr() || min!=ci->minorNr()))
	    ++ci;
	if( ci!=c.end() )
	    ret = &*ci;
	}
    y2mil( "maj:" << maj << " min:" << min << " ret:" << (ret?ret->device():"NULL") );
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

	for (auto const &it : tmp)
	{
	    switch (it.type())
	    {
		case UB_MD:
		    ret = removeVolume(it.device());
		    break;
		case UB_DM:
		    ret = removeVolume(it.device());
		    break;
		case UB_LVM:
                    if (it.device().size() > 5)
                        ret = removeLvmVg(it.device().substr(5));
                    else
                        y2err("strange UsedbyLvm:\"" << it.device() << "\"");
		    break;
		case UB_DMRAID:
		    break;
		case UB_DMMULTIPATH:
		    break;
		case UB_BTRFS:
		    {
		    BtrfsCo* co;
		    if (haveBtrfs(co))
			ret = co->removeUuid(it.device());
		    else
			ret = STORAGE_BTRFS_CO_NOT_FOUND;
		    }
		    break;
		case UB_MDPART:
		    ret = removeMdPartCo(it.device(), true);
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


    void
    Storage::syncMdadm()
    {
	bool have_mds = false;

	MdCo* md;
	if (haveMd(md))
	    have_mds = true;

	MdPartCoPair p = mdpCoPair();
	if (!p.empty())
	    have_mds = true;

	if (have_mds)
	{
	    mdadm.reset(new EtcMdadm(this, root()));

	    if (haveMd(md))
		md->syncMdadm(getMdadm());

	    for (auto const &it : p)
		it.syncMdadm(getMdadm());
	}
    }


void Storage::rootMounted()
    {
    root_mounted = true;
    if( !root().empty() )
	{
	string d = root() + "/etc";
	if (!checkDir(d))
	    createPath(d);

	syncMdadm();

	if( instsys() )
	    {
	    string path = root()+"/etc/fstab";
	    unlink( path.c_str() );
	    }

	int ret = fstab->changeRootPrefix( root()+"/etc" );
	if (ret == 0)
	    ret = fstab->flush();
	if (ret != 0)
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
Storage::umountDev( const string& device, bool unsetup )
    {
    bool ret = false;
    assertInit();
    y2mil("device:" << device << " unsetup:" << unsetup );
    VolIterator vol;
    if( !readonly() && findVolume( device, vol ) )
	{
	if( vol->umount()==0 )
	    {
	    if( unsetup )
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
    bool didCrsetup = false;
    assertInit();
    y2mil("device:" << device << " mp:" << mp << " ro:" << ro << " opts:" << opts);
    VolIterator vol;
    if( !readonly() && findVolume( device, vol ) )
	{
	if( vol->needCrsetup() )
	    {
	    bool slnt = vol->isSilent();
	    vol->setSilent(true);
	    ret = vol->doCrsetup(true)==0;
	    if( ret==0 )
		didCrsetup = true;
	    vol->setSilent(slnt);
	    }
	if( ret )
	    {
	    string save = vol->getFstabOption();
	    vol->setFstabOption( opts );
	    ret = vol->mount( mp, ro )==0;
	    vol->setFstabOption( save );
	    }
	if( !ret && didCrsetup )
	    vol->crUnsetup();
	}
    else
	ret = false;
    y2mil("ret:" << ret);
    return( ret );
    }

int
Storage::activateEncryption( const string& device, bool on )
    {
    int ret = 0;
    assertInit();
    y2mil("device:" << device << " on:" << on );
    VolIterator vol;
    if( !readonly() && findVolume( device, vol ) )
	{
	bool slnt = vol->isSilent();
	vol->setSilent(true);
	if( on && vol->needCrsetup() )
	    {
	    ret = vol->doCrsetup(true);
	    }
	else if( !on )
	    {
	    ret = vol->crUnsetup(true);
	    }
	vol->setSilent(slnt);
	}
    else
	ret = STORAGE_VOLUME_NOT_FOUND;
    y2mil("ret:" << ret);
    return( ret );
    }


bool
Storage::readFstab( const string& dir, deque<VolumeInfo>& infos )
{
    static Regex disk_part( "^/dev/[sh]d[a-z]+[0-9]+$" );
    infos.clear();
    bool ret = false;
    ConstVolIterator vol;
    assertInit();
    y2mil("dir:" << dir);
    EtcFstab fstab(dir, true);
    for (auto const &i : fstab.getEntries())
    {
	y2mil("entry:" << i);
	if (disk_part.match(i.dentry))
	{
	    VolumeInfo info;
	    info.create = info.format = info.resize = false;
	    info.sizeK = info.origSizeK = 0;
	    info.minor = info.major = 0;
	    info.device = i.dentry;
	    info.mount = i.mount;
	    info.mount_by = MOUNTBY_DEVICE;
	    info.fs = toValueWithFallback(i.fs, FSUNKNOWN);
	    info.fstab_options = boost::join(i.opts, ",");
	    infos.push_back(info);
	}
	else if (findVolume(i.dentry, vol) || findVolume(i.device, vol))
	{
	    VolumeInfo info;
	    vol->getInfo(info);
	    vol->mergeFstabInfo(info, i);
	    y2mil("volume:" << *vol);
	    infos.push_back(info);
	}
    }
    ret = !infos.empty();
    y2mil("ret:" << ret);
    return ret;
}

bool Storage::mountTmpRo( const Volume* vol, string& mp, const string& opts )
    {
    string opt(opts);
    if( opt.empty() )
	opt="ro";
    else
	opt += ",ro";
    return( mountTmp( vol, mp, opt ));
    }

bool Storage::mountTmp( const Volume* vol, string& mdir, const string& opt )
    {
    y2mil( "device:" << vol->device() << " opts:" << opt );
    bool ret = false;
    removeDmTableTo( *vol );
    mdir = tmpDir() + "/tmp-mp-XXXXXX";
    if (mkdtemp(mdir))
    {
	string opts = opt;
	list<string> ls = splitString( vol->getFstabOption(), "," );
	y2mil( "ls=" << ls << " format:" << vol->getFormat() );
	if( opt.find( "subvolid=0" )!=string::npos || vol->getFormat() )
	    {
	    ls.remove_if( string_starts_with("subvol=") );
	    y2mil( "ls=" << ls );
	    }
	if( !ls.empty() )
	    {
	    if( !opts.empty() )
		opts += ",";
	    opts += boost::join(ls, ",");
	    }
	if( vol->getFs()==NTFS )
	{
	    if( !opts.empty() )
		opts += ",";
	    opts += "show_sys_files";
	}

	list<string> opt_l = splitString(opt, ",");
	bool ro = find(opt_l.begin(), opt_l.end(), "ro") != opt_l.end();

	if( mountDev( vol->device(), mdir, ro, opts ) )
	{
	    ret = true;
	}
	else
	{
	    rmdir(mdir.c_str());
	    mdir.erase();
	}
    }

    y2mil( "ret:" << ret << " mp:" << mdir );
    return( ret );
    }


    int
    Storage::setUserdata(const string& device, const map<string, string>& userdata)
    {
	assertInit();

	int ret = 0;

	Device* tmp = findDevice(device);
	if (tmp)
	{
	    tmp->setUserdata(userdata);
	}
	else
	{
	    ret = STORAGE_DEVICE_NOT_FOUND;
	}

	y2mil("device:" << device << " ret:" << ret);
	return ret;
    }


    int
    Storage::getUserdata(const string& device, map<string, string>& userdata)
    {
	assertInit();

	int ret = 0;

	Device* tmp = findDevice(device);
	if (tmp)
	{
	    userdata = tmp->getUserdata();
	}
	else
	{
	    ret = STORAGE_DEVICE_NOT_FOUND;
	}

	y2mil("device:" << device << " ret:" << ret);
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
	if( (vol->getEncryption()!=ENC_NONE && vol->needCryptPwd()) || 
	    (vol->getEncryption()==ENC_NONE && 
	     (vol->getFs() == FSUNKNOWN || vol->getFs() == FSNONE || vol->getFs() == SWAP)))
	{
	    ret = false;
	}
	else if (vol->isUsedBy()&&!isUsedBySingleBtrfs(*vol))
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
	    if( !vol->isMounted() && mountTmpRo( &(*vol), mp ) )
		needUmount = true;
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

	    if( vol->needCrsetup() && vol->doCrsetup(true) )
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
    y2mil("device:"<<device);
    map<string,string>::iterator i=pwdBuf.find(device);
    if( i!=pwdBuf.end() )
	{
	VolIterator vol;
	if( findVolume( device, vol ) )
	    vol->setCryptPwd( i->second );
	pwdBuf.erase(i);
	}
    }


    void
    Storage::logFreeInfo(const string& Dir) const
    {
	string fname(Dir + "/free.info.tmp");

	XmlFile xml;
	xmlNode* node = xmlNewNode("free");
	xml.setRootElement(node);

	for (auto const &it : free_infos)
	{
	    xmlNode* tmp = xmlNewChild(node, "free");
	    setChildValue(tmp, "device", it.first);
	    it.second.saveData(tmp);
	}

	xml.save(fname);

	handleLogFile(fname);
    }


    void
    Storage::readFreeInfo(const string& fname)
    {
	XmlFile file(fname);
	const xmlNode* root = file.getRootElement();
	const xmlNode* free = getChildNode(root, "free");
	if (free)
	{
	    for (auto const &it : getChildNodes(free, "free"))
	    {
		string device;
		getChildValue(it, "device", device);
		mapInsertOrReplace(free_infos, device, FreeInfo(it));
	    }
	}
    }


    void
    Storage::logArchInfo(const string& Dir) const
    {
	string fname(Dir + "/arch.info.tmp");

	XmlFile xml;
	xmlNode* node = xmlNewNode("arch");
	xml.setRootElement(node);
	archinfo.saveData(node);
	xml.save(fname);

	handleLogFile(fname);
    }


    void
    Storage::readArchInfo(const string& fname)
    {
	XmlFile file(fname);
	const xmlNode* root = file.getRootElement();
	const xmlNode* node = getChildNode(root, "arch");
	if (node)
	    archinfo.readData(node);
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
	for (auto const &i : cont)
	    tmp.push_back(i->getCopy());
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
	    for (auto const *i : b->second)
		cont.push_back(i->getCopy());
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
    bool ret = backups.find(name) != backups.end();
    y2mil("name:" << name << " ret:" << ret);
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
		ret = (*i)->compareContainer( **j, verbose_log ) && ret;
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
          MdPartCo::activate(val, tmpDir());
          Dm::activate(val);
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
	    string dir = prependRoot(mount);
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
    string cmd(UDEVADMBIN " settle --timeout=20");
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

unsigned long long Storage::sizeK( const string& device )
    {
    unsigned long long ret = 0;
    int fd = open(device.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd >= 0)
	{
	uint64_t bytes = 0;
        int rcode = ioctl(fd, BLKGETSIZE64, &bytes);
	y2mil("BLKGETSIZE64 rcode:" << rcode << " bytes:" << bytes);
	if (rcode == 0 && bytes != 0)
            ret = bytes / 1024;
	else
            {
	    unsigned long blocks;
	    rcode = ioctl(fd, BLKGETSIZE, &blocks);
	    y2mil("BLKGETSIZE rcode:" << rcode << " blocks:" << blocks);
	    if (rcode == 0 && blocks != 0)
                ret = blocks*2;
            }
	close(fd);
	}
    y2mil("device:" << device << " ret:" << ret );
    return( ret );
    }

int
Storage::zeroDevice(const string& device, bool random,
		    unsigned long long startK, unsigned long long endK)
{
    y2mil("device:" << device << " random:" << random <<
	  " startK:" << startK << " endK:" << endK);

    waitForDevice(device);

    int ret = 0;

    const string source = (random ? "/dev/urandom" : "/dev/zero");

    SystemCmd c;
    string cmd;

    cmd = WIPEFSBIN " -a " + quote(device);
    c.execute(cmd);

    unsigned long long sz = sizeK(device);
    if( sz>0 )
	startK = min(startK, sz);
    cmd = DDBIN " if=" + source + " of=" + quote(device) + " bs=1k count=" + decString(startK) + " conv=nocreat";
    if (c.execute(cmd) != 0)
	ret = STORAGE_ZERO_DEVICE_FAILED;

    waitForDevice(device);

    y2mil("ret:" << ret);
    return ret;
}

bool Storage::loadModuleIfNeeded(const string& module )
    {
    bool ret = false;
    y2mil( "module:" << module );
    string kname = "^" + boost::replace_all_copy(module, "-", "_") + " ";
    SystemCmd c;
    c.execute( GREPBIN " " + quote(kname) + " /proc/modules" );
    if( c.numLines()==0 )
        {
        c.execute( MODPROBEBIN " " + module );
        ret = c.retcode()==0;
        }
    return( ret );
    }



std::ostream& operator<<(std::ostream& s, const Storage& v)
    {
    v.printInfo(s);
    return(s);
    }

std::ostream& operator<<(std::ostream& s, Storage& v)
    {
    v.assertInit();
    v.printInfo(s);
    return(s);
    }

list<string> Storage::tmp_dirs;

void Storage::clean_tmpdir()
     {
     list<string>::iterator i = tmp_dirs.begin();
     while( i!=tmp_dirs.end() )
	{
	if( access( i->c_str(), W_OK )==0 )
	    rmdir( i->c_str() );
	i = tmp_dirs.erase(i);
	}
     }

CallbackLogDo logger_do_fnc = defaultLogDo;
CallbackLogQuery logger_query_fnc = defaultLogQuery;

void setLogDoCallback(CallbackLogDo pfnc) { logger_do_fnc = pfnc; }
CallbackLogDo getLogDoCallback() { return logger_do_fnc; }
void setLogQueryCallback( CallbackLogQuery pfnc) { logger_query_fnc = pfnc; }
CallbackLogQuery getLogQueryCallback() { return logger_query_fnc; }


    // workaround for broken YCP bindings
    CallbackProgressBar progress_bar_cb_ycp = NULL;
    CallbackShowInstallInfo install_info_cb_ycp = NULL;
    CallbackInfoPopup info_popup_cb_ycp = NULL;
    CallbackYesNoPopup yesno_popup_cb_ycp = NULL;
    CallbackCommitErrorPopup commit_error_popup_cb_ycp = NULL;
    CallbackPasswordPopup password_popup_cb_ycp = NULL;

}
