/*
 * Copyright (c) [2004-2009] Novell, Inc.
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

#include "storage/LoopCo.h"
#include "storage/Loop.h"
#include "storage/SystemCmd.h"
#include "storage/Dm.h"
#include "storage/SystemInfo.h"
#include "storage/ProcMounts.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/EtcFstab.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    LoopCo::LoopCo(Storage* s)
	: Container(s, "loop", "/dev/loop", staticType())
    {
	y2deb("constructing LoopCo");
    }


    LoopCo::LoopCo(Storage* s, SystemInfo& systeminfo)
	: Container(s, "loop", "/dev/loop", staticType(), systeminfo)
    {
	y2deb("constructing LoopCo");
	getLoopData(systeminfo);
    }


    LoopCo::LoopCo(const LoopCo& c)
	: Container(c)
    {
	y2deb("copy-constructed LoopCo from " << c.dev);

	ConstLoopPair p = c.loopPair();
	for (ConstLoopIter i = p.begin(); i != p.end(); ++i)
	{
	    Loop* p = new Loop(*this, *i);
	    vols.push_back(p);
	}
    }


    LoopCo::~LoopCo()
    {
	y2deb("destructed LoopCo " << dev);
    }


void
    LoopCo::getLoopData(SystemInfo& systeminfo)
    {
    y2mil("begin");
    list<FstabEntry> l;
    const EtcFstab* fstab = getStorage()->getFstab();
    fstab->getFileBasedLoops( getStorage()->root(), l );
    if( !l.empty() )
	{
	SystemCmd c(LOSETUPBIN " -a");
	for( list<FstabEntry>::const_iterator i=l.begin(); i!=l.end(); ++i )
	    {
	    y2mil( "i:" << *i );
	    string lfile = getStorage()->root() + i->device;
	    if( findLoop( i->dentry ))
		y2war("duplicate loop file " << i->dentry);
	    else if( !i->loop_dev.empty() && Volume::loopInUse( getStorage(),
								i->loop_dev ) )
		y2war("duplicate loop_device " << i->loop_dev);
	    else if( !checkNormalFile( lfile ))
		y2war("not existent or special file " << lfile);
	    else
		{
		Loop *l = new Loop( *this, i->loop_dev, lfile,
				    i->dmcrypt, !i->noauto?i->dentry:"",
				    systeminfo, c);
		l->setEncryption( i->encr );
		l->setFs(toValueWithFallback(i->fs, FSUNKNOWN));
		y2mil( "l:" << *l );
		addToList( l );
		}
	    }
	LoopPair p=loopPair(Loop::notDeleted);
	LoopIter i=p.begin();
	map<string, string> mp = systeminfo.getProcMounts().allMounts();
	while( i!=p.end() )
	    {
	    if( i->dmcrypt() )
		{
		y2mil( "i:" << *i );
		if( i->dmcryptDevice().empty() )
		    {
		    const Dm* dm = 0;
		    if( !i->loopDevice().empty() )
			{
			getStorage()->findDmUsing( i->loopDevice(), dm );
			}
		    if( dm==0 && !i->getMount().empty() &&
			!mp[i->getMount()].empty() )
			{
			y2mil( "mp:" << i->getMount() << " dev:" <<
			       mp[i->getMount()] );
			getStorage()->findDm( mp[i->getMount()], dm );
			}
		    if( dm )
			{
			i->setDmcryptDev( dm->device() );
			i->setSize( dm->sizeK() );
			}
		    y2mil( "i:" << *i );
		    }
		getStorage()->removeDm( i->dmcryptDevice() );
		}
	    ++i;
	    }
	}
    }


    list<unsigned>
    LoopCo::usedNumbers() const
    {
	list<unsigned> nums;

	ConstLoopPair p = loopPair(Loop::notDeleted);
	for (ConstLoopIter i = p.begin(); i != p.end(); ++i)
	    nums.push_back(i->nr());

	return nums;
    }


bool
LoopCo::findLoop( unsigned num, LoopIter& i )
    {
    LoopPair p=loopPair(Loop::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->nr()!=num )
	++i;
    return( i!=p.end() );
    }

bool
LoopCo::findLoop( unsigned num )
    {
    LoopIter i;
    return( findLoop( num, i ));
    }

bool
LoopCo::findLoop( const string& file, LoopIter& i )
    {
    LoopPair p=loopPair(Loop::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->loopFile()!=file )
	++i;
    return( i!=p.end() );
    }

bool
LoopCo::findLoop( const string& file )
    {
    LoopIter i;
    return( findLoop( file, i ));
    }

bool
LoopCo::findLoopDev( const string& dev, LoopIter& i )
    {
    LoopPair p=loopPair(Loop::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->device()!=dev )
	++i;
    return( i!=p.end() );
    }

int
LoopCo::createLoop( const string& file, bool reuseExisting,
                    unsigned long long sizeK, bool dmcr, string& device )
    {
    int ret = 0;
    y2mil("file:" << file << " reuse:" << reuseExisting << " sizeK:" <<
	  sizeK << " dmcr:" << dmcr);
    if( readonly() )
	{
	ret = LOOP_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( findLoop( file ))
	    ret = LOOP_DUPLICATE_FILE;
	}
    if( ret==0 )
	{
	Loop* l = new Loop( *this, file, reuseExisting, sizeK, dmcr );
	l->setCreated( true );
	addToList( l );
	device = l->device();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LoopCo::updateLoop( const string& device, const string& file,
                    bool reuseExisting, unsigned long long sizeK )
    {
    int ret = 0;
    y2mil("device:" << device << " reuse:" << reuseExisting << " sizeK:" <<
	  sizeK);
    LoopIter i;
    if( readonly() )
	{
	ret = LOOP_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findLoopDev( device, i ))
	    ret = LOOP_UNKNOWN_FILE;
	}
    if( ret==0 && !i->created() )
	{
	ret = LOOP_MODIFY_EXISTING;
	}
    if( ret==0 )
	{
	i->setLoopFile( file );
	i->setReuse( reuseExisting );
	if( !i->getReuse() )
	    i->setSize( sizeK );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LoopCo::removeLoop( const string& file, bool removeFile )
    {
    int ret = 0;
    y2mil("file:" << file << " removeFile:" << removeFile);
    LoopIter i;
    if( readonly() )
	{
	ret = LOOP_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findLoop( file, i ) && !findLoopDev( file, i ) )
	    ret = LOOP_UNKNOWN_FILE;
	}
    if (ret == 0 && i->isUsedBy())
	{
	ret = LOOP_REMOVE_USED_BY;
	}
    if( ret==0 )
	{
	if( i->created() )
	    {
	    if( !removeFromList( &(*i) ))
		ret = LOOP_REMOVE_CREATE_NOT_FOUND;
	    }
	else
	    {
	    i->setDeleted();
	    i->setDelFile( removeFile );
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int LoopCo::removeVolume( Volume* v )
    {
    int ret = 0;
    y2mil("name:" << v->name());
    Loop * l = dynamic_cast<Loop *>(v);
    if( l != NULL )
	ret = removeLoop( l->loopFile(), false );
    else
	ret = LOOP_REMOVE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }

int
LoopCo::doCreate( Volume* v )
    {
    y2mil("name:" << v->name());
    Loop * l = dynamic_cast<Loop *>(v);
    int ret = 0;
    if( l != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->createText(true) );
	    }
	if( !l->createFile() )
	    ret = LOOP_FILE_CREATE_FAILED;
	if( ret==0 )
	    {
	    ret = l->doCrsetup();
	    }
	if( ret==0 )
	    {
	    l->setCreated( false );
	    }
	}
    else
	ret = LOOP_CREATE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }

int
LoopCo::doRemove( Volume* v )
    {
    y2mil("name:" << v->name());
    Loop * l = dynamic_cast<Loop *>(v);
    int ret = 0;
    if( l != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->removeText(true) );
	    }
	ret = l->prepareRemove();
	if( ret==0 )
	    {
	    if( !l->removeFile() )
		ret = LOOP_REMOVE_FILE_FAILED;
	    if( !removeFromList( l ) && ret==0 )
		ret = LOOP_NOT_IN_LIST;
	    }
	}
    else
	ret = LOOP_REMOVE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }


    std::ostream& operator<<(std::ostream& s, const LoopCo& d)
    {
    s << dynamic_cast<const Container&>(d);
    return( s );
    }


    void
    LoopCo::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
	const LoopCo& rhs = dynamic_cast<const LoopCo&>(rhs_c);

	logDifference(log, rhs);
	log << endl;

	ConstLoopPair pp = loopPair();
	ConstLoopPair pc = rhs.loopPair();
	logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


bool LoopCo::equalContent( const Container& rhs ) const
    {
    const LoopCo * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const LoopCo*>(&rhs);
    if( ret && p )
	{
	ConstLoopPair pp = loopPair();
	ConstLoopPair pc = p->loopPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
	}
    return( ret );
    }


}
