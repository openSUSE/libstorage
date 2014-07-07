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


#include <sys/stat.h>
#include <sstream>

#include "storage/Btrfs.h"
#include "storage/StorageTypes.h"
#include "storage/Container.h"
#include "storage/AppUtil.h"
#include "storage/SystemInfo/SystemInfo.h"
#include "storage/Storage.h"
#include "storage/SystemCmd.h"
#include "storage/StorageDefines.h"


namespace storage
{
using namespace std;

Btrfs::Btrfs(const BtrfsCo& d, const Volume& v, unsigned long long sz,
             const list<string>& devs ) : Volume(d, v), devices(devs)
    {
    y2mil("constructed btrfs vol size:" << sz << " devs:" << devs );
    y2mil("constructed btrfs vol from:" << v );
    setFs(BTRFS);
    if( getMountBy()!=MOUNTBY_LABEL )
	changeMountBy(MOUNTBY_UUID);
    setSize( sz );
    }

Btrfs::Btrfs(const BtrfsCo& d, const Volume& v ) : Volume(d, v)
    {
    y2mil("constructed btrfs vol from:" << v );
    y2mil( "fs:" << fs << " det:" << detected_fs );
    setCreated(false);
    changeMountBy(MOUNTBY_UUID);
    devices.push_back(v.device());
    y2mil("constructed btrfs vol:" << *this );
    }

Btrfs::Btrfs(const BtrfsCo& d, const xmlNode* node ) : Volume(d, node)
    {
    for (auto const *it : getChildNodes(node, "devices"))
	{
	string s;
	getChildValue(it, "devices", s);
	devices.push_back(s);
	}
    for (auto const *it : getChildNodes(node, "dev_add"))
	{
	string s;
	getChildValue(it, "devices", s);
	dev_add.push_back(s);
	}
    for (auto const *it : getChildNodes(node, "dev_rem"))
	{
	string s;
	getChildValue(it, "devices", s);
	dev_rem.push_back(s);
	}
    for (auto const *it : getChildNodes(node, "subvolumes"))
	{
	subvol.push_back(Subvolume(it));
	}
    }


Btrfs::Btrfs(const BtrfsCo& c, const Btrfs& v) : Volume(c, v), 
    devices(v.devices), subvol(v.subvol)
    {
    y2deb("copy-constructed Btrfs from " << v.dev);
    }


Btrfs::~Btrfs()
    {
    y2deb("destructed Btrfs " << dev);
    }

BtrfsCo* Btrfs::co()
    {
    Container* con = const_cast<Container*>(cont);
    return(dynamic_cast<storage::BtrfsCo*>(con));
    }

list<string> 
Btrfs::getDevices( bool add_del ) const
    {
    list<string> ret;
    getDevices( ret, add_del );
    return( ret );
    }

void 
Btrfs::getDevices( list<string>& devs, bool add_del ) const
    {
    y2mil( "add_del:" << add_del );
    devs = devices;
    if( add_del )
	{
	if( !dev_add.empty() )
	    devs.insert( devs.end(), dev_add.begin(), dev_add.end() );
	if( !dev_rem.empty() )
	    for (auto const &s : dev_rem)
		devs.remove(s);
	}
    y2mil( "devs:" << devs );
    }

void Btrfs::detectSubvol()
    {
    y2mil( "dev:" << device() );
    if( getFormat() )
        clearSubvol();
    else
        {
        bool mounted = false;
        string mp = getMount();
        if( !isMounted() && getStorage()->mountTmpRo( this, mp ) )
            mounted = true;
        if( !mp.empty() )
            {
            clearSubvol();
            SystemCmd cmd(BTRFSBIN " subvolume list -a -p " + quote(mp));
            for (auto const &s : cmd.stdout())
                {
		string parent;
		auto pos1 = s.find(" parent ");
		if (pos1 != string::npos)
		    pos1 = s.find_first_not_of(app_ws, pos1 + 6);
		if (pos1 != string::npos)
		    parent = s.substr(pos1, s.find_last_not_of(app_ws));

		string subvol;
		auto pos2 = s.find(" path ");
		if (pos2 != string::npos)
		    pos2 = s.find_first_not_of(app_ws, pos2 + 5);
		if (pos2 != string::npos)
		    subvol = s.substr(pos2, s.find_last_not_of(app_ws));
		if (boost::starts_with(subvol, "<FS_TREE>/"))
		    subvol.erase(0, 10);

		// Subvolume can already be deleted, in which case parent is "0"
		// (and path "DELETED"). That is a temporary state.
		if (parent != "0" && !subvol.empty())
		    addSubvol(subvol);
		}
            }
        if( mounted )
            {
            getStorage()->umountDev( device() );
            if( mp!=getMount() )
                rmdir( mp.c_str() );
            }
        }
    y2mil( "ret dev:" << device() << " subvol:" << subvol );
    }

void Btrfs::addSubvol( const string& path )
    {
    y2mil( "path:\"" << path << "\"" );
    Subvolume v( path );
    if( !contains( subvol, v ))
	subvol.push_back( v );
    else
	y2war( "subvolume " << v << " already exists!" );
    }

bool
Btrfs::existSubvolume( const string& name )
    {
    bool ret=false;
    y2mil( "name:" << name );
    for (auto const &s : subvol) {
	if (ret) break;
	ret = !s.deleted() && s.path() == name && (!getFormat() || s.created());
    }
    y2mil( "ret:" << ret );
    return( ret );
    }

int
Btrfs::createSubvolume( const string& name )
    {
    int ret=0;
    y2mil( "name:" << name );
    list<Subvolume>::iterator i=subvol.begin();
    while( i!=subvol.end() && !i->deleted() && i->path()!=name )
	++i;
    if( i==subvol.end() )
	{
	Subvolume v( name );
	v.setCreated();
	subvol.push_back( v );
	}
    else if( getFormat() )
	{
	i->setCreated();
	}
    else
	ret = BTRFS_SUBVOL_EXISTS;
    y2mil( "ret:" << ret );
    return( ret );
    }

int
Btrfs::deleteSubvolume( const string& name )
    {
    int ret=0;
    y2mil( "name:" << name );
    list<Subvolume>::iterator i=subvol.begin();
    while( i!=subvol.end() && i->path()!=name )
	++i;
    if( i!=subvol.end() )
	{
	if( i->created() )
	    subvol.erase(i);
	else
	    i->setDeleted();
	}
    else
	ret = BTRFS_SUBVOL_NON_EXISTS;
    y2mil( "ret:" << ret );
    return( ret );
    }

int Btrfs::extendVolume( const string& dev )
    {
    list<string> d;
    d.push_back(dev);
    return( extendVolume(d));
    }

int Btrfs::extendVolume( const list<string>& devs )
    {
    int ret = 0;
    y2mil( "name:" << name() << " devices:" << devs );
    y2mil( "this:" << *this );

    list<string>::const_iterator i=devs.begin();
    list<string>::iterator p;
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	if( (p=find( devices.begin(), devices.end(), d ))!=devices.end() ||
	    (p=find( dev_add.begin(), dev_add.end(), d ))!=dev_add.end())
	    ret = BTRFS_DEV_ALREADY_CONTAINED;
	else if( (p=find( dev_rem.begin(), dev_rem.end(), d )) != dev_rem.end() && 
	         !getStorage()->deletedDevice( d ) )
	    {
	    }
	else if( !getStorage()->knownDevice( d, true ) )
	    {
	    ret = BTRFS_DEVICE_UNKNOWN;
	    }
	else if( !getStorage()->canUseDevice( d, true ) )
	    {
	    ret = BTRFS_DEVICE_USED;
	    }
	++i;
	}
    i=devs.begin();
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	if( (p=find( dev_rem.begin(), dev_rem.end(), d )) != dev_rem.end() && 
	    !getStorage()->deletedDevice( d ) )
	    {
	    devices.push_back( *p );
	    dev_rem.erase( p );
	    }
	else
	    {
	    if( format )
		devices.push_back( d );
	    else
		dev_add.push_back( d );
	    if( !getStorage()->isDisk(d))
		getStorage()->changeFormatVolume( d, false, FSNONE );
	    }
	getStorage()->setUsedByBtrfs(d, getUuid());
	setSize( size_k+getStorage()->deviceSize( d ) );
	++i;
	}
    if( ret==0 && dev_add.size()+devices.size()-dev_rem.size()<=0 )
	ret = BTRFS_HAS_NONE_DEV;
    y2mil( "this:" << *this );
    y2mil("ret:" << ret);
    return( ret );
    }

