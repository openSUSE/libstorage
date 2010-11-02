/*
 * File: MdPartCo.cc
 *
 * Implementation of MdPartCo class which represents single MD Device (RAID
 * Volume) like md126 which is a Container for partitions.
 *
 * Copyright (c) 2009, Intel Corporation.
 * Copyright (c) [2009-2010] Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>

#include <sstream>
#include <algorithm>
#include <string>

#include "storage/MdPartCo.h"
#include "storage/MdPart.h"
#include "storage/SystemInfo.h"
#include "storage/ProcParts.h"
#include "storage/Partition.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"
#include "storage/Regex.h"
#include "storage/EtcMdadm.h"


namespace storage
{
    using namespace std;


    MdPartCo::MdPartCo(Storage* s, const string& name, const string& device, SystemInfo& systeminfo)
	: Container(s, name, device, staticType()), disk(NULL), md_type(RAID_UNK),
	  md_parity(PAR_DEFAULT), chunk_k(0), sb_ver("01.00.00"), destrSb(false),
	  has_container(false)
    {
	getMajorMinor();

	/* First Initialize RAID properties. */
	initMd(systeminfo);
	/* Initialize 'disk' part, partitions.*/
	init(systeminfo);

	setUdevData(systeminfo);

	y2deb("constructed MdPartCo " << dev);
    }


    MdPartCo::MdPartCo(const MdPartCo& c)
	: Container(c), md_type(c.md_type), md_parity(c.md_parity),
	  chunk_k(c.chunk_k), md_uuid(c.md_uuid), md_name(c.md_name),
	  sb_ver(c.sb_ver), destrSb(c.destrSb), devs(c.devs), spare(c.spare),
	  udev_id(c.udev_id),
	  has_container(c.has_container), parent_container(c.parent_container),
	  parent_uuid(c.parent_uuid), parent_md_name(c.parent_md_name),
	  parent_metadata(c.parent_metadata), parent_member(c.parent_member)
    {
	y2deb("copy-constructed MdPartCo by from " << c.nm);

	disk = NULL;
	if( c.disk )
	    disk = new Disk( *c.disk );
	ConstMdPartPair p = c.mdpartPair();
	for (ConstMdPartIter i = p.begin(); i != p.end(); ++i)
        {
	    MdPart* p = new MdPart(*this, *i);
	    vols.push_back(p);
        }
	updatePointers(true);
    }


MdPartCo::~MdPartCo()
    {
    if( disk )
        {
        delete disk;
        disk = NULL;
        }
    y2deb("destructed MdPartCo : " << dev);
    }


    string
    MdPartCo::sysfsPath() const
    {
	return SYSFSDIR "/" + procName();
    }


bool MdPartCo::isMdPart(const string& name) const
{
  string n = undevName(name);
  static Regex mdpart( "^md[0123456789]+p[0123456789]+$" );
  return (mdpart.match(n));
}


void MdPartCo::getPartNum(const string& device, unsigned& num) const
{
  string dev = device;
  string::size_type pos;

  pos = dev.find("p");
  if( pos != string::npos )
    {
      /* md125p12 - after p is 12, this will be returned. */
      dev.substr(pos+1) >> num;
    }
  else
    {
    num = 0;
    }
}

/* Add new partition after creation. Called in 'CreatePartition' */
int
MdPartCo::addNewDev(string& device)
{
    int ret = 0;
    if ( isMdPart(device) == false )
      {
        ret = MD_PARTITION_NOT_FOUND;
      }
    else
    {
        unsigned number;
        const string tmpS(device);
        getPartNum(tmpS,number);
        device = getPartDevice(number);
        Partition *p = getPartition( number, false );
        if( p==NULL )
          {
            ret = MDPART_PARTITION_NOT_FOUND;
          }
        else
        {
            MdPart * md = NULL;
            newP( md, p->nr(), p );
            md->getFsInfo( p );
            md->setCreated();
	    md->addUdevData();
            addToList( md );
            y2mil("device:" << device << " was added to MdPartCo : " << dev);

        }
        handleWholeDevice();
    }
    if( ret != 0 )
      {
      y2war("device:" << device << " was not added to MdPartCo : " << dev);
      }
    return ret;
}


int
MdPartCo::createPartition( storage::PartitionType type,
                           long unsigned start,
                           long unsigned len,
                           string& device,
                           bool checkRelaxed )
    {
	y2mil("begin type:" << toString(type) << " start:" << start << " len:" << len <<
	      " relaxed:" << checkRelaxed);
    int ret = disk ? 0 : MDPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
        ret = MDPART_CHANGE_READONLY;
    if( ret==0 )
      {
        ret = disk->createPartition( type, start, len, device, checkRelaxed );
      }
    if( ret==0 )
      {
        ret = addNewDev( device );
      }
    y2mil("ret:" << ret);
    return( ret );
    }

