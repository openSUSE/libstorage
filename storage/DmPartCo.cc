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

#include "storage/DmPartCo.h"
#include "storage/DmPart.h"
#include "storage/ProcParts.h"
#include "storage/Partition.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"
#include "storage/SystemInfo.h"


namespace storage
{
    using namespace std;


    DmPartCo::DmPartCo(Storage* s, const string& name, const string& device, CType t,
		       SystemInfo& systeminfo)
	: PeContainer(s, name, device, t, systeminfo), disk(NULL), active(false)
    {
    y2deb("constructing DmPartCo name:" << name);
	getMajorMinor();
	init(systeminfo);
    }


DmPartCo::~DmPartCo()
    {
    if( disk )
	{
	delete disk;
	disk = NULL;
	}
    y2deb("destructed DmPartCo " << dev);
    }


    string
    DmPartCo::sysfsPath() const
    {
	return SYSFSDIR "/" + procName();
    }


int
DmPartCo::addNewDev(string& device)
{
    int ret = 0;
    y2mil("device:" << device << " dev:" << dev);
    string::size_type pos = device.rfind("_part");
    if (pos == string::npos)
	ret = DMPART_PARTITION_NOT_FOUND;
    else
    {
	unsigned number;
	device.substr(pos + 5) >> number;
	y2mil("num:" << number);
	device = getPartDevice(number);
	Partition *p = getPartition( number, false );
	if( p==NULL )
	    ret = DMPART_PARTITION_NOT_FOUND;
	else
	{
	    y2mil("*p:" << *p);
	    DmPart * dm = NULL;
	    newP( dm, p->nr(), p );
	    dm->getFsInfo( p );
	    dm->setCreated();
	    dm->addUdevData();
	    addToList( dm );
	}
	handleWholeDevice();
    }
    y2mil("device:" << device << " ret:" << ret);
    return ret;
}


int 
DmPartCo::createPartition( storage::PartitionType type, long unsigned start,
			   long unsigned len, string& device, bool checkRelaxed )
    {
    y2mil("begin type:" << toString(type) << " start:" << start << " len:" << len <<
	  " relaxed:" << checkRelaxed);
    int ret = disk ? 0 : DMPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
	ret = DMPART_CHANGE_READONLY;
    if( ret==0 )
	ret = disk->createPartition( type, start, len, device, checkRelaxed );
    if( ret==0 )
	ret = addNewDev( device );
    y2mil("ret:" << ret);
    return( ret );
    }

int 
DmPartCo::createPartition( long unsigned len, string& device, bool checkRelaxed )
    {
    y2mil("len:" << len << " relaxed:" << checkRelaxed);
    int ret = disk ? 0 : DMPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
	ret = DMPART_CHANGE_READONLY;
    if( ret==0 )
	ret = disk->createPartition( len, device, checkRelaxed );
    if( ret==0 )
	ret = addNewDev( device );
    y2mil("ret:" << ret);
    return( ret );
    }

int 
DmPartCo::createPartition( storage::PartitionType type, string& device )
    {
    y2mil("type:" << toString(type));
    int ret = disk ? 0 : DMPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
	ret = DMPART_CHANGE_READONLY;
    if( ret==0 )
	ret = disk->createPartition( type, device );
    if( ret==0 )
	ret = addNewDev( device );
    y2mil("ret:" << ret);
    return( ret );
    }

int DmPartCo::updateDelDev()
    {
    int ret = 0;
    list<Volume*> l;
    DmPartPair p=dmpartPair();
    DmPartIter i=p.begin();
    while( i!=p.end() )
	{
	Partition *p = i->getPtr();
	if( p && validPartition( p ) )
	    {
	    if( i->nr()!=p->nr() )
		{
		i->updateName();
		y2mil( "updated name dm:" << *i );
		}
	    if( i->deleted() != p->deleted() )
		{
		i->setDeleted( p->deleted() );
		y2mil( "updated del dm:" << *i );
		}
	    }
	else
	    l.push_back( &(*i) );
	++i;
	}
    list<Volume*>::iterator vi = l.begin();
    while( ret==0 && vi!=l.end() )
	{
	if( !removeFromList( *vi ))
	    ret = DMPART_PARTITION_NOT_FOUND;
	++vi;
	}
    handleWholeDevice();
    return( ret );
    }

int 
DmPartCo::removePartition( unsigned nr )
    {
    int ret = disk ? 0 : DMPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
	ret = DMPART_CHANGE_READONLY;
    if( ret==0 )
	{
	if( nr>0 )
	    ret = disk->removePartition( nr );
	else
	    ret = DMPART_PARTITION_NOT_FOUND;
	}
    if( ret==0 )
	ret = updateDelDev();
    y2mil("ret:" << ret);
    return( ret );
    }

int
DmPartCo::removeVolume( Volume* v )
    {
    int ret = disk ? 0 : DMPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
	ret = DMPART_CHANGE_READONLY;
    if( ret==0 )
	{
	unsigned num = v->nr();
	if( num>0 )
	    ret = disk->removePartition( v->nr() );
	}
    if( ret==0 )
	ret = updateDelDev();
    getStorage()->logCo( this );
    y2mil("ret:" << ret);
    return( ret );
    }


int
DmPartCo::freeCylindersAroundPartition(const DmPart* dm, unsigned long& freeCylsBefore,
				       unsigned long& freeCylsAfter) const
{
    const Partition* p = dm->getPtr();
    int ret = p ? 0 : DMPART_PARTITION_NOT_FOUND;
    if (ret == 0)
    {
	ret = disk->freeCylindersAroundPartition(p, freeCylsBefore, freeCylsAfter);
    }
    y2mil("ret:" << ret);
    return ret;
}


int DmPartCo::resizePartition( DmPart* dm, unsigned long newCyl )
    {
    Partition * p = dm->getPtr();
    int ret = p?0:DMPART_PARTITION_NOT_FOUND;
    if( ret==0 )
	{
	p->getFsInfo( dm );
	ret = disk->resizePartition( p, newCyl );
	dm->updateSize();
	}
    y2mil( "dm:" << *dm );
    y2mil("ret:" << ret);
    return( ret );
    }

int
DmPartCo::resizeVolume( Volume* v, unsigned long long newSize )
    {
    int ret = disk ? 0 : DMPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
	ret = DMPART_CHANGE_READONLY;
    DmPart * l = dynamic_cast<DmPart *>(v);
    if( ret==0 && l==NULL )
	ret = DMPART_INVALID_VOLUME;
    if( ret==0 )
	{
	Partition *p = l->getPtr();
	unsigned num = v->nr();
	if( num>0 && p!=NULL )
	    {
	    p->getFsInfo( v );
	    ret = disk->resizeVolume( p, newSize );
	    }
	else
	    ret = DMPART_PARTITION_NOT_FOUND;
	}
    if( ret==0 )
	{
	l->updateSize();
	}
    y2mil("ret:" << ret);
    return( ret );
    }


void 
    DmPartCo::init(SystemInfo& systeminfo)
    {
	CmdDmsetup::Entry entry;
	if (systeminfo.getCmdDmsetup().getEntry(nm, entry) && entry.segments > 0)
	{
	systeminfo.getProcParts().getSize("/dev/dm-" + decString(entry.mnr), size_k);
	y2mil("minor:" << entry.mnr << " nm:" << nm);
	y2mil( "pe_size:" << pe_size << " size_k:" << size_k );
	if( size_k>0 )
	    pe_size = size_k;
	else
	    y2war("size_k zero for dm minor " << entry.mnr);
	num_pe = 1;
	createDisk(systeminfo);
	if( disk->numPartitions()>0 )
	    {
	    string pat = getPartName(1);
	    pat.erase( pat.length()-1, 1 );
	    string reg = "^" + pat + "[0-9]+" "$";
	    list<string> tmp = systeminfo.getCmdDmsetup().getMatchingEntries(regex_matches(reg));
	    if (tmp.empty())
		activate_part(true);
	    }
	getVolumes(systeminfo.getProcParts());
	active = true;
	}
    }


void
    DmPartCo::createDisk(SystemInfo& systeminfo)
    {
    if( disk )
	delete disk;
    disk = new Disk(getStorage(), nm, dev, size_k, systeminfo);
    disk->setNumMinor( 64 );
    disk->setSilent();
    disk->setSlave();
    disk->setAddpart(false);
    disk->detect(systeminfo);
    }


void 
DmPartCo::newP( DmPart*& dm, unsigned num, Partition* p )
    {
    y2mil( "num:" << num );
    dm = new DmPart( *this, getPartName(num), getPartDevice(num), num, p );
    }


void
    DmPartCo::getVolumes(const ProcParts& parts)
    {
    clearPointerList(vols);
    Disk::PartPair pp = disk->partPair();
    Disk::PartIter i = pp.begin();
    DmPart * p = NULL;
    while( i!=pp.end() )
	{
	newP( p, i->nr(), &(*i) );
	p->updateSize(parts);
	addToList( p );
	++i;
	}
    handleWholeDevice();
    }

void DmPartCo::handleWholeDevice()
    {
    Disk::PartPair pp = disk->partPair( Partition::notDeleted );
    y2mil("empty:" << pp.empty());
    if( pp.empty() )
	{
	DmPart * p = NULL;
	newP( p, 0, NULL );
	p->setSize( size_k );
	addToList( p );
	}
    else
	{
	DmPartIter i;
	if( findDm( 0, i ))
	    {
	    DmPart* dm = &(*i);
	    if( !removeFromList( dm ))
		y2err( "not found:" << *i );
	    }
	}
    }
    
Partition* 
DmPartCo::getPartition( unsigned nr, bool del )
    {
    Partition* ret = NULL;
    Disk::PartPair pp = disk->partPair();
    Disk::PartIter i = pp.begin();
    while( i!=pp.end() && (nr!=i->nr() || del!=i->deleted()) )
	++i;
    if( i!=pp.end() )
	ret = &(*i);
    if( ret )
	y2mil( "nr:" << nr << " del:" << del << " *p:" << *ret );
    else
	y2mil( "nr:" << nr << " del:" << del << " p:NULL" );
    return( ret );
    }

bool 
DmPartCo::validPartition( const Partition* p )
    {
    bool ret = false;
    Disk::PartPair pp = disk->partPair();
    Disk::PartIter i = pp.begin();
    while( i!=pp.end() && p != &(*i) )
	++i;
    ret = i!=pp.end();
    y2mil( "nr:" << p->nr() << " ret:" << ret );
    return( ret );
    }

void DmPartCo::updatePointers( bool invalid )
    {
    DmPartPair p=dmpartPair();
    DmPartIter i=p.begin();
    while( i!=p.end() )
	{
	if( invalid )
	    i->setPtr( getPartition( i->nr(), i->deleted() ));
	i->updateName();
	++i;
	}
    }

void DmPartCo::updateMinor()
    {
    DmPartPair p=dmpartPair();
    for (DmPartIter i = p.begin(); i != p.end(); ++i)
	i->updateMinor();
    }