int Btrfs::shrinkVolume( const string& dev )
    {
    list<string> d;
    d.push_back(dev);
    return( shrinkVolume(d));
    }

int Btrfs::shrinkVolume( const list<string>& devs )
    {
    int ret = 0;
    y2mil("name:" << name() << " devices:" << devs);
    y2mil("this:" << *this);

    list<string>::const_iterator i = devs.begin();
    list<string>::iterator p;
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	if( (p=find( devices.begin(), devices.end(), d ))==devices.end() &&
	    (p=find( dev_add.begin(), dev_add.end(), d ))==dev_add.end())
	    ret = BTRFS_DEV_NOT_FOUND;
	++i;
	}
    unsigned long long s = size_k;
    i = devs.begin();
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	if( (p=find( dev_add.begin(), dev_add.end(), d ))!=dev_add.end())
	    dev_add.erase(p);
	else if( format )
	    {
	    if( (p=find( devices.begin(), devices.end(), d ))!=devices.end())
		devices.erase(p);
	    }
	else
	    dev_rem.push_back(d);
	getStorage()->clearUsedBy(d);
	s -= min(getStorage()->deviceSize( d ),s);
	setSize( size_k-getStorage()->deviceSize( d ) );
	++i;
	}
    if( ret==0 && dev_add.size()+devices.size()-devs.size()<=0 )
	ret = BTRFS_HAS_NONE_DEV;
    if( ret == 0 )
	{
	setSize( s );
	}
    y2mil("this:" << *this);
    y2mil("ret:" << ret);
    return ret;
    }