int
MdPartCo::createPartition( long unsigned len, string& device, bool checkRelaxed )
    {
    y2mil("len:" << len << " relaxed:" << checkRelaxed);
    int ret = disk ? 0 : MDPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
        ret = MDPART_CHANGE_READONLY;
    if( ret==0 )
        ret = disk->createPartition( len, device, checkRelaxed );
    if( ret==0 )
        ret = addNewDev( device );
    y2mil("ret:" << ret);
    return( ret );
    }

int
MdPartCo::createPartition( storage::PartitionType type, string& device )
    {
	y2mil("type:" << toString(type));
    int ret = disk ? 0 : MDPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
        ret = MDPART_CHANGE_READONLY;
    if( ret==0 )
        ret = disk->createPartition( type, device );
    if( ret==0 )
        ret = addNewDev( device );
    y2mil("ret:" << ret);
    return( ret );
    }

int MdPartCo::updateDelDev()
    {
    int ret = 0;
    list<Volume*> l;
    MdPartPair p=mdpartPair();
    MdPartIter i=p.begin();
    while( i!=p.end() )
        {
        Partition *p = i->getPtr();
        if( p && validPartition( p ) )
            {
            if( i->nr()!=p->nr() )
                {
                i->updateName();
                y2mil( "updated name md:" << *i );
                }
            if( i->deleted() != p->deleted() )
                {
                i->setDeleted( p->deleted() );
                y2mil( "updated del md:" << *i );
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
            ret = MDPART_PARTITION_NOT_FOUND;
        ++vi;
        }
    handleWholeDevice();
    return( ret );
    }

int
MdPartCo::removePartition( unsigned nr )
    {
    int ret = disk ? 0 : MDPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
        ret = MDPART_CHANGE_READONLY;
    if( ret==0 )
        {
        if( nr>0 )
          {
            ret = disk->removePartition( nr );
          }
        else
            ret = MDPART_PARTITION_NOT_FOUND;
        }
    if( ret==0 )
        ret = updateDelDev();
    y2mil("ret:" << ret);
    return( ret );
    }

int
MdPartCo::removeVolume( Volume* v )
    {
    int ret = disk ? 0 : MDPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
        ret = MDPART_CHANGE_READONLY;
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
MdPartCo::freeCylindersAroundPartition(const MdPart* dm, unsigned long& freeCylsBefore,
				       unsigned long& freeCylsAfter) const
{
    const Partition* p = dm->getPtr();
    int ret = p ? 0 : MDPART_PARTITION_NOT_FOUND;
    if (ret == 0)
    {
        ret = disk->freeCylindersAroundPartition(p, freeCylsBefore, freeCylsAfter);
    }
    y2mil("ret:" << ret);
    return ret;
}


int MdPartCo::resizePartition( MdPart* dm, unsigned long newCyl )
    {
    Partition * p = dm->getPtr();
    int ret = p?0:MDPART_PARTITION_NOT_FOUND;
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
MdPartCo::resizeVolume( Volume* v, unsigned long long newSize )
    {
    int ret = disk ? 0 : MDPART_INTERNAL_ERR;
    if( ret==0 && readonly() )
        ret = MDPART_CHANGE_READONLY;
    MdPart * l = dynamic_cast<MdPart *>(v);
    if( ret==0 && l==NULL )
        ret = MDPART_INVALID_VOLUME;
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
            ret = MDPART_PARTITION_NOT_FOUND;
        }
    if( ret==0 )
        {
        l->updateSize();
        }
    y2mil("ret:" << ret);
    return( ret );
    }


void
MdPartCo::init(SystemInfo& systeminfo)
{
    systeminfo.getProcParts().getSize(procName(), size_k);
    y2mil("nm:" << nm << " size_k:" << size_k);
    createDisk(systeminfo);
    getVolumes(systeminfo.getProcParts());
}


void
MdPartCo::createDisk(SystemInfo& systeminfo)
    {
    if( disk )
        delete disk;
    disk = new Disk(getStorage(), nm, dev, size_k, systeminfo);
    disk->setNumMinor( 64 );
    disk->setSilent();
    disk->setSlave();
    disk->detect(systeminfo);
    }

// Creates new partition.
void
MdPartCo::newP( MdPart*& dm, unsigned num, Partition* p )
    {
    dm = new MdPart(*this, getPartName(num), getPartDevice(num), num, p);
    }

//This seems to detect partitions from ppart and adds them to Container.
void
MdPartCo::getVolumes(const ProcParts& ppart)
    {
    vols.clear();
    Disk::PartPair pp = disk->partPair();
    Disk::PartIter i = pp.begin();
    MdPart * p = NULL;
    while( i!=pp.end() )
        {
        newP( p, i->nr(), &(*i) );
	p->updateSize(ppart);
        addToList( p );
        ++i;
        }
    handleWholeDevice();
    }

void MdPartCo::handleWholeDevice()
    {
    Disk::PartPair pp = disk->partPair( Partition::notDeleted );
    y2mil("empty:" << pp.empty());

    if( pp.empty() )
        {
        MdPart * p = NULL;
        newP( p, 0, NULL );
        p->setSize( size_k );
        addToList( p );
        }
    else
        {
        MdPartIter i;
        if( findMdPart( 0, i ))
            {
            MdPart* md = &(*i);
            if( !removeFromList( md ))
                y2err( "not found:" << *i );
            }
        }
    }

Partition*
MdPartCo::getPartition( unsigned nr, bool del )
    {
    Partition* ret = NULL;
    Disk::PartPair pp = disk->partPair();
    Disk::PartIter i = pp.begin();
    while( i!=pp.end() &&
           (nr!=i->nr() || del!=i->deleted()) )
      {
        ++i;
      }
    if( i!=pp.end() )
        ret = &(*i);
    if( ret )
      {
        y2mil( "nr:" << nr << " del:" << del << " *p:" << *ret );
      }
    else
      {
        y2mil( "nr:" << nr << " del:" << del << " p:NULL" );
      }
    return( ret );
    }

bool
MdPartCo::validPartition( const Partition* p )
    {
    bool ret = false;
    Disk::ConstPartPair pp = disk->partPair();
    Disk::ConstPartIter i = pp.begin();
    while( i!=pp.end() && p != &(*i) )
        ++i;
    ret = i!=pp.end();
    y2mil( "nr:" << p->nr() << " ret:" << ret );
    return( ret );
    }

void MdPartCo::updatePointers( bool invalid )
    {
    MdPartPair p=mdpartPair();
    MdPartIter i=p.begin();
    while( i!=p.end() )
        {
        if( invalid )
            i->setPtr( getPartition( i->nr(), i->deleted() ));
        i->updateName();
        ++i;
        }
    }

void MdPartCo::updateMinor()
    {
    MdPartPair p=mdpartPair();
    for (MdPartIter i = p.begin(); i != p.end(); ++i)
        i->updateMinor();
    }


string
MdPartCo::getPartName( unsigned mdNum ) const
    {
    string ret = nm;
    if( mdNum>0 )
        {
        ret += "p";
        ret += decString(mdNum);
        }
    return( ret );
    }


string
MdPartCo::getPartDevice( unsigned mdNum ) const
    {
    string ret = dev;
    if( mdNum>0 )
        {
        ret += "p";
        ret += decString(mdNum);
        }
    return( ret );
    }


//
// Assumption is that we're using /dev not /dev/md
// directory.
string MdPartCo::undevName( const string& name )
    {
    string ret = name;
    if( ret.find( "/dev/" ) == 0 )
        ret.erase( 0, 5 );
    return( ret );
    }

int MdPartCo::destroyPartitionTable( const string& new_label )
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

int MdPartCo::changePartitionId( unsigned nr, unsigned id )
    {
    int ret = nr>0?0:MDPART_PARTITION_NOT_FOUND;
    if( ret==0 )
        {
        ret = disk->changePartitionId( nr, id );
        }
    y2mil("ret:" << ret);
    return( ret );
    }

int MdPartCo::forgetChangePartitionId( unsigned nr )
    {
    int ret = nr>0?0:MDPART_PARTITION_NOT_FOUND;
    if( ret==0 )
        {
        ret = disk->forgetChangePartitionId( nr );
        }
    y2mil("ret:" << ret);
    return( ret );
    }


int
MdPartCo::nextFreePartition(PartitionType type, unsigned& nr, string& device) const
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
    MdPartCo::changePartitionArea(unsigned nr, const Region& cylRegion, bool checkRelaxed)
    {
    int ret = nr>0?0:MDPART_PARTITION_NOT_FOUND;
    if( ret==0 )
        {
	ret = disk->changePartitionArea(nr, cylRegion, checkRelaxed);
        MdPartIter i;
        if( findMdPart( nr, i ))
            i->updateSize();
        }
    y2mil("ret:" << ret);
    return( ret );
    }

bool MdPartCo::findMdPart( unsigned nr, MdPartIter& i )
    {
    MdPartPair p = mdpartPair( MdPart::notDeleted );
    i=p.begin();
    while( i!=p.end() && i->nr()!=nr )
        ++i;
    return( i!=p.end() );
    }

//Do we need to activate partition? it will be activated already
void MdPartCo::activate_part( bool val )
    {
  (void)val;
    }

int MdPartCo::doSetType( MdPart* md )
    {
    y2mil("doSetType container:" << name() << " name:" << md->name());
    Partition * p = md->getPtr();
    int ret = p?0:MDPART_PARTITION_NOT_FOUND;
    if( ret==0 )
        {
        if( !silent )
            {
            getStorage()->showInfoCb( md->setTypeText(true) );
            }
        ret = disk->doSetType( p );
        }
    y2mil("ret:" << ret);
    return( ret );
    }

int MdPartCo::doCreateLabel()
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
        getStorage()->waitForDevice();
        }
    y2mil("ret:" << ret);
    return( ret );
    }

int
MdPartCo::removeMdPart()
    {
    int ret = 0;
    y2mil("begin");
    if( readonly() )
        {
        y2war("Read-Only RAID.");
        ret = MDPART_CHANGE_READONLY;
        }
    if( ret==0 && !created() )
        {
        //Remove partitions
        MdPartPair p=mdpartPair(MdPart::notDeleted);
        for( MdPartIter i=p.begin(); i!=p.end(); ++i )
            {
            if( i->nr()>0 )
              {
                ret = removePartition( i->nr() );
                if( ret != 0 )
                  {
                  // Error. Break.
                  break;
                  }

              }
            }
        //Remove 'whole device' it was created when last partition was deleted.
        p=mdpartPair(MdPart::notDeleted);
        if( p.begin()!=p.end() && p.begin()->nr()==0 )
            {
            if( !removeFromList( &(*p.begin()) ))
              {
                y2err( "not found:" << *p.begin() );
                ret = MDPART_PARTITION_NOT_FOUND;
              }
            }
        }
    if( ret==0 )
      {
      unuseDevs();
      setDeleted( true );
      destrSb = true;
      }
    y2mil("ret:" << ret);
    return( ret );
    }


void MdPartCo::unuseDevs() const
{
    getStorage()->clearUsedBy(getDevs());
}


void MdPartCo::removePresentPartitions()
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

void MdPartCo::removeFromMemory()
    {
    VIter i = vols.begin();
    while( i!=vols.end() )
        {
        y2mil( "rem:" << *i );
        if( !(*i)->created() )
            {
            i = vols.erase( i );
            }
        else
            ++i;
        }
    }

static bool toChangeId( const MdPart&d )
    {
    Partition* p = d.getPtr();
    return( p!=NULL && !d.deleted() && Partition::toChangeId(*p) );
    }

void MdPartCo::getToCommit(CommitStage stage, list<const Container*>& col,
                           list<const Volume*>& vol) const
    {
    y2mil("col:" << col.size() << " << vol:" << vol.size());
    getStorage()->logCo( this );
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    Container::getToCommit( stage, col, vol );
    if( stage==INCREASE )
        {
        ConstMdPartPair p = mdpartPair( toChangeId );
        for( ConstMdPartIter i=p.begin(); i!=p.end(); ++i )
            if( find( vol.begin(), vol.end(), &(*i) )==vol.end() )
                vol.push_back( &(*i) );
        }
    if( disk->del_ptable && find( col.begin(), col.end(), this )==col.end() )
        col.push_back( this );
    if( col.size()!=oco || vol.size()!=ovo )
        y2mil("col:" << col.size() << " vol:" << vol.size());
    }


int MdPartCo::commitChanges( CommitStage stage, Volume* vol )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = Container::commitChanges( stage, vol );
    if( ret==0 && stage==INCREASE )
        {
        MdPart * dm = dynamic_cast<MdPart *>(vol);
        if( dm!=NULL )
            {
            Partition* p = dm->getPtr();
            if( p && disk && Partition::toChangeId( *p ) )
                ret = doSetType( dm );
            }
        else
            ret = MDPART_INVALID_VOLUME;
        }
    y2mil("ret:" << ret);
    return( ret );
    }

