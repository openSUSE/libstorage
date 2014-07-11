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


#include <ostream>
#include <sstream>

#include "storage/BtrfsCo.h"
#include "storage/Btrfs.h"
#include "storage/SystemCmd.h"
#include "storage/Dm.h"
#include "storage/SystemInfo/SystemInfo.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    BtrfsCo::BtrfsCo(Storage* s)
	: Container(s, "btrfs", "/dev/btrfs", staticType())
    {
	y2deb("constructing BtrfsCo");
    }


    BtrfsCo::BtrfsCo(Storage* s, SystemInfo& systeminfo)
	: Container(s, "btrfs", "/dev/btrfs", staticType(), systeminfo)
    {
	y2deb("constructing BtrfsCo");
	getBtrfsData(systeminfo);
    }


    BtrfsCo::BtrfsCo(const BtrfsCo& c)
	: Container(c)
    {
	y2deb("copy-constructed BtrfsCo from " << c.dev);

	for (auto const &i : c.btrfsPair())
	    vols.push_back(new Btrfs(*this, i));
    }


    BtrfsCo::~BtrfsCo()
    {
	y2deb("destructed BtrfsCo " << dev);
    }


void BtrfsCo::getBtrfsData(SystemInfo& systeminfo)
    {
    y2mil("begin");
    list<string> uuids = systeminfo.getCmdBtrfsShow().getUuids();
    CmdBtrfsShow::Entry e;
    for (string const &it : uuids)
	{
	if (systeminfo.getCmdBtrfsShow().getEntry(it, e))
	    {
	    Volume const* cv = NULL;
	    unsigned long long sum_size = 0;
	    list<string> an;
	    bool forbidden = false;
	    for (string const &d : e.devices)
		{
		Volume const* v;
		if (getStorage()->findVolume(d, v))
		    {
		    list<UsedBy> ub = v->getUsedBy();
		    for (auto const &it : ub)
			{
			forbidden = it.type() != UB_BTRFS;
			if( forbidden )
			    y2mil( "forbidden:" << v->device() << 
				   " used by non-btrfs:" << ub );
			}
		    if( !forbidden )
			{
			forbidden = v->getFs()!=BTRFS;
			if( forbidden )
			    y2mil( "forbidden:" << v->device() << 
				   " non-btrfs:" << v->fsTypeString() );
			}
		    if( !ub.empty() )
			y2mil( "used_by:" << ub );
		    if( cv==NULL || 
			(cv->getMount().empty() && !v->getMount().empty()) ||
			(!cv->getFormat() && v->getFormat() ))
			cv = v;
		    sum_size += v->sizeK();
		    list<string> li = v->altNames();
		    an.splice( an.end(), li );
		    }
		else
		    y2war("device " << d << " not found");
		}
	    list<string> devs;
	    if( forbidden )
		cv = NULL;
	    else
		{
		for (string const &it : e.devices)
		    {
		    const Device* v;
		    if (getStorage()->findDevice(it, v))
			devs.push_back( v->device() );
		    else
			devs.push_back(it);
		    }
		}
	    y2mil( "forbidden:" << forbidden << " cv:" << cv << 
	           " sum:" << sum_size << " devs:" << devs );
	    if( cv!=NULL )
		{
		Btrfs* b = new Btrfs( *this, *cv, sum_size, devs );
		y2mil( "alt_names:" << an );
		b->setAltNames( an );
		vols.push_back(b);
		}
	    }
	else
	    y2war("uuid " << it << " not found");
	}
    for (auto &i : btrfsPair())
        i.detectSubvol();
    y2mil("end");
    }

string BtrfsCo::fakeUuid()
    {
    string ret;
    unsigned long long uuid = 12345;
    BtrfsIter i;
    while( findBtrfs(decString(uuid),i) )
	++uuid;
    ret = decString(uuid);
    y2mil( "ret:" << ret );
    return(ret);
    }

void
BtrfsCo::addFromVolume( const Volume& v, string &uuid )
    {
    Btrfs* b = new Btrfs( *this, v );
    uuid = fakeUuid();
    b->initUuid( uuid );
    vols.push_back(b);
    }

bool BtrfsCo::existSubvolume( const string& device, const string& name )
    {
    bool ret = false;
    y2mil( "device:" << device << " name:" << name );
    BtrfsIter i;
    if( findBtrfs( device, i ))
	ret = i->existSubvolume( name );
    y2mil( "ret:" << ret );
    return( ret );
    }

int BtrfsCo::createSubvolume( const string& device, const string& name )
    {
    int ret = 0;
    y2mil( "device:" << device << " name:" << name );
    BtrfsIter i;
    if( readonly() )
	ret = BTRFS_CHANGE_READONLY;
    else if( findBtrfs( device, i ))
	ret = i->createSubvolume( name );
    else
	ret = BTRFS_VOLUME_NOT_FOUND;
    y2mil( "ret:" << ret );
    return( ret );
    }