int Btrfs::doExtend()
    {
    y2mil( "this:" << *this );
    TmpMount tmp_mount;
    int ret = prepareTmpMount(tmp_mount);
    list<string> devs = dev_add;
    list<string>::const_iterator d = devs.begin();
    SystemCmd c;
    while( ret==0 && d!=devs.end() )
	{
	getStorage()->showInfoCb(extendText(true, *d),silent);
	string cmd = BTRFSBIN " device add " + quote(*d) + " " + quote(tmp_mount.mount_point);
	c.execute( cmd );
	if( c.retcode()==0 )
	    {
	    devices.push_back(*d);
	    dev_add.remove_if( bind2nd(equal_to<string>(),*d));
	    }
	else
	    ret = BTRFS_EXTEND_FAIL;
	++d;
	}
    if (tmp_mount.needs_umount)
    {
	ret = umountTmpMount(tmp_mount, ret);
	is_mounted = tmp_mount.was_mounted;
    }
    y2mil( "this:" << *this );
    y2mil("ret:" << ret);
    return( ret );
    }

int Btrfs::doReduce()
    {
    y2mil( "this:" << *this );
    TmpMount tmp_mount;
    int ret = prepareTmpMount(tmp_mount);
    list<string> devs = dev_rem;
    list<string>::const_iterator d = devs.begin();
    SystemCmd c;
    while( ret==0 && d!=devs.end() )
	{
	getStorage()->showInfoCb(reduceText(true, *d),silent);
	string cmd = BTRFSBIN " device delete " + quote(*d) + " " + quote(tmp_mount.mount_point);
	c.execute( cmd );
	if( c.retcode()==0 )
	    {
	    devices.remove_if( bind2nd(equal_to<string>(),*d));
	    dev_rem.remove_if( bind2nd(equal_to<string>(),*d));
	    }
	else
	    ret = BTRFS_REDUCE_FAIL;
	++d;
	}
    if (tmp_mount.needs_umount)
    {
	ret = umountTmpMount(tmp_mount, ret);
	is_mounted = tmp_mount.was_mounted;
    }
    y2mil( "this:" << *this );
    y2mil("ret:" << ret);
    return( ret );
    }