int MdPartCo::commitChanges( CommitStage stage )
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
        ret = MDPART_COMMIT_NOTHING_TODO;
    y2mil("ret:" << ret);
    return( ret );
    }

void
MdPartCo::getCommitActions(list<commitAction>& l ) const
    {
    y2mil( "l:" << l );
    Container::getCommitActions( l );
    y2mil( "l:" << l );
    if( deleted() || disk->del_ptable )
        {
        list<commitAction>::iterator i = l.begin();
        while (i != l.end())
            {
            if (i->stage == DECREASE)
                {
                i=l.erase( i );
                }
            else
                ++i;
            }
        Text txt = deleted() ? removeText(false) : setDiskLabelText(false);
        l.push_front(commitAction( DECREASE, staticType(), txt, this, true));
        }
    y2mil( "l:" << l );
    }

int
MdPartCo::doCreate( Volume* v )
    {
    y2mil("name:" << name() << " v->name:" << v->name());
    MdPart * l = dynamic_cast<MdPart *>(v);
    int ret = disk ? 0 : MDPART_INTERNAL_ERR;
    if( ret==0 && l == NULL )
        ret = MDPART_INVALID_VOLUME;
    Partition *p = NULL;
    if( ret==0 )
        {
        if( !silent )
            {
            getStorage()->showInfoCb( l->createText(true) );
            }
        p = l->getPtr();
        if( p==NULL )
            ret = MDPART_PARTITION_NOT_FOUND;
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
            ProcParts pp;
            updateMinor();
            l->updateSize( pp );
            }
        if( p->type()!=EXTENDED )
            getStorage()->waitForDevice( l->device() );
        }
    y2mil("ret:" << ret);
    return( ret );
    }