int BtrfsCo::removeSubvolume( const string& device, const string& name )
    {
    int ret = 0;
    y2mil( "device:" << device << " name:" << name );
    BtrfsIter i;
    if( readonly() )
	ret = BTRFS_CHANGE_READONLY;
    else if( findBtrfs( device, i ))
	ret = i->deleteSubvolume( name );
    else
	ret = BTRFS_VOLUME_NOT_FOUND;
    y2mil( "ret:" << ret );
    return( ret );
    }

int BtrfsCo::extendVolume( const string& device, const string& dev )
    {
    list<string> d;
    d.push_back(dev);
    return( extendVolume(device,d));
    }

int BtrfsCo::extendVolume( const string& device, const list<string>& devs )
    {
    int ret = 0;
    y2mil( "device:" << device << " devices:" << devs );
    BtrfsIter i;
    if( readonly() )
	ret = BTRFS_CHANGE_READONLY;
    else if( findBtrfs( device, i ))
	ret = i->extendVolume( devs );
    else
	ret = BTRFS_VOLUME_NOT_FOUND;
    y2mil( "ret:" << ret );
    return( ret );
    }

int BtrfsCo::shrinkVolume( const string& device, const string& dev )
    {
    list<string> d;
    d.push_back(dev);
    return( shrinkVolume(device,d));
    }

int BtrfsCo::shrinkVolume( const string& device, const list<string>& devs )
    {
    int ret = 0;
    y2mil( "device:" << device << " devs:" << devs );
    BtrfsIter i;
    if( readonly() )
	ret = BTRFS_CHANGE_READONLY;
    else if( findBtrfs( device, i ))
	ret = i->shrinkVolume( devs );
    else
	ret = BTRFS_VOLUME_NOT_FOUND;
    y2mil( "ret:" << ret );
    return( ret );
    }

void
BtrfsCo::eraseVolume( Volume* v )
    {
    BtrfsPair p=btrfsPair(Btrfs::notDeleted);
    BtrfsIter i = p.begin();
    while( i!=p.end() && i->device()!=v->device() )
	++i;
    if( i!=p.end() )
	removeFromList( v );
    }

bool
BtrfsCo::findBtrfs( const string& id, BtrfsIter& i )
    {
    BtrfsPair p=btrfsPair(Btrfs::notDeleted);
    string uuid( id );
    if( boost::starts_with( uuid, "UUID=" ))
	uuid = uuid.substr( 5 );
    i=p.begin();
    while( i!=p.end() && i->getUuid()!=uuid )
	++i;
    if( i==p.end() && !p.empty() )
	{
	i=p.begin();
	bool found = false;
	while( i!=p.end() && !found )
	    {
	    found = i->device()==id;
	    if( !found )
		{
		const list<string>& al( i->altNames() );
		found = find( al.begin(), al.end(), id )!=al.end();
		}
	    if( !found )
		++i;
	    }
	}
    y2mil( "id:" << id << " ret:" << (i!=p.end()) );
    return( i!=p.end() );
    }

bool BtrfsCo::deviceToUuid( const string& device, string& uuid )
    {
    bool ret = false;
    y2mil( "device:" << device );
    const Volume* v = getStorage()->getVolume( device );
    uuid.clear();
    if (v)
	for (auto const &ul : v->getUsedBy())
	{
	if (ul.type() == UB_BTRFS)
	    {
	    uuid = v->getUsedBy().front().device();
	    ret = true;
	    }
	}
    y2mil( "ret:" << ret << " uuid:" << (ret?uuid:"") );
    return( ret );
    }

