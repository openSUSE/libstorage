/*
 * Copyright (c) [2004-2011] Novell, Inc.
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
#include "storage/SystemInfo.h"
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
    list<const xmlNode*> l = getChildNodes(node, "devices");
    for( list<const xmlNode*>::const_iterator it=l.begin(); it!=l.end(); ++it )
	{
	string s;
	getChildValue(*it, "devices", s);
	devices.push_back(s);
	}
    l = getChildNodes(node, "dev_add");
    for( list<const xmlNode*>::const_iterator it=l.begin(); it!=l.end(); ++it )
	{
	string s;
	getChildValue(*it, "devices", s);
	dev_add.push_back(s);
	}
    l = getChildNodes(node, "dev_rem");
    for( list<const xmlNode*>::const_iterator it=l.begin(); it!=l.end(); ++it )
	{
	string s;
	getChildValue(*it, "devices", s);
	dev_rem.push_back(s);
	}
    l = getChildNodes(node, "subvolumes");
    for (list<const xmlNode*>::const_iterator it=l.begin(); it!=l.end(); ++it )
	{
	subvol.push_back(Subvolume(*it));
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

void Btrfs::addSubvol( const string& path )
    {
    y2mil( "path:\"" << path << "\"" );
    Subvolume v( path );
    if( !contains( subvol, v ))
	subvol.push_back( v );
    else
	y2war( "subvolume " << v << " already exists!" );
    }

int
Btrfs::createSubvolume( const string& name )
    {
    int ret=0;
    y2mil( "name:" << name );
    list<Subvolume>::iterator i=subvol.begin();
    while( i!=subvol.end() && !i->deleted() && i->path()!=name )
	++i;
    if( i==subvol.end() || getFormat() )
	{
	Subvolume v( name );
	v.setCreated();
	subvol.push_back( v );
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
	    dev_add.push_back( d );
	    if( !getStorage()->isDisk(d))
		getStorage()->changeFormatVolume( d, false, FSNONE );
	    }
	getStorage()->setUsedBy(d, UB_BTRFS, device());
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
    bool needUmount;
    string m;
    int ret = prepareTmpMount( m, needUmount );
    list<string> devs = dev_add;
    list<string>::const_iterator d = devs.begin();
    SystemCmd c;
    while( ret==0 && d!=devs.end() )
	{
	if( !silent )
	    getStorage()->showInfoCb(extendText(true, *d));
	string cmd = BTRFSBIN " device add " + quote(*d) + " " + m;
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
    if( needUmount )
	ret = umountTmpMount( ret );
    y2mil( "this:" << *this );
    y2mil("ret:" << ret);
    return( ret );
    }

int Btrfs::doReduce()
    {
    y2mil( "this:" << *this );
    bool needUmount;
    string m;
    int ret = prepareTmpMount( m, needUmount );
    list<string> devs = dev_rem;
    list<string>::const_iterator d = devs.begin();
    SystemCmd c;
    while( ret==0 && d!=devs.end() )
	{
	if( !silent )
	    getStorage()->showInfoCb(reduceText(true, *d));
	string cmd = BTRFSBIN " device delete " + quote(*d) + " " + m;
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
    if( needUmount )
	ret = umountTmpMount( ret );
    y2mil( "this:" << *this );
    y2mil("ret:" << ret);
    return( ret );
    }

int Btrfs::doDeleteSubvol()
    {
    bool needUmount;
    string m;
    int ret = prepareTmpMount( m, needUmount );
    if( ret==0 )
	{
	SystemCmd c;
	string cmd = BTRFSBIN " subvolume delete " + m + '/';
	for( list<Subvolume>::iterator i=subvol.begin(); i!=subvol.end(); ++i )
	    {
	    if( i->deleted() )
		{
		if( !silent )
		    getStorage()->showInfoCb( deleteSubvolText(true,i->path()));
		c.execute( cmd + i->path() );
		if( c.retcode()==0 )
		    i->setDeleted(false);
		else
		    ret = BTRFS_DELETE_SUBVOL_FAIL;
		}
	    }
	}
    if( needUmount )
	ret = umountTmpMount( ret );
    y2mil( "ret:" << ret );
    return( ret );
    }

int Btrfs::doCreateSubvol()
    {
    bool needUmount;
    string m;
    int ret = prepareTmpMount( m, needUmount );
    if( ret==0 )
	{
	SystemCmd c;
	string cmd = BTRFSBIN " subvolume create ";
	for( list<Subvolume>::iterator i=subvol.begin(); i!=subvol.end(); ++i )
	    {
	    if( i->created() )
		{
		if( !silent )
		    getStorage()->showInfoCb( createSubvolText(true,i->path()));
		y2mil( "dir:" << m << " path:" << i->path() );
		string path = m + "/" + i->path();
		string dir = path.substr( 0, path.find_last_of( "/" ) );
		y2mil( "path:" << path << " dir:" << dir );
		if( !checkDir( dir ) )
		    {
		    y2mil( "create path:" << dir );
		    createPath( dir );
		    }
		c.execute( cmd + path );
		if( c.retcode()==0 )
		    i->setCreated(false);
		else
		    ret = BTRFS_CREATE_SUBVOL_FAIL;
		}
	    }
	}
    if( needUmount )
	ret = umountTmpMount( ret );
    y2mil( "ret:" << ret );
    return( ret );
    }

list<string> Btrfs::getSubvolAddDel( bool add ) const
    {
    list<string> ret;
    list<Subvolume>::const_iterator i;
    for( i=subvol.begin(); i!=subvol.end(); ++i )
	{
	if( !add && i->deleted() )
	    ret.push_back(i->path());
	if( add && i->created() )
	    ret.push_back(i->path());
	}
    if( !ret.empty() )
	y2mil( "add:" << add << " ret:" << ret );
    return( ret );
    }

void
Btrfs::countSubvolAddDel( unsigned& add, unsigned& del ) const
    {
    add = del = 0;
    for( list<Subvolume>::const_iterator i=subvol.begin();
	 i!=subvol.end(); ++i )
	{
	if( i->deleted() )
	    del++;
	if( i->created() )
	    add++;
	}
    if( add>0 || del>0 )
	y2mil( "add:" << add << " del:" << del );
    }

string
Btrfs::subvolNames( bool added ) const
    {
    string ret;
    for( list<Subvolume>::const_iterator i=subvol.begin();
	 i!=subvol.end(); ++i )
	{
	if( (added && i->created()) ||
	    (!added && i->deleted()))
	    {
	    if( !ret.empty() )
		ret += ' ';
	    ret += i->path();
	    }
	}
    y2mil( "ret:" << ret );
    return( ret );
    }

void Btrfs::unuseDev() const
    {
    for( list<string>::const_iterator s=devices.begin(); s!=devices.end(); ++s )
	getContainer()->getStorage()->clearUsedBy(*s);
    for( list<string>::const_iterator s=dev_add.begin(); s!=dev_add.end(); ++s )
	getContainer()->getStorage()->clearUsedBy(*s);
    }

int Btrfs::clearSignature()
    {
    int ret = 0;
    // no need to actually remove signature from single volume btrfs filesystems
    if( devices.size()>1 )
	{
	for( list<string>::const_iterator s=devices.begin(); s!=devices.end(); ++s )
	    getContainer()->getStorage()->zeroDevice(*s,0);
	}
    y2mil( "ret:" << ret );
    return( ret );
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

Text Btrfs::formatText(bool doing) const
    {
    Text txt;
    bool done = false;
    if( devices.size()+dev_add.size()==1 )
	{
	Volume const *v = NULL;
	if( getStorage()->findVolume( devices.front(), v, true ))
	    {
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
    unsigned rem, add;
    list<string>::const_iterator i;
    if( !dev_add.empty() )
	for( i=dev_add.begin(); i!=dev_add.end(); ++i )
	    l.push_back(commitAction(INCREASE, cont->type(),
			extendText(false, *i), this, true));
    if( !dev_rem.empty() )
	for( i=dev_rem.begin(); i!=dev_rem.end(); ++i )
	    l.push_back(commitAction(DECREASE, cont->type(),
			reduceText(false, *i), this, false));
    countSubvolAddDel( add, rem );
    if( rem>0 )
	{
	list<string> sl = getSubvolAddDel( false );
	for( list<string>::const_iterator i=sl.begin(); i!=sl.end(); ++i )
	    l.push_back(commitAction(SUBVOL, cont->type(),
				     deleteSubvolText(false,*i), this, true));
	}
    if( add>0 )
	{
	list<string> sl = getSubvolAddDel( true );
	for( list<string>::const_iterator i=sl.begin(); i!=sl.end(); ++i )
	    l.push_back(commitAction(SUBVOL, cont->type(),
				     createSubvolText(false,*i), this, false));
	}
    }

Text
Btrfs::extendText(bool doing, const string& dev) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, 
	// %1$s and %2$s are replaced by a device names (e.g. /dev/hda1)
        txt = sformat( _("Extending BTRFS volume %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    else
        {
        // displayed text before action, 
	// %1$s and %2$s are replaced by a device names (e.g. /dev/hda1)
        txt = sformat( _("Extend BTRFS volume %1$s by %2$s"), name().c_str(),
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
        txt = sformat( _("Reducing BTRFS volume %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    else
        {
        // displayed text before action, 
	// %1$s and %2$s are replaced by a device names (e.g. /dev/hda1)
        txt = sformat( _("Reduce BTRFS volume %1$s by %2$s"), name().c_str(),
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

void Btrfs::getInfo( BtrfsInfo& tinfo ) const
    {
    Volume::getInfo(info.v);
    info.devices = boost::join( devices, "\n" );
    info.devices_add = boost::join( dev_add, "\n" );
    info.devices_rem = boost::join( dev_rem, "\n" );
    info.subvol.erase();

    for( list<Subvolume>::const_iterator i=subvol.begin(); 
	 i!=subvol.end(); ++i )
	 {
	 if( !info.subvol.empty() )
	    info.subvol += '\n';
	 if( !i->deleted() )
	    info.subvol += i->path();
	 }
    tinfo = info;
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
    list<string>::const_iterator i;
    string tmp;
    for( i = devices.begin(); i != devices.end(); ++i)
	if (!contains(rhs.devices, *i))
	    tmp += *i + "-->";
    for( i = rhs.devices.begin(); i != rhs.devices.end(); ++i)
	if (!contains(devices, *i))
	    tmp += "<--" + *i;
    if (!tmp.empty())
	log << " Devices:" << tmp;

    tmp.erase();
    for( i = dev_add.begin(); i != dev_add.end(); ++i)
	if (!contains(rhs.dev_add, *i))
	    tmp += *i + "-->";
    for( i = rhs.dev_add.begin(); i != rhs.dev_add.end(); ++i)
	if (!contains(dev_add, *i))
	    tmp += "<--" + *i;
    if (!tmp.empty())
	log << " DevAdd:" << tmp;

    tmp.erase();
    for( i = dev_rem.begin(); i != dev_rem.end(); ++i)
	if (!contains(rhs.dev_rem, *i))
	    tmp += *i + "-->";
    for( i = rhs.dev_rem.begin(); i != rhs.dev_rem.end(); ++i)
	if (!contains(dev_rem, *i))
	    tmp += "<--" + *i;
    if (!tmp.empty())
	log << " DevRem:" << tmp;

    tmp.erase();
    list<Subvolume>::const_iterator s;
    for( s=subvol.begin(); s!=subvol.end(); ++s )
	{
	if( s->deleted() )
	    tmp += "<--" + s->path();
	else if( s->created() )
	    tmp += s->path() + "-->";
	}
    if (!tmp.empty())
	log << " SubVol:" << tmp;
    }

void Btrfs::saveData(xmlNode* node) const
    {
    Volume::saveData(node);
    setChildValue(node, "devices", devices);
    if( !dev_add.empty() )
	setChildValue(node, "dev_add", dev_add);
    if( !dev_rem.empty() )
	setChildValue(node, "dev_rem", dev_rem);
    if (!subvol.empty())
	setChildValue(node, "subvolume", subvol);
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