//Remove MDPART unless:
//1. It's IMSM or DDF SW RAID
//2. It contains partitions.
int MdPartCo::doRemove()
    {
    y2mil("begin");
    // 1. Check Metadata.
    if( sb_ver == "imsm" || sb_ver == "ddf" )
      {
      y2err("Cannot remove IMSM or DDF SW RAIDs.");
      return (MDPART_NO_REMOVE);
      }
    // 2. Check for partitions.
    if( disk!=NULL && disk->numPartitions()>0 )
      {
      int permitRemove=1;
      //handleWholeDevice: partition 0.
      if( disk->numPartitions() == 1 )
        {
        //Find partition '0' if it exists then this 'whole device'
        MdPartIter i;
        if( findMdPart( 0, i ) == true)
          {
          //Single case when removal is allowed.
          permitRemove = 0;
          }
        }
      if( permitRemove == 1 )
        {
        y2err("Cannot remove RAID with partitions.");
        return (MDPART_NO_REMOVE);
        }
      }
    /* Try to remove this. */
    y2mil("Raid:" << name() << " is going to be removed permanently");
    int ret = 0;
    if( deleted() )
      {
      string cmd = MDADMBIN " --stop " + quote(device());
      SystemCmd c( cmd );
      if( c.retcode()!=0 )
        {
        ret = MD_REMOVE_FAILED;
        setExtError( c );
        }
      if( !silent )
        {
        getStorage()->showInfoCb( removeText(true) );
        }
      if( ret==0 && destrSb )
        {
        SystemCmd c;
        list<string> d = getDevs();
        for( list<string>::const_iterator i=d.begin(); i!=d.end(); ++i )
          {
          c.execute(MDADMBIN " --zero-superblock " + quote(*i));
          }
        }
      if( ret==0 )
        {
	EtcMdadm* mdadm = getStorage()->getMdadm();
	if (mdadm)
	    mdadm->removeEntry(getMdUuid());
        }
      }
    y2mil("Done, ret:" << ret);
    return( ret );
    }