    string
    DmPartCo::getPartName(unsigned num) const
    {
    string ret = nm;
    if( num>0 )
	{
	ret += "_part";
	ret += decString(num);
	}
    y2mil( "num:" << num << " ret:" << ret );
    return( ret );
    }


    string
    DmPartCo::getPartDevice(unsigned num) const
    {
    string ret = dev;
    if( num>0 )
	{
	ret += "_part";
	ret += decString(num);
	}
    y2mil( "num:" << num << " ret:" << ret );
    return( ret );
    }


string DmPartCo::undevName( const string& name )
    {
    string ret = name;
    if( ret.find( "/dev/" ) == 0 )
	ret.erase( 0, 5 );
    if( ret.find( "mapper/" ) == 0 )
	ret.erase( 0, 7 );
    return( ret );
    }

int DmPartCo::destroyPartitionTable( const string& new_label )
    {
    y2mil("begin");
    int ret = disk->destroyPartitionTable( new_label );
    if( ret==0 )
	{
	VIter j = vols.begin();
	while( j!=vols.end() )
	    {
	    if( (*j)->created() )
		{
		delete( *j );
		j = vols.erase( j );
		}
	    else
		++j;
	    }
	bool save = getStorage()->getRecursiveRemoval();
	getStorage()->setRecursiveRemoval(true);
	if (isUsedBy())
	    {
	    getStorage()->removeUsing( device(), getUsedBy() );
	    }
	ronly = false;
	RVIter i = vols.rbegin();
	while( i!=vols.rend() )
	    {
	    if( !(*i)->deleted() )
		getStorage()->removeVolume( (*i)->device() );
	    ++i;
	    }
	getStorage()->setRecursiveRemoval(save);
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int DmPartCo::changePartitionId( unsigned nr, unsigned id )
    {
    int ret = nr>0?0:DMPART_PARTITION_NOT_FOUND;
    if( ret==0 )
	{
	ret = disk->changePartitionId( nr, id );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int DmPartCo::forgetChangePartitionId( unsigned nr )
    {
    int ret = nr>0?0:DMPART_PARTITION_NOT_FOUND;
    if( ret==0 )
	{
	ret = disk->forgetChangePartitionId( nr );
	}
    y2mil("ret:" << ret);
    return( ret );
    }


int
DmPartCo::nextFreePartition(PartitionType type, unsigned& nr, string& device) const
{
    int ret = 0;
    device = "";
    nr = disk->availablePartNumber( type );
    if (nr == 0)
	{
	ret = DISK_PARTITION_NO_FREE_NUMBER;
	}
    else
	{
	device = getPartDevice(nr);
	}
    y2mil("ret:" << ret << " nr:" << nr << " device:" << device);
    return ret;
}


    int
    DmPartCo::changePartitionArea(unsigned nr, const Region& cylRegion, bool checkRelaxed)
    {
    int ret = nr>0?0:DMPART_PARTITION_NOT_FOUND;
    if( ret==0 )
	{
	ret = disk->changePartitionArea(nr, cylRegion, checkRelaxed);
	DmPartIter i;
	if( findDm( nr, i ))
	    i->updateSize();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

bool DmPartCo::findDm( unsigned nr, DmPartIter& i )
    {
    DmPartPair p = dmpartPair( DmPart::notDeleted );
    i=p.begin();
    while( i!=p.end() && i->nr()!=nr )
	++i;
    return( i!=p.end() );
    }

void DmPartCo::activate_part( bool val )
    {
	active = val;
    }

int DmPartCo::doSetType( DmPart* dm )
    {
    y2mil("doSetType container " << name() << " name " << dm->name());
    Partition * p = dm->getPtr();
    int ret = p?0:DMPART_PARTITION_NOT_FOUND;
    if( ret==0 )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( dm->setTypeText(true) );
	    }
	ret = disk->doSetType( p );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int DmPartCo::doCreateLabel()
    {
    y2mil("label:" << labelName());
    int ret = 0;
    if( !silent )
	{
	getStorage()->showInfoCb( setDiskLabelText(true) );
	}
    getStorage()->removeDmMapsTo( device() );
    removePresentPartitions();
    ret = disk->doCreateLabel();
    if( ret==0 )
	{
	removeFromMemory();
	handleWholeDevice();
	Storage::waitForDevice();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
DmPartCo::removeDmPart()
    {
    int ret = 0;
    y2mil("begin");
    if( readonly() )
	{
	ret = DMPART_CHANGE_READONLY;
	}
    if( ret==0 && !created() )
	{
	DmPartPair p=dmpartPair(DmPart::notDeleted);
	for( DmPartIter i=p.begin(); i!=p.end(); ++i )
	    {
	    if( i->nr()>0 )
		ret = removePartition( i->nr() );
	    }
	p=dmpartPair(DmPart::notDeleted);
	if( p.begin()!=p.end() && p.begin()->nr()==0 )
	    {
	    if( !removeFromList( &(*p.begin()) ))
		y2err( "not found:" << *p.begin() );
	    }
	setDeleted();
	}
    if( ret==0 )
	{
	unuseDev();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

void DmPartCo::removePresentPartitions()
    {
    VolPair p = volPair();
    if( !p.empty() )
	{
	bool save=silent;
	setSilent( true );
	list<VolIterator> l;
	for( VolIterator i=p.begin(); i!=p.end(); ++i )
	    {
	    y2mil( "rem:" << *i );
	    if( !i->created() )
		l.push_front( i );
	    }
	for( list<VolIterator>::const_iterator i=l.begin(); i!=l.end(); ++i )
	    {
	    doRemove( &(**i) );
	    }
	setSilent( save );
	}
    }

void DmPartCo::removeFromMemory()
    {
    VIter i = vols.begin();
    while( i!=vols.end() )
	{
	y2mil( "rem:" << *i );
	if( !(*i)->created() )
	    {
	    delete *i;
	    i = vols.erase( i );
	    }
	else
	    ++i;
	}
    }

static bool toChangeId( const DmPart&d ) 
    { 
    Partition* p = d.getPtr();
    return( p!=NULL && !d.deleted() && Partition::toChangeId(*p) );
    }


void
DmPartCo::getToCommit(CommitStage stage, list<const Container*>& col, list<const Volume*>& vol) const
{
    y2mil("col:" << col.size() << " vol:" << vol.size());
    getStorage()->logCo( this );
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    Container::getToCommit( stage, col, vol );
    if( stage==INCREASE )
	{
	ConstDmPartPair p = dmpartPair( toChangeId );
	for( ConstDmPartIter i=p.begin(); i!=p.end(); ++i )
	    if( find( vol.begin(), vol.end(), &(*i) )==vol.end() )
		vol.push_back( &(*i) );
	}
    if( disk->del_ptable && find( col.begin(), col.end(), this )==col.end() )
	col.push_back( this );
    if( col.size()!=oco || vol.size()!=ovo )
	y2mil("stage:" << stage << " col:" << col.size() << " vol:" << vol.size());
}


int DmPartCo::commitChanges( CommitStage stage, Volume* vol )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = Container::commitChanges( stage, vol );
    if( ret==0 && stage==INCREASE )
        {
        DmPart * dm = dynamic_cast<DmPart *>(vol);
        if( dm!=NULL )
            {
	    Partition* p = dm->getPtr();
            if( p && disk && Partition::toChangeId( *p ) )
                ret = doSetType( dm );
            }
        else
            ret = DMPART_INVALID_VOLUME;
        }
    y2mil("ret:" << ret);
    return( ret );
    }

int DmPartCo::commitChanges( CommitStage stage )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = 0;
    if( stage==DECREASE && deleted() )
	{
	ret = doRemove();
	}
    else if( stage==DECREASE && disk->del_ptable )
	{
	ret = doCreateLabel();
	}
    else
	ret = DMPART_COMMIT_NOTHING_TODO;
    y2mil("ret:" << ret);
    return( ret );
    }


void
DmPartCo::getCommitActions(list<commitAction>& l) const
    {
    y2mil( "l:" << l );
    Container::getCommitActions( l );
    y2mil( "l:" << l );
    if( deleted() || disk->del_ptable )
	{
	l.remove_if(stage_is(DECREASE));
	Text txt = deleted() ? removeText(false) : setDiskLabelText(false);
	l.push_front(commitAction(DECREASE, staticType(), txt, this, true));
	}
    y2mil( "l:" << l );
    }


int 
DmPartCo::doCreate( Volume* v ) 
    {
    y2mil("DmPart:" << name() << " name:" << v->name());
    DmPart * l = dynamic_cast<DmPart *>(v);
    int ret = disk ? 0 : DMPART_INTERNAL_ERR;
    if( ret==0 && l == NULL )
	ret = DMPART_INVALID_VOLUME;
    Partition *p = NULL;
    if( ret==0 )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->createText(true) );
	    }
	p = l->getPtr();
	if( p==NULL )
	    ret = DMPART_PARTITION_NOT_FOUND;
	else
	    ret = disk->doCreate( p );
	if( ret==0 && p->id()!=Partition::ID_LINUX )
	    ret = doSetType( l );
	}
    if( ret==0 )
	{
	l->setCreated(false);
	if( active )
	    {
	    activate_part(false);
	    activate_part(true);
	    ProcParts parts;
	    updateMinor();
	    l->updateSize(parts);
	    }
	if( p->type()!=EXTENDED )
	    Storage::waitForDevice(l->device());
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int DmPartCo::doRemove()
    {
    return( DMPART_NO_REMOVE );
    }

int DmPartCo::doRemove( Volume* v )
    {
    y2mil("DmPart:" << name() << " name:" << v->name());
    DmPart * l = dynamic_cast<DmPart *>(v);
    bool save_act = false;
    int ret = disk ? 0 : DMPART_INTERNAL_ERR;
    if( ret==0 && l == NULL )
	ret = DMPART_INVALID_VOLUME;
    if( ret==0 )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->removeText(true) );
	    }
	ret = v->prepareRemove();
	}
    if( ret==0 )
	{
	save_act = active;
	if( active )
	    activate_part(false);
	Partition *p = l->getPtr();
	if( p==NULL )
	    ret = DMPART_PARTITION_NOT_FOUND;
	else if( !deleted() )
	    ret = disk->doRemove( p );
	}
    if( ret==0 )
	{
	if( !removeFromList( l ) )
	    ret = DMPART_REMOVE_PARTITION_LIST_ERASE;
	}
    if( save_act && !deleted() )
	{
	activate_part(true);
	updateMinor();
	}
    if( ret==0 )
	Storage::waitForDevice();
    y2mil("ret:" << ret);
    return( ret );
    }

int DmPartCo::doResize( Volume* v ) 
    {
    y2mil("DmPart:" << name() << " name:" << v->name());
    DmPart * l = dynamic_cast<DmPart *>(v);
    int ret = disk ? 0 : DMPART_INTERNAL_ERR;
    if( ret==0 && l == NULL )
	ret = DMPART_INVALID_VOLUME;
    bool remount = false;
    bool needExtend = false;
    if( ret==0 )
	{
	needExtend = !l->needShrink();
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->resizeText(true) );
	    }
	if( l->isMounted() )
	    {
	    ret = l->umount();
	    if( ret==0 )
		remount = true;
	    }
	if( ret==0 && !needExtend && l->getFs()!=VFAT && l->getFs()!=FSNONE )
	    ret = l->resizeFs();
	}
    if( ret==0 )
	{
	Partition *p = l->getPtr();
	if( p==NULL )
	    ret = DMPART_PARTITION_NOT_FOUND;
	else
	    ret = disk->doResize( p );
	}
    if( ret==0 && active )
	{
	activate_part(false);
	activate_part(true);
	}
    if( ret==0 && needExtend && l->getFs()!=VFAT && l->getFs()!=FSNONE )
	ret = l->resizeFs();
    if( ret==0 )
	{
	ProcParts parts;
	updateMinor();
	l->updateSize(parts);
	Storage::waitForDevice(l->device());
	}
    if( ret==0 && remount )
	ret = l->mount();
    y2mil("ret:" << ret);
    return( ret );
    }

Text DmPartCo::setDiskLabelText( bool doing ) const
    {
	return disk->setDiskLabelText(doing);
    }

Text DmPartCo::removeText( bool doing ) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. pdc_igeeeadj),
        txt = sformat( _("Removing %1$s"), name().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. pdc_igeeeadj),
        txt = sformat( _("Remove %1$s"), name().c_str() );
        }
    return( txt );
    }