int Btrfs::doDeleteSubvol()
    {
    TmpMount tmp_mount;
    int ret = prepareTmpMount(tmp_mount, false, "subvolid=0");
    if( ret==0 )
	{
	SystemCmd c;
	string cmd = BTRFSBIN " subvolume delete ";
	for (auto &s : subvol)
	    {
	    if (s.deleted())
		{
		getStorage()->showInfoCb(deleteSubvolText(true, s.path()), silent);
		c.execute(cmd + quote(tmp_mount.mount_point + '/' + s.path()));
		if( c.retcode()==0 )
		    s.setDeleted(false);
		else
		    ret = BTRFS_DELETE_SUBVOL_FAIL;
		}
	    }
	}
    if (tmp_mount.needs_umount)
    {
	ret = umountTmpMount(tmp_mount, ret);
	is_mounted = tmp_mount.was_mounted;
    }
    y2mil( "ret:" << ret );
    return( ret );
    }

int Btrfs::doCreateSubvol()
    {
    TmpMount tmp_mount;
    int ret = prepareTmpMount(tmp_mount, false, "subvolid=0");
    if( ret==0 )
	{
	SystemCmd c;
	string cmd = BTRFSBIN " subvolume create ";
	for (auto &s : subvol)
	    {
	    if (s.created())
		{
		getStorage()->showInfoCb(createSubvolText(true, s.path()), silent);
		y2mil("dir:" << tmp_mount.mount_point << " path:" << s.path());
		string path = tmp_mount.mount_point + "/" + s.path();
		string dir = path.substr( 0, path.find_last_of( "/" ) );
		y2mil( "path:" << path << " dir:" << dir );
		if( !checkDir( dir ) )
		    {
		    y2mil( "create path:" << dir );
		    createPath( dir );
		    }
		c.execute(cmd + quote(path));
		if( c.retcode()==0 )
		    s.setCreated(false);
		else
		    ret = BTRFS_CREATE_SUBVOL_FAIL;
		}
	    }
	}
    if (tmp_mount.needs_umount)
    {
	ret = umountTmpMount(tmp_mount, ret);
	is_mounted = tmp_mount.was_mounted;
    }
    y2mil( "ret:" << ret );
    return( ret );
    }

list<string> Btrfs::getSubvolAddDel( bool add ) const
    {
    list<string> ret;
    for (auto const &i : subvol)
	{
	if (!add && i.deleted())
	    ret.push_back(i.path());
	if (add && i.created())
	    ret.push_back(i.path());
	}
    if( !ret.empty() )
	y2mil( "add:" << add << " ret:" << ret );
    return( ret );
    }

void
Btrfs::countSubvolAddDel( unsigned& add, unsigned& del ) const
    {
    add = del = 0;
    for (auto const &s : subvol)
	{
	if (s.deleted())
	    del++;
	if (s.created())
	    add++;
	}
    if( add>0 || del>0 )
	y2mil( "add:" << add << " del:" << del );
    }

string
Btrfs::subvolNames( bool added ) const
    {
    string ret;
    for (auto const &s : subvol)
	{
	if ((added && s.created()) || (!added && s.deleted()))
	    {
	    if( !ret.empty() )
		ret += ' ';
	    ret += s.path();
	    }
	}
    y2mil( "ret:" << ret );
    return( ret );
    }

int Btrfs::setFormat( bool val, storage::FsType new_fs )
    {
    int ret = 0;
    y2mil("device:" << dev << " val:" << val << " fs:" << toString(new_fs));
    ret = Volume::setFormat( val, new_fs );
    if( ret==0 )
	{
        if( val )
            uuid = co()->fakeUuid();
        detectSubvol();
	getStorage()->setBtrfsUsedBy( this );
	}
    y2mil("device:" << *this );
    y2mil("ret:" << ret);
    return( ret );
    }

void Btrfs::unuseDev() const
    {
    for (auto const &s : devices)
	getContainer()->getStorage()->clearUsedBy(s);
    for (auto const &s : dev_add)
	getContainer()->getStorage()->clearUsedBy(s);
    }

int Btrfs::clearSignature()
    {
    int ret = 0;
    // no need to actually remove signature from single volume btrfs filesystems
    if( devices.size()>1 )
	{
	for (auto const &s : devices)
	    getContainer()->getStorage()->zeroDevice(s);
	}
    y2mil( "ret:" << ret );
    return( ret );
    }


    void
    Btrfs::changeDeviceName( const string& old, const string& nw )
    {
	if (dev == old)
	{
	    Volume const* v;
	    if (getStorage()->findVolume(old, v))
		setNameDevice(v->name(), nw);
	    else
		y2err("device " << old << " not fount");
	}

	list<string>::iterator i = find(devices.begin(), devices.end(), old);
	if (i != devices.end())
	    *i = nw;
	i = find(dev_add.begin(), dev_add.end(), old);
	if (i != dev_add.end())
	    *i = nw;
	i = find(dev_rem.begin(), dev_rem.end(), old);
	if (i != dev_rem.end())
	    *i = nw;
    }