int MdPartCo::doRemove( Volume* v )
    {
    y2mil("name:" << name() << " v->name:" << v->name());
    MdPart * l = dynamic_cast<MdPart *>(v);
    int ret = disk ? 0 : MDPART_INTERNAL_ERR;
    if( ret==0 && l == NULL )
        ret = MDPART_INVALID_VOLUME;
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
        Partition *p = l->getPtr();
        if( p==NULL )
          {
            y2err("Partition not found");
            ret = MDPART_PARTITION_NOT_FOUND;
          }
        else
          {
          ret = disk->doRemove( p );
          }
        }
    if( ret==0 )
        {
        if( !removeFromList( l ) )
          {
            y2war("Couldn't remove parititon from list.");
            ret = MDPART_REMOVE_PARTITION_LIST_ERASE;
          }
        }
    if( ret==0 )
        getStorage()->waitForDevice();
    y2mil("Done, ret:" << ret);
    return( ret );
    }

int MdPartCo::doResize( Volume* v )
    {
    y2mil("name:" << name() << " v->name:" << v->name());
    MdPart * l = dynamic_cast<MdPart *>(v);
    int ret = disk ? 0 : MDPART_INTERNAL_ERR;
    if( ret==0 && l == NULL )
        ret = MDPART_INVALID_VOLUME;
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
            ret = MDPART_PARTITION_NOT_FOUND;
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
        ProcParts pp;
        updateMinor();
        l->updateSize( pp );
        getStorage()->waitForDevice( l->device() );
        }
    if( ret==0 && remount )
        ret = l->mount();
    y2mil("ret:" << ret);
    return( ret );
    }

 