int BtrfsCo::commitChanges( CommitStage stage, Volume* vol )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = 0;
    if( stage==DECREASE )
	{
	Btrfs * b = dynamic_cast<Btrfs *>(vol);
	if( b!=NULL )
	    {
	    if( Btrfs::needReduce(*b) )
		ret = b->doReduce();
	    }
	else
	    ret = BTRFS_COMMIT_INVALID_VOLUME;
	}
    if( ret==0 )
	ret = Container::commitChanges( stage, vol );
    if( ret==0 && stage==INCREASE )
	{
	Btrfs * b = dynamic_cast<Btrfs *>(vol);
	if( b!=NULL )
	    {
	    if( Btrfs::needExtend(*b) )
		ret = b->doExtend();
	    }
	else
	    ret = BTRFS_COMMIT_INVALID_VOLUME;
	}
    else if( ret==0 && stage==SUBVOL )
	{
	Btrfs * b = dynamic_cast<Btrfs *>(vol);
	if( b!=NULL )
	    {
	    if( Btrfs::needDeleteSubvol( *b ) )
		ret = b->doDeleteSubvol();
	    else if( Btrfs::needCreateSubvol( *b ) )
		ret = b->doCreateSubvol();
	    }
	else
	    ret = BTRFS_COMMIT_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

void BtrfsCo::getToCommit( storage::CommitStage stage, list<const Container*>& col,
                           list<const Volume*>& vol ) const
    {
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    Container::getToCommit( stage, col, vol );
    if (stage == DECREASE)
	{
	for (auto const &i : btrfsPair(Btrfs::needReduce))
	    if (find(vol.begin(), vol.end(), &i) == vol.end())
		vol.push_back(&i);
	}
    else if (stage == INCREASE)
	{
	for (auto const &i : btrfsPair(Btrfs::needExtend))
	    if (find(vol.begin(), vol.end(), &i) == vol.end())
		vol.push_back(&i);
	}
    else if (stage == SUBVOL)
	{
	for (auto const &i : btrfsPair(Btrfs::needDeleteSubvol))
	    if (find(vol.begin(), vol.end(), &i) == vol.end())
		vol.push_back(&i);
	for (auto const &i : btrfsPair(Btrfs::needCreateSubvol))
	    if (find(vol.begin(), vol.end(), &i) == vol.end())
		vol.push_back(&i);
	}
    if( col.size()!=oco || vol.size()!=ovo )
	y2mil("stage:" << stage << " col:" << col.size() << " vol:" << vol.size());
    }


int BtrfsCo::removeUuid( const string& uuid )
    {
    int ret = 0;
    y2mil( "uuid:" << uuid );
    BtrfsIter b;
    if( findBtrfs( uuid, b ) )
	ret = removeVolume( &(*b), true );
    else
	ret = BTRFS_REMOVE_NOT_FOUND;
    y2mil( "ret:" << ret );
    return( ret );
    }

int BtrfsCo::removeVolume( Volume* v, bool quiet )
    {
    int ret = 0;
    y2mil("qiet:" << quiet);
    y2mil("vol:" << *v);
    v->setDeleted();
    if( quiet )
	v->setSilent();
    Btrfs * b = dynamic_cast<Btrfs *>(v);
    if( b )
	b->unuseDev();
    else
	y2err( "no btrfs volume:" << *v );
    y2mil("ret:" << ret);
    return( ret );
    }

int BtrfsCo::removeVolume( Volume* v )
    {
    return( removeVolume( v, false ));
    }

int
BtrfsCo::doRemove( Volume* v )
    {
    int ret = 0;
    Btrfs *b = dynamic_cast<Btrfs *>(v);
    if( b != NULL )
	{
	getStorage()->showInfoCb( b->removeText(true), b->isSilent() );
	ret = b->prepareRemove();
	if( ret==0 )
	    ret = b->clearSignature();
	if( ret==0 && !removeFromList(v) )
	    ret = BTRFS_REMOVE_NO_BTRFS;
	}
    else
	ret = BTRFS_REMOVE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }


    void
    BtrfsCo::changeDeviceName( const string& old, const string& nw )
    {
        for (auto &i : btrfsPair())
            i.changeDeviceName(old, nw);
    }

int BtrfsCo::resizeVolume( Volume* v, Container* r_co,
                           Volume* r_v, unsigned long long newSize )
    {
    int ret = 0;
    Btrfs *b = dynamic_cast<Btrfs *>(v);
    if( b != NULL )
	{
	long long size_diff = newSize - b->sizeK();
	y2mil( "size_diff:" << size_diff );
	if( b->getDevices().size()==1 || size_diff>0 )
	    {
	    y2mil( "r_v:" << *r_v );
	    ret = r_co->resizeVolume( r_v, r_v->sizeK()+size_diff );
	    y2mil( "r_v:" << *r_v );
	    }
	else if( size_diff<0 )
	    {
	    ret = BTRFS_MULTIDEV_SHRINK_UNSUPPORTED;
	    }
	y2mil( "ret:" << ret << " size_diff:" << size_diff );
	if( ret==0 )
	    {
	    v->setSilent();
	    v->setResizedSize( v->sizeK()+size_diff );
	    }
	y2mil( "size_diff:" << size_diff );
	y2mil( "this:" << *this );
	}
    else
	ret = BTRFS_RESIZE_INVALID_VOLUME;
    return( ret );
    }

void
BtrfsCo::logData(const string& Dir) const
    {
    string fname(Dir + "/btrfs.info.tmp");

    XmlFile xml;
    xmlNode* node = xmlNewNode("btrfs");
    xml.setRootElement(node);
    saveData(node);
    xml.save(fname);

    getStorage()->handleLogFile( fname );
    }

void
BtrfsCo::saveData(xmlNode* node) const
    {
    Container::saveData(node);
    for (auto const &v : btrfsPair())
        v.saveData(xmlNewChild(node, "btrfs"));
    }


std::ostream& operator<<(std::ostream& s, const BtrfsCo& d)
    {
    s << dynamic_cast<const Container&>(d);
    return( s );
    }


void BtrfsCo::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
    const BtrfsCo& rhs = dynamic_cast<const BtrfsCo&>(rhs_c);

    logDifference(log, rhs);
    log << endl;

    ConstBtrfsPair pp = btrfsPair();
    ConstBtrfsPair pc = rhs.btrfsPair();
    logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


bool BtrfsCo::equalContent( const Container& rhs ) const
    {
    const BtrfsCo * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const BtrfsCo*>(&rhs);
    if( ret && p )
	{
	ConstBtrfsPair pp = btrfsPair();
	ConstBtrfsPair pc = p->btrfsPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
	}
    return( ret );
    }

}