Text Btrfs::removeText(bool doing) const
    {
    Text txt;
    string d = boost::join( devices, " " );
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device names e.g /dev/sda1 /dev/sda2
	txt = sformat( _("Deleting Btrfs volume on devices %1$s"), d.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device names e.g /dev/sda1 /dev/sda2
	// %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Delete Btrfs volume on devices %1$s (%2$s)"),
			   d.c_str(), sizeString().c_str() );
	}
    return( txt );
    }


string Btrfs::udevPath() const
    {
    Volume const *v = findRealVolume();
    if( v )
	return( v->udevPath() );
    else
	return( Device::udevPath() );
    }

list<string> Btrfs::udevId() const
    {
    Volume const *v = findRealVolume();
    if( v )
	return( v->udevId() );
    else
	return( Device::udevId() );
    }

string Btrfs::sysfsPath() const
    {
    string ret;
    Volume const *v = findRealVolume();
    if( v )
	ret = v->sysfsPath();
    return( ret );
    }

Volume const * Btrfs::findRealVolume() const
    {
    Volume const *v = NULL;
    if( !getStorage()->findVolume( devices.front(), v, true ))
	v = NULL;
    return( v );
    }

Text Btrfs::formatText(bool doing) const
    {
    Text txt;
    bool done = false;
    if( devices.size()+dev_add.size()==1 )
	{
	Volume const *v = findRealVolume();
	if( v!=NULL )
	    {
	    y2mil( "this: " << *this );
	    y2mil( "found:" << *v );
	    txt = v->formatText(doing);
	    done = true;
	    }
	}
    if( !done )
	{
	list<string> tl = devices;
	tl.insert( tl.end(), dev_add.begin(), dev_add.end() );
	string d = boost::join( tl, " " );
	if( doing )
	    {
	    // displayed text during action,
	    // %1$s is replaced by size (e.g. 623.5 MB)
	    // %2$s  is repleace by a list of names e.g /dev/sda1 /dev/sda2
	    txt = sformat( _("Formatting Btrfs volume of size %1$s (used devices:%2$s)"),
	                   sizeString().c_str(), d.c_str() );
	    }
	else
	    {
	    if( !mp.empty() )
	    // displayed text before action, 
	    // %1$s is replaced by size (e.g. 623.5 MB)
	    // %2$s  is repleace by a list of names e.g /dev/sda1 /dev/sda2
	    // %3$s is replaced by mount point (e.g. /usr) 
		txt = sformat( _("Format Btrfs volume of size %1$s for %3$s (used devices:%2$s)"), 
			       sizeString().c_str(), d.c_str(), mp.c_str() );
	    else 
	    // displayed text before action,
	    // %1$s is replaced by size (e.g. 623.5 MB)
	    // %2$s  is repleace by a list of names e.g /dev/sda1 /dev/sda2
		txt = sformat( _("Format Btrfs volume of size %1$s (used devices:%2$s)"), 
			       sizeString().c_str(), d.c_str() );
	    }
	}
    return( txt );
    }