Text
MdPartCo::setDiskLabelText(bool doing) const
    {
	return disk->setDiskLabelText(doing);
    }


Text
MdPartCo::removeText( bool doing ) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. pdc_igeeeadj),
	txt = sformat( _("Removing software RAID %1$s"), name().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. pdc_igeeeadj),
	txt = sformat( _("Remove software RAID %1$s"), name().c_str() );
        }
    return txt;
    }


void
MdPartCo::setUdevData(SystemInfo& systeminfo)
{
    const UdevMap& by_id = systeminfo.getUdevMap("/dev/disk/by-id");
    UdevMap::const_iterator it = by_id.find(nm);
    if (it != by_id.end())
    {
	udev_id = it->second;
	partition(udev_id.begin(), udev_id.end(), string_starts_with("md-uuid-"));
    }
    else
    {
	udev_id.clear();
    }

    y2mil("dev:" << dev << " udev_id:" << udev_id);

    alt_names.remove_if(string_starts_with("/dev/disk/by-id/"));
    for (list<string>::const_iterator i = udev_id.begin(); i != udev_id.end(); ++i)
	alt_names.push_back("/dev/disk/by-id/" + *i);

    if (disk)
    {
        disk->setUdevData("", udev_id);
    }
    MdPartPair pp = mdpartPair();
    for( MdPartIter p=pp.begin(); p!=pp.end(); ++p )
    {
	p->addUdevData();
    }
}


void MdPartCo::getInfo( MdPartCoInfo& tinfo ) const
    {
    if( disk )
        {
        disk->getInfo( info.d );
        }

    info.type = md_type;
    info.nr = mnr;
    info.parity = md_parity;
    info.uuid = md_uuid;
    info.sb_ver = sb_ver;
    info.chunkSizeK = chunk_k;

    info.devices = boost::join(devs, " ");
    info.spares = boost::join(spare, " ");

    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const MdPartCo& d )
    {
    s << dynamic_cast<const Container&>(d)
      << " Personality:" << toString(d.md_type);
    if (d.chunk_k > 0)
	s << " ChunkK:" << d.chunk_k;
    if (d.md_parity != PAR_DEFAULT)
	s << " Parity:" << toString(d.md_parity);
    if (!d.sb_ver.empty() )
	s << " SbVer:" << d.sb_ver;
    if (!d.md_uuid.empty())
	s << " md_uuid:" << d.md_uuid; 
    if (!d.md_name.empty())
	s << " md_name:" << d.md_name;
    if( d.destrSb )
	s << " destroySb";
    s << " Devices:" << d.devs;
    if( !d.spare.empty() )
	s << " Spares:" << d.spare;
    s << " geometry:" << d.disk->getGeometry();
    return s;
    }


    void
    MdPartCo::logDifference(std::ostream& log, const MdPartCo& rhs) const
    {
	Container::logDifference(log, rhs);

	logDiff(log, "active", active, rhs.active);

	logDiffEnum(log, "md_type", md_type, rhs.md_type);
	logDiffEnum(log, "md_parity", md_parity, rhs.md_parity);
	logDiff(log, "chunk_k", chunk_k, rhs.chunk_k);
	logDiff(log, "sb_ver", sb_ver, rhs.sb_ver);
	logDiff(log, "md_uuid", md_uuid, rhs.md_uuid);
	logDiff(log, "md_name", md_name, rhs.md_name);
	logDiff(log, "destrSb", destrSb, rhs.destrSb);
	logDiff(log, "devices", devs, rhs.devs);
	logDiff(log, "spares", spare, rhs.spare);

	logDiff(log, "parent_container",  parent_container, rhs.parent_container);
	logDiff(log, "parent_md_name", parent_md_name, rhs.parent_md_name);
	logDiff(log, "parent_metadata", parent_metadata, rhs.parent_metadata);
	logDiff(log, "parent_uuid", parent_uuid, rhs.parent_uuid);
    }


    void
    MdPartCo::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
	const MdPartCo& rhs = dynamic_cast<const MdPartCo&>(rhs_c);

	logDifference(log, rhs);
	log << endl;

	ConstMdPartPair pp = mdpartPair();
        ConstMdPartPair pc = rhs.mdpartPair();
	logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


