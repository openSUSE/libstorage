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
    setSize( sz );
    }

Btrfs::Btrfs(const BtrfsCo& d, const Volume& v ) : Volume(d, v)
    {
    y2mil("constructed btrfs vol from:" << v );
    y2mil( "fs:" << fs << " det:" << detected_fs );
    devices.push_back(v.device());
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

int Btrfs::doDeleteSubvol()
    {
    int ret = 0;
    bool needUmount = false;
    Storage* st = NULL;
    string m = getMount();
    if( !silent )
	getStorage()->showInfoCb( deleteSubvolText(true) );
    if( !isMounted() )
	{
	st = getContainer()->getStorage();
	if( st->mountTmp( this, m ) )
	    needUmount = true;
	else
	    ret = BTRFS_CANNOT_TMP_MOUNT;
	}
    if( ret==0 )
	{
	SystemCmd c;
	string cmd = BTRFSBIN " subvolume delete " + m + '/';
	for( list<Subvolume>::iterator i=subvol.begin(); i!=subvol.end(); ++i )
	    {
	    if( i->deleted() )
		{
		c.execute( cmd + i->path() );
		if( c.retcode()==0 )
		    i->setDeleted(false);
		else
		    ret = BTRFS_DELETE_SUBVOL_FAIL;
		}
	    }
	}
    if( needUmount )
	{
	if( !st->umountDev( device() ) && ret==0 )
	    {
	    ret = BTRFS_CANNOT_TMP_UMOUNT;
	    }
	}
    y2mil( "ret:" << ret );
    return( ret );
    }

int Btrfs::doCreateSubvol()
    {
    int ret = 0;
    bool needUmount = false;
    Storage* st = NULL;
    string m = getMount();
    if( !silent )
	getStorage()->showInfoCb( createSubvolText(true) );
    if( !isMounted() )
	{
	st = getContainer()->getStorage();
	if( st->mountTmp( this, m ) )
	    needUmount = true;
	else
	    ret = BTRFS_CANNOT_TMP_MOUNT;
	}
    if( ret==0 )
	{
	SystemCmd c;
	string cmd = BTRFSBIN " subvolume create " + m + '/';
	for( list<Subvolume>::iterator i=subvol.begin(); i!=subvol.end(); ++i )
	    {
	    if( i->created() )
		{
		c.execute( cmd + i->path() );
		if( c.retcode()==0 )
		    i->setCreated(false);
		else
		    ret = BTRFS_CREATE_SUBVOL_FAIL;
		}
	    }
	}
    if( needUmount )
	{
	if( !st->umountDev( device() ) && ret==0 )
	    {
	    ret = BTRFS_CANNOT_TMP_UMOUNT;
	    }
	}
    y2mil( "ret:" << ret );
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

void
Btrfs::getCommitActions(list<commitAction>& l) const
    {
    Volume::getCommitActions( l );
    unsigned rem, add;
    countSubvolAddDel( add, rem );
    if( rem>0 )
	{
	l.push_back(commitAction(SUBVOL, cont->staticType(),
				 deleteSubvolText(false), this, true));
	}
    if( add>0 )
	{
	l.push_back(commitAction(SUBVOL, cont->staticType(),
				 createSubvolText(false), this, true));
	}
    }

Text Btrfs::createSubvolText(bool doing) const
    {
    Text txt;
    unsigned cnt, dummy;
    countSubvolAddDel( cnt, dummy );
    string vols = subvolNames( true );
    if( doing )
	{
	if( cnt<=1 )
	    // displayed text during action, %1$s is replaced by subvolume name e.g. tmp
	    // %2$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Creating subvolume %1$s on device %2$s"), 
	                   vols.c_str(), dev.c_str() );
	else
	    // displayed text during action, %1$s is replaced by subvolume names e.g. "tmp var/tmp var/log"
	    // %2$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Creating subvolumes %1$s on device %2$s"), 
	                   vols.c_str(), dev.c_str() );
	}
    else
	{
	if( cnt<=1 )
	    // displayed text before action, %1$s is replaced by subvolume name e.g. tmp
	    // %2$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Create subvolume %1$s on device %2$s"), 
	                   vols.c_str(), dev.c_str() );
	else
	    // displayed text before action, %1$s is replaced by subvolume names e.g. "tmp var/tmp var/log"
	    // %2$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Create subvolumes %1$s on device %2$s"), 
	                   vols.c_str(), dev.c_str() );
	}
    return( txt );
    }

Text Btrfs::deleteSubvolText(bool doing) const
    {
    Text txt;
    unsigned cnt, dummy;
    countSubvolAddDel( dummy, cnt );
    string vols = subvolNames(false);
    if( doing )
	{
	if( cnt<=1 )
	    // displayed text during action, %1$s is replaced by subvolume name e.g. tmp
	    // %2$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Removing subvolume %1$s on device %2$s"), 
	                   vols.c_str(), dev.c_str() );
	else
	    // displayed text during action, %1$s is replaced by subvolume names e.g. "tmp var/tmp var/log"
	    // %2$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Removing subvolumes %1$s on device %2$s"), 
	                   vols.c_str(), dev.c_str() );
	}
    else
	{
	if( cnt<=1 )
	    // displayed text before action, %1$s is replaced by subvolume name e.g. tmp
	    // %2$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Remove subvolume %1$s on device %2$s"), 
	                   vols.c_str(), dev.c_str() );
	else
	    // displayed text before action, %1$s is replaced by subvolume names e.g. "tmp var/tmp var/log"
	    // %2$s is replaced by device name e.g. /dev/hda1
	    txt = sformat( _("Remove subvolumes %1$s on device %2$s"), 
	                   vols.c_str(), dev.c_str() );
	}
    return( txt );
    }

void Btrfs::getInfo( BtrfsInfo& tinfo ) const
    {
    Volume::getInfo(info.v);
    info.devices = boost::join( devices, "\n" );
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
    if( !v.subvol.empty() )
	s << " subvol:" << v.subvol;
    return( s );
    }


bool Btrfs::equalContent( const Btrfs& rhs ) const
    {
    return( Volume::equalContent(rhs) && devices==rhs.devices &&
            subvol==rhs.subvol );
    }


void Btrfs::logDifference(std::ostream& log, const Btrfs& rhs) const
    {
    Volume::logDifference(log, rhs);
    }

void Btrfs::saveData(xmlNode* node) const
    {
    Volume::saveData(node);
    setChildValue(node, "devices", devices);
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

}