void
Btrfs::getCommitActions(list<commitAction>& l) const
    {
    Volume::getCommitActions( l );
    if( !l.empty() && l.back().stage==FORMAT )
	{
	Volume const *v = NULL;
	if( getStorage()->findVolume( l.back().vol()->device(), v, true ) &&
	    getStorage()->isUsedBySingleBtrfs(*v) )
	    {
	    y2mil( "found:" << *v );
	    if( v->created() )
		{
		y2mil( "removing:" << l.back() );
		l.pop_back();
		}
	    }
	}

    for (auto const &s : dev_add)
	l.push_back(commitAction(INCREASE, cont->type(), extendText(false, s), this, true));
    for (auto const &s : dev_rem)
	l.push_back(commitAction(DECREASE, cont->type(), reduceText(false, s), this, false));

    unsigned rem, add;
    countSubvolAddDel( add, rem );
    if( rem>0 )
	{
	list<string> sl = getSubvolAddDel( false );
	for (auto const &s : sl)
	    l.push_back(commitAction(SUBVOL, cont->type(),
				     deleteSubvolText(false, s), this, true));
	}
    if( add>0 )
	{
	list<string> sl = getSubvolAddDel( true );
	for (auto const &s : sl)
	    l.push_back(commitAction(SUBVOL, cont->type(),
				     createSubvolText(false, s), this, false));
	}
    }


    int
    Btrfs::extraFstabAdd(EtcFstab* fstab, const FstabChange& change)
    {
	if (getMount() == "/")
	{
	    string def_subvol = getStorage()->getDefaultSubvolName();

	    for (auto const &s : subvol)
	    {
		string path = s.path();
		if (!def_subvol.empty() && boost::starts_with(s.path(), def_subvol + "/"))
		    path = path.substr(def_subvol.size() + 1);

		FstabChange tmp_change = change;
		tmp_change.mount += (tmp_change.mount == "/" ? "" : "/") + path;
		tmp_change.opts.remove("defaults");
		tmp_change.opts.remove_if(string_starts_with("subvol="));
		tmp_change.opts.push_back("subvol=" + s.path());
		fstab->addEntry(tmp_change);
	    }
	}

	return 0;
    }


    int
    Btrfs::extraFstabUpdate(EtcFstab* fstab, const FstabKey& key, const FstabChange& change)
    {
	if (getMount() == "/")
	{
	    string def_subvol = getStorage()->getDefaultSubvolName();

	    for (auto const &s : subvol)
	    {
		string path = s.path();
		if (!def_subvol.empty() && boost::starts_with(s.path(), def_subvol + "/"))
		    path = path.substr(def_subvol.size() + 1);

		FstabKey tmp_key(key);
		tmp_key.mount += (tmp_key.mount == "/" ? "" : "/") + path;
		FstabChange tmp_change = change;
		tmp_change.mount += (tmp_change.mount == "/" ? "" : "/") + path;
		tmp_change.opts.remove("defaults");
		tmp_change.opts.remove_if(string_starts_with("subvol="));
		tmp_change.opts.push_back("subvol=" + s.path());
		fstab->updateEntry(tmp_key, tmp_change);
	    }
	}

	return 0;
    }


    int
    Btrfs::extraFstabRemove(EtcFstab* fstab, const FstabKey& key)
    {
	if (getMount() == "/")
	{
	    string def_subvol = getStorage()->getDefaultSubvolName();

	    for (auto const &s : subvol)
	    {
		string path = s.path();
		if (!def_subvol.empty() && boost::starts_with(s.path(), def_subvol + "/"))
		    path = path.substr(def_subvol.size() + 1);

		FstabKey tmp_key(key);
		tmp_key.mount += (tmp_key.mount == "/" ? "" : "/") + path;
		fstab->removeEntry(tmp_key);
	    }
	}

	return 0;
    }