bool MdPartCo::equalContent( const Container& rhs ) const
    {
    bool ret = Container::equalContent(rhs);
    if( ret )
      {
      const MdPartCo* mdp = dynamic_cast<const MdPartCo*>(&rhs);
      if( mdp == 0 )
        {
        return false;
        }
      ret = ret &&
          active==mdp->active;
      ret = ret &&
          (chunk_k == mdp->chunk_k &&
              md_type == mdp->md_type &&
              md_parity == mdp->md_parity &&
              sb_ver == mdp->sb_ver &&
              devs == mdp->devs &&
              spare == mdp->spare &&
              md_uuid == mdp->md_uuid &&
              destrSb == mdp->destrSb &&
              md_name == mdp->md_name);
      if( ret )
        {
        if( has_container )
          {
          ret = ret &&
              (parent_container == mdp->parent_container &&
               parent_md_name == mdp->parent_md_name &&
               parent_metadata == mdp->parent_metadata &&
               parent_uuid == mdp->parent_uuid);
          }
        }
      if( ret )
        {
        ConstMdPartPair pp = mdpartPair();
        ConstMdPartPair pc = mdp->mdpartPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
        }
      } 
    return ret;
    }


// Get list of active MD RAID's
// cat /proc/mdstat
// If we're looking for Volume then
// find devname and in next line will be: 'external:imsm'
//Personalities : [raid0] [raid1]
//md125 : active (auto-read-only) raid1 sdb[1] sdc[0]
//      130071552 blocks super external:/md127/1 [2/2] [UU]
//
//md126 : active raid0 sdb[1] sdc[0]
//      52428800 blocks super external:/md127/0 128k chunks
//
//md127 : inactive sdc[1](S) sdb[0](S)
//      4514 blocks super external:imsm
//
//unused devices: <none>


    list<string>
    MdPartCo::getMdRaids(SystemInfo& systeminfo)
    {
	list<string> ret;

	const ProcMdstat& procmdstat = systeminfo.getProcMdstat();
	for (ProcMdstat::const_iterator it = procmdstat.begin(); it != procmdstat.end(); ++it)
	{
	    if (!it->second.is_container)
		ret.push_back(it->first);
	}

	return ret;
    }


    list<string>
    MdPartCo::getDevs(bool all, bool spares) const
    {
	list<string> ret;
	if (!all)
	{
	    ret = spares ? spare : devs;
	}
	else
        {
	    ret = devs;
	    ret.insert(ret.end(), spare.begin(), spare.end());
        }
	return ret;
    }


void MdPartCo::setSize(unsigned long long size )
{
  size_k = size;
}


    void
    MdPartCo::initMd(SystemInfo& systeminfo)
    {
	ProcMdstat::Entry entry;
	if (!systeminfo.getProcMdstat().getEntry(nm, entry))
	    y2err("not found in mdstat nm:" << nm);

	md_type = entry.md_type;
	md_parity = entry.md_parity;

	setSize(entry.size_k);
	chunk_k = entry.chunk_k;

	devs = entry.devices;
	spare = entry.spares;

	if (entry.has_container)
	{
	    has_container = true;
	    parent_container = entry.container_name;

	    MdadmDetails details;
	    if (getMdadmDetails("/dev/" + entry.container_name, details))
	    {
		parent_uuid = details.uuid;
		parent_md_name = details.devname;
		parent_metadata = details.metadata;
	    }

	    parent_member = entry.container_member;

	    sb_ver = parent_metadata;
	}
	else
	{
	    sb_ver = entry.super;
	}

	MdadmDetails details;
	if (getMdadmDetails("/dev/" + nm, details))
	{
	    md_uuid = details.uuid;
	    md_name = details.devname;
	}

	getStorage()->addUsedBy(devs, UB_MDPART, dev);
	getStorage()->addUsedBy(spare, UB_MDPART, dev);
    }


void MdPartCo::activate( bool val, const string& tmpDir  )
{
  if( active!=val )
      {
      MdCo::activate(val,tmpDir);
      active = val;
      }
}

bool MdPartCo::matchRegex( const string& dev )
    {
    static Regex md( "^md[0123456789]+$" );
    return( md.match(dev));
    }

bool MdPartCo::mdStringNum( const string& name, unsigned& num )
    {
    bool ret=false;
    string d = undevDevice(name);
    if( matchRegex( d ))
        {
        d.substr( 2 )>>num;
        ret = true;
        }
    return( ret );
    }


int
MdPartCo::getMdPartCoState(MdPartCoStateInfo& info) const
{
    string value;
    if (read_sysfs_property(sysfsPath() + "/md/array_state", value))
	if (toValue(value, info.state))
	    return STORAGE_NO_ERROR;

    return MD_GET_STATE_FAILED;
}