void
DmPartCo::setUdevData(const list<string>& id)
{  
    if (disk)
    {
	disk->setUdevData("", id);
    }
}


void DmPartCo::getInfo( DmPartCoInfo& tinfo ) const
    {
    if( disk )
	{
	disk->getInfo( info.d );
	}
    info.minor = mnr;
    info.devices.clear();
    list<Pv>::const_iterator i=pv.begin();
    while( i!=pv.end() )
	{
	if( !info.devices.empty() )
	    info.devices += ' ';
	info.devices += i->device;
	++i;
	}
    y2mil( "device:" << info.devices );
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const DmPartCo& d )
    {
    s << dynamic_cast<const PeContainer&>(d);
    if( !d.udev_id.empty() )
	s << " UdevId:" << d.udev_id;
    if( !d.active )
      s << " inactive";
    s << " geometry:" << d.disk->getGeometry();
    return( s );
    }


    void
    DmPartCo::logDifference(std::ostream& log, const DmPartCo& rhs) const
    {
	PeContainer::logDifference(log, rhs);

	logDiff(log, "active", active, rhs.active);
    }


bool DmPartCo::equalContent( const DmPartCo& rhs ) const
    {
    bool ret = PeContainer::equalContent(rhs,false) &&
	active==rhs.active;
    if( ret )
	{
	ConstDmPartPair pp = dmpartPair();
	ConstDmPartPair pc = rhs.dmpartPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
	}
    return( ret );
    }


    DmPartCo::DmPartCo(const DmPartCo& c)
	: PeContainer(c), udev_id(c.udev_id), active(c.active)
    {
	y2deb("copy-constructed DmPartCo from " << c.dev);

	disk = NULL;
	if (c.disk)
	    disk = new Disk(*c.disk);

	Storage::waitForDevice();
	ConstDmPartPair p = c.dmpartPair();
	for (ConstDmPartIter i = p.begin(); i != p.end(); ++i)
	{
	    DmPart* p = new DmPart(*this, *i);
	    vols.push_back(p);
	}

	updatePointers(true);
    }

}