Text
Btrfs::extendText(bool doing, const string& dev) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, 
	// %1$s and %2$s are replaced by a device names (e.g. /dev/hda1)
        txt = sformat( _("Extending Btrfs volume %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    else
        {
        // displayed text before action, 
	// %1$s and %2$s are replaced by a device names (e.g. /dev/hda1)
        txt = sformat( _("Extend Btrfs volume %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    return( txt );
    }

Text
Btrfs::reduceText(bool doing, const string& dev) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, 
	// %1$s and %2$s are replaced by a device names (e.g. /dev/hda1)
        txt = sformat( _("Reducing Btrfs volume %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    else
        {
        // displayed text before action, 
	// %1$s and %2$s are replaced by a device names (e.g. /dev/hda1)
        txt = sformat( _("Reduce Btrfs volume %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    return( txt );
    }

Text Btrfs::createSubvolText(bool doing, const string& name) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by subvolume name e.g. tmp
	// %2$s is replaced by device name e.g. /dev/hda1
	txt = sformat( _("Creating subvolume %1$s on device %2$s"), 
		       name.c_str(), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by subvolume name e.g. tmp
	// %2$s is replaced by device name e.g. /dev/hda1
	txt = sformat( _("Create subvolume %1$s on device %2$s"), 
		       name.c_str(), dev.c_str() );
	}
    return( txt );
    }

Text Btrfs::deleteSubvolText(bool doing, const string& name) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by subvolume name e.g. tmp
	// %2$s is replaced by device name e.g. /dev/hda1
	txt = sformat( _("Removing subvolume %1$s on device %2$s"), 
		       name.c_str(), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by subvolume name e.g. tmp
	// %2$s is replaced by device name e.g. /dev/hda1
	txt = sformat( _("Remove subvolume %1$s on device %2$s"), 
		       name.c_str(), dev.c_str() );
	}
    return( txt );
    }

void Btrfs::getInfo( BtrfsInfo& info ) const
    {
    Volume::getInfo(info.v);
    info.devices = devices;
    info.devices_add = dev_add;
    info.devices_rem = dev_rem;

    info.subvol.clear();
    info.subvol_add.clear();
    info.subvol_rem.clear();
    for (auto const &s : subvol)
    {
	if (s.deleted())
	    info.subvol_rem.push_back(s.path());
	else if (s.created())
	    info.subvol_add.push_back(s.path());
	else
	    info.subvol.push_back(s.path());
    }
    }

std::ostream& operator<< (std::ostream& s, const Btrfs& v )
    {
    s << "Btrfs " << dynamic_cast<const Volume&>(v);
    s << " devices:" << v.devices;
    if( !v.dev_add.empty() )
	s << " dev_add:" << v.dev_add;
    if( !v.dev_rem.empty() )
	s << " dev_rem:" << v.dev_rem;
    if( !v.subvol.empty() )
	s << " subvol:" << v.subvol;
    return( s );
    }


bool Btrfs::equalContent( const Btrfs& rhs ) const
    {
    return( Volume::equalContent(rhs) && devices==rhs.devices &&
            dev_add==rhs.dev_add && dev_rem==rhs.dev_rem &&
            subvol==rhs.subvol );
    }


void Btrfs::logDifference(std::ostream& log, const Btrfs& rhs) const
    {
    Volume::logDifference(log, rhs);
    string tmp;
    for (auto const &it : devices)
	if (!contains(rhs.devices, it))
	    tmp += it + "-->";
    for (auto const &it : rhs.devices)
	if (!contains(devices, it))
	    tmp += "<--" + it;
    if (!tmp.empty())
	log << " Devices:" << tmp;

    tmp.erase();

    for (auto const &it : dev_add)
	if (!contains(rhs.dev_add, it))
	    tmp += it + "-->";
    for (auto const &it : rhs.dev_add)
	if (!contains(dev_add, it))
	    tmp += "<--" + it;
    if (!tmp.empty())
	log << " DevAdd:" << tmp;

    tmp.erase();

    for (auto const &it : dev_rem)
	if (!contains(rhs.dev_rem, it))
	    tmp += it + "-->";
    for (auto const &it : rhs.dev_rem)
	if (!contains(dev_rem, it))
	    tmp += "<--" + it;
    if (!tmp.empty())
	log << " DevRem:" << tmp;

    tmp.erase();

    for (auto const &s : subvol)
	{
	if (s.deleted())
	    tmp += "<--" + s.path();
	else if (s.created())
	    tmp += s.path() + "-->";
	}

    if (!tmp.empty())
	log << " SubVol:" << tmp;
    }


    void
    Btrfs::saveData(xmlNode* node) const
    {
	Volume::saveData(node);

	setChildValue(node, "devices", devices);
	setChildValueIf(node, "dev_add", dev_add, !dev_add.empty());
	setChildValueIf(node, "dev_rem", dev_rem, !dev_rem.empty());

	setChildValueIf(node, "subvolume", subvol, !subvol.empty());
    }


bool Btrfs::needCreateSubvol( const Btrfs& v )
    {
    unsigned dummy, cnt;
    v.countSubvolAddDel( cnt, dummy );
    return( cnt>0 );
    }

bool Btrfs::needDeleteSubvol( const Btrfs& v )
    {
    unsigned dummy, cnt;
    v.countSubvolAddDel( dummy, cnt );
    return( cnt>0 );
    }

bool Btrfs::needReduce( const Btrfs& v )
    {
    return( !v.dev_rem.empty() );
    }

bool Btrfs::needExtend( const Btrfs& v )
    {
    return( !v.dev_add.empty() );
    }

}