/*
 * Return true if on RAID Volume has a partition table
 *
 * : /usr/sbin/parted name print - will return:
 * 1. List of partitions if partition table exits.
 * 2. Error if no partition table is on device.
 *
 * Ad 2. Clean newly created device or FS on device.
 */
bool MdPartCo::hasPartitionTable(const string& name, SystemInfo& systeminfo)
{
    const Parted& parted = systeminfo.getParted("/dev/" + name);
    string dlabel = parted.getLabel();
    bool ret = !dlabel.empty() && dlabel != "loop";
    y2mil("name:" << name << " ret:" << ret);
    return ret;
}


bool
MdPartCo::hasFileSystem(const string& name, SystemInfo& systeminfo)
{
    bool ret = false;

    Blkid::Entry entry;
    if (systeminfo.getBlkid().getEntry("/dev/" + name, entry))
    {
	ret = entry.is_fs || entry.is_lvm || entry.is_luks;
    }

    y2mil("name:" << name << " ret:" << ret);
    return ret;
}


void
MdPartCo::syncMdadm(EtcMdadm* mdadm) const
{
    updateEntry(mdadm);
}


    bool
    MdPartCo::updateEntry(EtcMdadm* mdadm) const
    {
	EtcMdadm::mdconf_info info;

	if (!md_name.empty())
	    info.device = "/dev/md/" + md_name;
	else
	    info.device = dev;

	info.uuid = md_uuid;

	if (has_container)
        {
	    info.container_present = true;
	    info.container_uuid = parent_uuid;
	    info.container_metadata = parent_metadata;
	    info.container_member = parent_member;
        }

	return mdadm->updateEntry(info);
    }


bool
MdPartCo::scanForRaid(list<string>& raidNames)
{
    bool ret = false;
  raidNames.clear();

  SystemCmd c(MDADMBIN " --examine --scan");
  if( c.retcode() == 0 )
    {
    for(unsigned i = 0; i < c.numLines(false); i++ )
      {
      //Example:
      //ARRAY metadata=imsm UUID=b...5
      //ARRAY /dev/md/Vol_r5 container=b...5 member=0 UUID=0...c
      //ARRAY metadata=imsm UUID=8...b
      //ARRAY /dev/md/Vol0 container=8...b member=0 UUID=7...9
      string line = c.getLine(i);
      string dev_name = extractNthWord( 1, line );
      if( dev_name.find("/dev/md/") == 0 )
        {
        dev_name.erase(0,8);
        raidNames.push_back(dev_name);
        }
      }
    ret = true;
    }
  y2mil(" Detected list of MD RAIDs : " << raidNames);
  return ret;
}


CType
MdPartCo::envSelection(const string& name)
{
    string str = "LIBSTORAGE_" + boost::to_upper_copy(name, locale::classic());
    const char* tenv = getenv(str.c_str());
  if( tenv == NULL )
    {
    return CUNKNOWN;
    }
  string isMd(tenv);
  if( isMd == "MD" )
    {
    return MD;
    }
  if( isMd == "MDPART" )
    {
    return MDPART;
    }
  return CUNKNOWN;
}


bool
MdPartCo::havePartsInProc(const string& name, SystemInfo& systeminfo)
{
    string reg = "^" "/dev/" + name + "p[1-9]+" "$";
    list <string> parts = systeminfo.getProcParts().getMatchingEntries(regex_matches(reg));
    bool ret = !parts.empty();
    y2mil("name:" << name << " ret:" << ret);
    return ret;
}


list<string>
MdPartCo::filterMdPartCo(const list<string>& raidList, SystemInfo& systeminfo, bool instsys)
{
  list<string> mdpList;

  for( list<string>::const_iterator i=raidList.begin(); i!=raidList.end(); ++i )
    {
	y2mil("name:" << *i);

	CType ctype = envSelection(*i);
	if (ctype == MD)
      {
      // skip
      continue;
      }
    if (ctype == MDPART)
      {
      mdpList.push_back(*i);
      continue;
      }

    if (havePartsInProc(*i, systeminfo))
      {
      mdpList.push_back(*i);
      continue;
      }

    if (instsys)
      {
      // 1. With Partition Table
      // 2. Without Partition Table and without FS on it.
      // 3. this gives: No FS.
	  if (!hasFileSystem(*i, systeminfo))
        {
        mdpList.push_back(*i);
        }
      }
    else
      {
      // In 'normal' mode ONLY volume with Partition Table.
      // Partitions should be visible already so check it.
	  if (hasPartitionTable(*i, systeminfo))
        {
        mdpList.push_back(*i);
        }
      }
    }
  y2mil("List of partitionable devs: " << mdpList);
  return mdpList;
}


bool MdPartCo::active = false;

}
