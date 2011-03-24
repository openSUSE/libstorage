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


#include <ostream>
#include <sstream>

#include "storage/LvmVg.h"
#include "storage/LvmLv.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


static bool lvNotCreated( const LvmLv& l ) { return( !l.created() ); }
static bool lvNotDeletedCreated( const LvmLv& l ) { return( !l.created()&&!l.deleted() ); }


    LvmVg::LvmVg(Storage* s, const string& name, const string& device, bool lvm1)
	: PeContainer(s, name, device, staticType()), lvm1(lvm1)
    {
	y2deb("constructing LvmVg name:" << name << " lvm1:" << lvm1);
	setCreated(true);
    }


    LvmVg::LvmVg(Storage* s, const string& name, const string& device, SystemInfo& systeminfo)
	: PeContainer(s, name, device, staticType(), systeminfo), lvm1(false)
    {
	y2deb("constructing LvmVg name:" << name);
	getVgData(name, false);
    }


    LvmVg::LvmVg(Storage* s, const xmlNode* node)
	: PeContainer(s, staticType(), node), lvm1(false)
    {
	const list<const xmlNode*> l = getChildNodes(node, "logical_volume");
	for (list<const xmlNode*>::const_iterator it = l.begin(); it != l.end(); ++it)
	    addToList(new LvmLv(*this, *it));

	y2deb("constructed LvmVg " << dev);
    }


    LvmVg::LvmVg(const LvmVg& c)
	: PeContainer(c), status(c.status), uuid(c.uuid), lvm1(c.lvm1)
    {
	y2deb("copy-constructed LvmVg " << dev);

	ConstLvmLvPair p = c.lvmLvPair();
	for (ConstLvmLvIter i = p.begin(); i != p.end(); ++i)
	{
	    LvmLv* p = new LvmLv(*this, *i);
	    vols.push_back(p);
	}
    }


    LvmVg::~LvmVg()
    {
	y2deb("destructed LvmVg " << dev);
    }


    void
    LvmVg::saveData(xmlNode* node) const
    {
	PeContainer::saveData(node);

	ConstLvmLvPair vp = lvmLvPair();
	for (ConstLvmLvIter v = vp.begin(); v != vp.end(); ++v)
	    v->saveData(xmlNewChild(node, "logical_volume"));
    }


static bool lvDeleted( const LvmLv& l ) { return( l.deleted() ); }
static bool lvCreated( const LvmLv& l ) { return( l.created() ); }
static bool lvResized( const LvmLv& l ) { return( l.extendSize()!=0 ); }


unsigned
LvmVg::numLv() const
{
    return lvmLvPair(LvmLv::notDeleted).length();
}


int
LvmVg::removeVg()
    {
    int ret = 0;
    y2mil("begin");
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 && !created() )
	{
	LvmLvPair p = lvmLvPair(LvmLv::notDeleted);
	for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
	    ret = removeLv( i->name() );
	setDeleted();
	}
    if( ret==0 )
	{
	unuseDev();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LvmVg::extendVg( const string& dev )
    {
    list<string> l;
    l.push_back( dev );
    return( extendVg( l ) );
    }

int
LvmVg::extendVg( const list<string>& devs )
    {
    int ret = 0;
    y2mil( "name:" << name() << " devices:" << devs );
    y2mil( "this:" << *this );

    checkConsistency();
    list<string>::const_iterator i=devs.begin();
    list<Pv>::iterator p;
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    else if( devs.empty() )
	{
	ret = LVM_LIST_EMPTY;
	}
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	if( (p=find( pv.begin(), pv.end(), d ))!=pv.end() ||
	    (p=find( pv_add.begin(), pv_add.end(), d ))!=pv_add.end())
	    ret = LVM_PV_ALREADY_CONTAINED;
	else if( (p=find( pv_remove.begin(), pv_remove.end(), d )) !=
	         pv_remove.end() && !getStorage()->deletedDevice( d ) )
	    {
	    }
	else if( !getStorage()->knownDevice( d, true ) )
	    {
	    ret = LVM_PV_DEVICE_UNKNOWN;
	    }
	else if( !getStorage()->canUseDevice( d, true ) )
	    {
	    ret = LVM_PV_DEVICE_USED;
	    }
	++i;
	}
    i=devs.begin();
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	unsigned long pe = 0;
	if( (p=find( pv_remove.begin(), pv_remove.end(), d )) !=
	     pv_remove.end() && !getStorage()->deletedDevice( d ) )
	    {
	    pv.push_back( *p );
	    pe = p->num_pe;
	    pv_remove.erase( p );
	    }
	else
	    {
	    Pv pvn;
	    unsigned long long s = getStorage()->deviceSize( d );
	    pe = (s - 500)/pe_size;
	    pvn.num_pe = pvn.free_pe = pe;
	    pvn.device = d;

	    const Volume* v;
	    if (getStorage()->findVolume(d, v) && v->dmcrypt())
		pvn.dmcryptDevice = v->dmcryptDevice();

	    pv_add.push_back( pvn );
	    if( !getStorage()->isDisk(d))
		getStorage()->changeFormatVolume( d, false, FSNONE );
	    }
	getStorage()->setUsedBy(d, UB_LVM, device());
	free_pe += pe;
	num_pe += pe;
	calcSize();
	++i;
	}
    if( ret==0 && pv_add.size()+pv.size()-pv_remove.size()<=0 )
	ret = LVM_VG_HAS_NONE_PV;
    if( ret==0 )
	checkConsistency();
    y2mil( "this:" << *this );
    y2mil("ret:" << ret);
    return( ret );
    }

int
LvmVg::reduceVg( const string& dev )
    {
    list<string> l;
    l.push_back( dev );
    return( reduceVg( l ) );
    }


int
LvmVg::reduceVg( const list<string>& devs )
    {
    int ret = 0;
    y2mil("name:" << name() << " devices:" << devs);
    y2mil("this:" << *this);
    y2mil("add:" << pv_add.size() << " pv:" << pv.size() << " remove:" << pv_remove.size());

    checkConsistency();

    list<Pv> pl = pv;
    list<Pv> pladd = pv_add;
    list<Pv> plrem = pv_remove;
    unsigned long rem_pe = 0;
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    else if( devs.empty() )
	{
	ret = LVM_LIST_EMPTY;
	}

    list<string>::const_iterator i = devs.begin();
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	ret = tryUnusePe( d, pl, pladd, plrem, rem_pe );
	++i;
	}

    if( ret==0 && pv_add.size()+pv.size()-devs.size()<=0 )
	ret = LVM_VG_HAS_NONE_PV;
    if( ret == 0 )
	{
	pv = pl;
	pv_add = pladd;
	pv_remove = plrem;
	free_pe -= rem_pe;
	num_pe -= rem_pe;
	calcSize();
	}
    if( ret==0 )
	checkConsistency();

    y2mil("this:" << *this);
    y2mil("add:" << pv_add.size() << " pv:" << pv.size() << " remove:" << pv_remove.size());
    y2mil("ret:" << ret);
    return ret;
    }


int
LvmVg::setPeSize( long long unsigned peSizeK )
    {
    int ret = 0;
    y2mil("old:" << pe_size << " new:" << peSizeK);
    if( peSizeK != pe_size )
	{
	unsigned long long old_pe = pe_size;
	ret = PeContainer::setPeSize( peSizeK, lvm1 );
	if( ret==0 )
	    {
	    LvmLvPair p=lvmLvPair();
	    LvmLvIter i=p.begin();
	    while( i!=p.end() )
		{
		i->modifyPeSize( old_pe, peSizeK );
		++i;
		}
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LvmVg::createLv( const string& name, unsigned long long sizeK, unsigned stripe,
                 string& device )
    {
    int ret = 0;
    y2mil("name:" << name << " sizeK:" << sizeK << " stripe:" << stripe);
    checkConsistency();
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 && name.find( "\"\' /\n\t:*?" )!=string::npos )
	{
	ret = LVM_LV_INVALID_NAME;
	}
    if( ret==0 )
	{
	ConstLvmLvPair p = lvmLvPair(LvmLv::notDeleted);
	ConstLvmLvIter i = p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if( i!=p.end() )
	    ret = LVM_LV_DUPLICATE_NAME;
	}
    unsigned long num_le = sizeToLe(sizeK);
    if( stripe>1 )
	num_le = ((num_le+stripe-1)/stripe)*stripe;
    if( ret==0 && free_pe<num_le )
	{
	ret = LVM_LV_NO_SPACE;
	}
    map<string,unsigned long> pe_map;
    if( ret==0 )
	ret = addLvPeDistribution( num_le, stripe, pv, pv_add, pe_map );
    if( ret==0 )
	{
	LvmLv* l = new LvmLv( *this, name, dev + "/" + name, "", num_le, stripe );
	l->setCreated( true );
	l->setPeMap( pe_map );
	device = l->device();
	free_pe -= num_le;
	addToList( l );
	}
    if( ret==0 )
	checkConsistency();
    y2mil("ret:" << ret << " device:" << (ret?"":device));
    return( ret );
    }

int LvmVg::resizeVolume( Volume* v, unsigned long long newSize )
    {
    int ret = 0;
    y2mil("newSizeK:" << newSize << " vol:" << v->name());
    checkConsistency();

    LvmLv * l = dynamic_cast<LvmLv *>(v);

    if (readonly())
    {
	ret = LVM_CHANGE_READONLY;
    }
    else if (l->isSnapshot())
    {
	ret = LVM_LV_IS_SNAPSHOT;
    }
    else if (l->hasSnapshots())
    {
	ret = LVM_LV_HAS_SNAPSHOTS;
    }

    if (ret == 0)
    {
	unsigned long new_le = sizeToLe(newSize);
	if( l->stripes()>1 )
	    new_le = ((new_le+l->stripes()-1)/l->stripes())*l->stripes();
	newSize = new_le*pe_size;
	if( l!=NULL )
	    {
	    if( new_le!=l->getLe() )
		{
		ret = v->canResize( newSize );
		}
	    if( ret==0 && new_le!=l->getLe() )
		{
		map<string,unsigned long> pe_map = l->getPeMap();
		list<Pv> pl = pv;
		list<Pv> pladd = pv_add;
		if( new_le<l->getLe() )
		    {
		    ret = remLvPeDistribution( l->getLe()-new_le, pe_map,
		                               pl, pladd );
		    }
		else
		    {
		    ret = addLvPeDistribution( new_le-l->getLe(), l->stripes(),
		                               pl, pladd, pe_map );
		    }
		if( ret==0 )
		    {
		    free_pe -= new_le-l->getLe();
		    pv = pl;
		    pv_add = pladd;
		    l->setLe( new_le );
		    l->setPeMap( pe_map );
		    if( v->created() )
			l->calcSize();
		    else
			v->setResizedSize( newSize );
		    }
		}
	    }
	else
	    {
	    ret = LVM_CHECK_RESIZE_INVALID_VOLUME;
	    }
    }
    if( ret==0 )
	checkConsistency();
    y2mil("ret:" << ret);
    return( ret );
    }

int LvmVg::removeVolume( Volume* v )
    {
    return( removeLv( v->name() ));
    }

int
LvmVg::removeLv( const string& name )
    {
    int ret = 0;
    y2mil("name:" << name);
    LvmLvIter i;
    checkConsistency();
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	LvmLvPair p = lvmLvPair(LvmLv::notDeleted);
	i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if( i==p.end() )
	    ret = LVM_LV_UNKNOWN_NAME;
	else if (i->hasSnapshots())
	    ret = LVM_LV_HAS_SNAPSHOTS;
	}
    if (ret == 0 && i->isUsedBy())
	{
	if( getStorage()->getRecursiveRemoval() || 
	    getStorage()->isUsedBySingleBtrfs(*i) )
	    ret = getStorage()->removeUsing( i->device(), i->getUsedBy() );
	else
	    ret = LVM_LV_REMOVE_USED_BY;
	}
    if( ret==0 )
	{
	map<string,unsigned long> pe_map = i->getPeMap();
	ret = remLvPeDistribution( i->getLe(), pe_map, pv, pv_add );
	}
    if( ret==0 )
	{
	free_pe += i->getLe();
	if( i->created() )
	    {
	    if( !removeFromList( &(*i) ))
		ret = LVM_LV_NOT_IN_LIST;
	    }
	else
	    i->setDeleted();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LvmVg::changeStripe( const string& name, unsigned long stripe )
    {
    int ret = 0;
    y2mil("name:" << name << " stripe:" << stripe);
    LvmLvIter i;
    checkConsistency();
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	LvmLvPair p = lvmLvPair(LvmLv::notDeleted);
	i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if( i==p.end() )
	    ret = LVM_LV_UNKNOWN_NAME;
	}
    if( i->stripes()!=stripe )
	{
	if( !i->created() )
	    ret = LVM_LV_ALREADY_ON_DISK;
	map<string,unsigned long> pe_map;
	if( ret==0 )
	    {
	    pe_map = i->getPeMap();
	    ret = remLvPeDistribution( i->getLe(), pe_map, pv, pv_add );
	    }
	if( ret==0 )
	    {
	    free_pe += i->getLe();
	    pe_map.clear();
	    unsigned long num_le = sizeToLe(i->sizeK());
	    if( stripe>1 )
		num_le = ((num_le+stripe-1)/stripe)*stripe;
	    ret = addLvPeDistribution( num_le, stripe, pv, pv_add, pe_map );
	    if( ret==0 )
		{
		i->setPeMap( pe_map );
		free_pe -= num_le;
		i->setStripes( stripe );
		}
	    else
		free_pe -= i->getLe();
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LvmVg::changeStripeSize( const string& name, unsigned long long stripeSize )
    {
    int ret = 0;
    y2mil("name:" << name << " stripeSize:" << stripeSize);
    LvmLvIter i;
    checkConsistency();
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	LvmLvPair p = lvmLvPair(LvmLv::notDeleted);
	i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if( i==p.end() )
	    ret = LVM_LV_UNKNOWN_NAME;
	}
    if( ret==0 && !i->created() )
	{
	ret = LVM_LV_ALREADY_ON_DISK;
	}
    if( ret==0 && i->stripes()<=1 )
	{
	ret = LVM_LV_NO_STRIPE_SIZE;
	}
    if( ret==0 )
	{
	i->setStripeSize( stripeSize );
	}
    y2mil("ret:" << ret);
    return( ret );
    }


int
LvmVg::createLvSnapshot(const string& origin, const string& name,
			unsigned long long cowSizeK, string& device)
{
    int ret = 0;
    device.erase();
    y2mil("origin:" << origin << " name:" << name << " cowSizeK:" << cowSizeK );
    checkConsistency();
    if (readonly())
    {
	ret = LVM_CHANGE_READONLY;
    }
    if (ret == 0 && name.find("\"\' /\n\t:*?") != string::npos)
    {
	ret = LVM_LV_INVALID_NAME;
    }
    int stripe = 1;
    if (ret == 0)
    {
	LvmLvPair p = lvmLvPair(LvmLv::notDeleted);
	LvmLvIter i = p.begin();
	while (i != p.end() && i->name() != origin)
	    ++i;
	if (i == p.end())
	    ret = LVM_LV_UNKNOWN_ORIGIN;
	else
	    stripe = i->stripes();
    }
    if (ret == 0)
    {
	LvmLvPair p = lvmLvPair(LvmLv::notDeleted);
	LvmLvIter i = p.begin();
	while (i != p.end() && i->name() != name)
	    ++i;
	if (i != p.end())
	    ret = LVM_LV_DUPLICATE_NAME;
    }
    unsigned long num_le = sizeToLe(cowSizeK);
    if( stripe>1 )
	num_le = ((num_le+stripe-1)/stripe)*stripe;
    if (ret == 0 && free_pe < num_le)
    {
	ret = LVM_LV_NO_SPACE;
    }
    map<string, unsigned long> pe_map;
    if (ret == 0)
	ret = addLvPeDistribution(num_le, stripe, pv, pv_add, pe_map);
    if (ret == 0)
    {
	LvmLv* l = new LvmLv(*this, name, dev + "/" + name, origin, num_le, stripe);
	l->setCreated(true);
	l->setPeMap(pe_map);
	device = l->device();
	free_pe -= num_le;
	addToList(l);
    }
    if (ret == 0)
	checkConsistency();
    y2mil("ret:" << ret << " device:" << device);
    return ret;
}


int
LvmVg::removeLvSnapshot(const string& name)
{
    int ret = 0;
    y2mil("name:" << name);
    if( ret==0 )
    {
	LvmLvPair p = lvmLvPair(LvmLv::notDeleted);
	LvmLvIter i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if (i==p.end())
	    ret = LVM_LV_UNKNOWN_NAME;
	else if (!i->isSnapshot())
	    ret = LVM_LV_NOT_SNAPSHOT;
    }
    if (ret == 0)
    {
	ret = removeLv(name);
    }
    y2mil("ret:" << ret);
    return ret;
}


int
LvmVg::getLvSnapshotState(const string& name, LvmLvSnapshotStateInfo& info)
{
    int ret = 0;
    y2mil("name:" << name);
    LvmLvIter i;
    checkConsistency();
    if (ret == 0)
    {
	LvmLvPair p = lvmLvPair(LvmLv::notDeleted);
	i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if (i == p.end())
	    ret = LVM_LV_UNKNOWN_NAME;
	else if (!i->isSnapshot())
	    ret = LVM_LV_NOT_SNAPSHOT;
    }
    if (ret == 0 && i->created())
    {
	ret = LVM_LV_NOT_ON_DISK;
    }
    if (ret == 0)
    {
	i->getState(info);
    }
    y2mil("ret:" << ret);
    return ret;
}


void LvmVg::getVgData( const string& name, bool exists )
    {
    y2mil("name:" << name);
    SystemCmd c(VGDISPLAYBIN " --units k -v " + quote(name));
    unsigned cnt = c.numLines();
    unsigned i = 0;
    string line;
    string tmp;
    string::size_type pos;
    while( i<cnt )
	{
	line = c.getLine( i++ );
	if( line.find( "Volume group" )!=string::npos )
	    {
	    line = c.getLine( i++ );
	    while( line.find( "Logical volume" )==string::npos &&
		   line.find( "Physical volume" )==string::npos &&
		   i<cnt )
		{
		line.erase( 0, line.find_first_not_of( app_ws ));
		if( line.find( "Format" ) == 0 )
		    {
		    bool lv1 = extractNthWord( 1, line )=="lvm1";
		    if( exists && lv1 != lvm1 )
			y2war("inconsistent lvm1 my:" << lvm1 << " lvm:" << lv1);
		    lvm1 = lv1;
		    }
		else if( line.find( "PE Size" ) == 0 )
		    {
		    unsigned long long pes;
		    tmp = extractNthWord( 2, line );
		    pos = tmp.find( '.' );
		    if( pos!=string::npos )
			tmp.erase( pos );
		    tmp >> pes;
		    if( exists && pes != pe_size )
			y2war("inconsistent pe_size my:" << pe_size << " lvm:" << pes);
		    pe_size = pes;
		    calcSize();
		    }
		else if( line.find( "Total PE" ) == 0 )
		    {
		    extractNthWord( 2, line ) >> num_pe;
		    calcSize();
		    }
		else if( line.find( "Free  PE" ) == 0 )
		    {
		    extractNthWord( 4, line ) >> free_pe;
		    }
		else if( line.find( "VG Status" ) == 0 )
		    {
		    status = extractNthWord( 2, line );
		    }
		else if( line.find( "VG Access" ) == 0 )
		    {
		    ronly = extractNthWord( 2, line, true ).find( "write" ) == string::npos;
		    }
		else if( line.find( "VG UUID" ) == 0 )
		    {
		    uuid = extractNthWord( 2, line );
		    }
		line = c.getLine( i++ );
		}
	    string vname;
	    string origin;
	    string uuid;
	    string status;
	    string allocation;
	    unsigned long num_le = 0;
	    unsigned long num_cow_le = 0;
	    bool readOnly = false;
	    while( line.find( "Physical volume" )==string::npos && i<cnt )
		{
		line.erase( 0, line.find_first_not_of( app_ws ));
		if( line.find( "LV Name" ) == 0 )
		    {
		    if( !vname.empty() )
		    {
			addLv(origin.empty() ? num_le : num_cow_le, vname, origin, uuid, status, allocation,
			      readOnly);
		    }
		    vname = extractNthWord( 2, line );
		    if( (pos=vname.rfind( "/" ))!=string::npos )
			vname.erase( 0, pos+1 );
		    }
		else if( line.find( "LV Write Access" ) == 0 )
		    {
		    readOnly = extractNthWord( 3, line, true ).find( "only" ) != string::npos;
		    }
		else if (line.find("LV snapshot status") == 0)
		{
		    if (line.find("destination for") != string::npos)
		    {
			origin = extractNthWord(6, line, true);
			string::size_type pos = origin.find("/", 5);
			if (pos != string::npos)
			    origin.erase(0, pos + 1);
		    }
		}
		else if( line.find( "LV Status" ) == 0 )
		    {
		    status = extractNthWord( 2, line, true );
		    }
		else if( line.find( "Current LE" ) == 0 )
		    {
		    extractNthWord( 2, line ) >> num_le;
		    }
		else if (line.find( "COW-table LE" ) == 0)
		{
		    extractNthWord( 2, line ) >> num_cow_le;
		}
		else if( line.find( "Allocation" ) == 0 )
		    {
		    allocation = extractNthWord( 1, line );
		    }
		else if( line.find( "LV UUID" ) == 0 )
		    {
		    uuid = extractNthWord( 2, line );
		    }
		line = c.getLine( i++ );
		}
	    if( !vname.empty() )
	    {
		addLv(origin.empty() ? num_le : num_cow_le, vname, origin, uuid, status, allocation, readOnly);
	    }
	    Pv *p = new Pv;
	    while( i<cnt )
		{
		line.erase( 0, line.find_first_not_of( app_ws ));
		if( line.find( "PV Name" ) == 0 )
		    {
		    if( !p->device.empty() )
			{
			addPv( p );
			}
		    p->device = extractNthWord( 2, line );

		    const Volume* v;
		    if (getStorage()->findVolume(p->device, v))
		    {
			p->device = v->device();
			p->dmcryptDevice = v->dmcryptDevice();
		    }
		    
		    }
		else if( line.find( "PV UUID" ) == 0 )
		    {
		    p->uuid = extractNthWord( 2, line );
		    }
		else if( line.find( "PV Status" ) == 0 )
		    {
		    p->status = extractNthWord( 2, line );
		    }
		else if( line.find( "Total PE" ) == 0 )
		    {
		    extractNthWord( 5, line ) >> p->num_pe;
		    extractNthWord( 7, line ) >> p->free_pe;
		    calcSize();
		    }
		line = c.getLine( i++ );
		}
	    if( !p->device.empty() )
		{
		addPv( p );
		}
	    delete p;
	    }
	}
    LvmLvPair p=lvmLvPair(lvDeleted);
    for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
	{
	//cout << "Deleted:" << *i << endl;
	map<string,unsigned long> pe_map = i->getPeMap();
	remLvPeDistribution( i->getLe(), pe_map, pv, pv_add );
	free_pe += i->getLe();
	}
    p=lvmLvPair(lvCreated);
    for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
	{
	//cout << "Created:" << *i << endl;
	map<string,unsigned long> pe_map;
	if( addLvPeDistribution( i->getLe(), i->stripes(), pv, pv_add,
	                         pe_map ) == 0 )
	    i->setPeMap( pe_map );
	free_pe -= i->getLe();
	}
    p=lvmLvPair(lvResized);
    for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
	{
	//cout << "Resized:" << *i << endl;
	map<string,unsigned long> pe_map = i->getPeMap();
	long size_diff = i->getLe() - sizeToLe(i->origSizeK());
	if( size_diff>0 )
	    {
	    if( addLvPeDistribution( size_diff, i->stripes(), pv, pv_add,
				     pe_map ) == 0 )
		i->setPeMap( pe_map );
	    }
	else if( size_diff<0 )
	    {
	    if( remLvPeDistribution( -size_diff, pe_map, pv, pv_add )==0 )
		i->setPeMap( pe_map );
	    }
	free_pe -= size_diff;
	}
    }

void LvmVg::addLv(unsigned long& le, string& name, string& origin, string& uuid,
		  string& status, string& alloc, bool& ro)
    {
    y2mil("addLv:" << name);
    LvmLvPair p=lvmLvPair(lvNotDeletedCreated);
    LvmLvIter i=p.begin();
    while( i!=p.end() && i->name()!=name )
	{
	++i;
	}
    y2mil("addLv exists " << (i!=p.end()));
    if( i!=p.end() )
    {
	if( !lvResized( *i ))
	    i->setLe( le );
	if( i->created() )
	    {
	    i->setCreated( false );
	    i->calcSize();
	    }
	i->setUuid( uuid );
	i->setStatus( status );
	i->setOrigin( origin );
	i->setAlloc( alloc );
	i->getTableInfo();
	i->updateMajorMinor();
	i->setReadonly(ro);
	}
    else
	{
	p=lvmLvPair(lvNotCreated);
	i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    {
	    ++i;
	    }
	y2mil("addLv exists deleted " << (i!=p.end()));
	if( i==p.end() )
	    {
	    LvmLv *n = new LvmLv( *this, name, dev + "/" + name, origin, le, uuid, status, alloc );
	    if( ro )
		n->setReadonly();
	    if( !n->inactive() )
		addToList( n );
	    else
		{
		y2mil("inactive Lv " << name);
		delete n;
		}
	    }
	}
    name = origin = uuid = status = alloc = "";
    le = 0;
    ro = false;
    }


void LvmVg::addPv( Pv*& p )
    {
    PeContainer::addPv( *p );
    if( !deleted() &&
        find( pv_remove.begin(), pv_remove.end(), *p )==pv_remove.end() )
	getStorage()->setUsedBy(p->device, UB_LVM, device());
    delete p;
    p = new Pv;
    }


void
LvmVg::getToCommit(CommitStage stage, list<const Container*>& col, list<const Volume*>& vol) const
{
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    Container::getToCommit( stage, col, vol );
    if( stage==DECREASE )
        {
	if( !pv_remove.empty() &&
	    find( col.begin(), col.end(), this )==col.end() )
	    {
	    col.push_back( this );
	    }
        }
    else if( stage==INCREASE )
        {
	if( !pv_add.empty() &&
	    find( col.begin(), col.end(), this )==col.end() )
	    {
	    col.push_back( this );
	    }
        }
    if( col.size()!=oco || vol.size()!=ovo )
	y2mil("stage:" << stage << " col:" << col.size() << " vol:" << vol.size());
}


int LvmVg::commitChanges( CommitStage stage )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = 0;
    switch( stage )
	{
	case DECREASE:
	    if( deleted() )
		{
		ret = doRemoveVg();
		}
	    else if( !pv_remove.empty() )
		{
		ret = doReduceVg();
		}
	    else
		ret = LVM_COMMIT_NOTHING_TODO;
	    break;
	case INCREASE:
	    if( created() )
		{
		ret = doCreateVg();
		}
	    else if( !pv_add.empty() )
		{
		ret = doExtendVg();
		}
	    else
		ret = LVM_COMMIT_NOTHING_TODO;
	    break;
	default:
	    ret = LVM_COMMIT_NOTHING_TODO;
	    break;
	}
    y2mil("ret:" << ret);
    return( ret );
    }


void
LvmVg::getCommitActions(list<commitAction>& l) const
    {
    Container::getCommitActions( l );
    y2mil( "Container::getCommitActions:" << l );
    if( deleted() )
	{
	l.push_back(commitAction(DECREASE, staticType(), removeText(false), this, true));
	}
    else if( created() )
	{
	l.push_front(commitAction(INCREASE, staticType(), createText(false), this, true));
	}
    else
	{
	if( !pv_add.empty() )
	    for( list<Pv>::const_iterator i=pv_add.begin(); i!=pv_add.end();
	         ++i )
		l.push_back(commitAction(INCREASE, staticType(),
					 extendText(false, i->device), this, true));
	if( !pv_remove.empty() )
	    for( list<Pv>::const_iterator i=pv_remove.begin();
	         i!=pv_remove.end(); ++i )
		l.push_back(commitAction(DECREASE, staticType(),
					 reduceText(false, i->device), this, false));
	}
    }


Text
LvmVg::removeText(bool doing) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. system),
        txt = sformat( _("Removing volume group %1$s"), name().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. system),
        txt = sformat( _("Remove volume group %1$s"), name().c_str() );
        }
    return( txt );
    }

Text
LvmVg::createText(bool doing) const
    {
    Text txt;
    if( doing )
        {
	// displayed text during action
	// %1$s is replaced by a name (e.g. system)
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
	txt = sformat(_("Creating volume group %1$s (%2$s) from %3$s"), name().c_str(),
		      sizeString().c_str(), addList().c_str());
        }
    else
        {
	// displayed text before action
	// %1$s is replaced by a name (e.g. system)
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
	txt = sformat(_("Create volume group %1$s (%2$s) from %3$s"), name().c_str(),
		      sizeString().c_str(), addList().c_str());
        }
    return( txt );
    }

Text
LvmVg::extendText(bool doing, const string& dev) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Extending volume group %1$s with %2$s"), name().c_str(),
	               dev.c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Extend volume group %1$s with %2$s"), name().c_str(),
	               dev.c_str() );
        }
    return( txt );
    }


Text
LvmVg::reduceText(bool doing, const string& dev) const
    {
    Text txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Reducing volume group %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Reduce volume group %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    return( txt );
    }


void
LvmVg::activate(bool val)
{
	if (getenv("LIBSTORAGE_NO_LVM") != NULL)
	    return;

    y2mil("old active:" << active << " val:" << val);

    if (active != val)
    {
	SystemCmd c;
	if (val)
	{
	    Dm::activate(true);
	    c.execute(VGSCANBIN " --mknodes");
	    c.execute(VGCHANGEBIN " -a y");
	}
	else
	{
	    c.execute(VGCHANGEBIN " -a n");
	}
	active = val;
    }

    Storage::waitForDevice();
}


    list<string>
    LvmVg::getVgs()
    {
	list<string> l;

	SystemCmd c(VGSBIN " --noheadings -o vg_name");
	if (c.retcode() == 0 && !c.stdout().empty())
	{
	    active = true;

	    for (vector<string>::const_iterator it = c.stdout().begin(); it != c.stdout().end(); ++it)
		l.push_back(boost::trim_copy(*it, locale::classic()));
	}

	y2mil("detected vgs " << l);
	return l;
    }


int
LvmVg::doCreateVg()
    {
    y2mil("Vg:" << name());
    int ret = 0;
    if( created() )
	{
	checkConsistency();
	if( !silent )
	    {
	    getStorage()->showInfoCb(createText(true));
	    }
	string devices;
	if( pv_add.size()+pv.size()-pv_remove.size()<=0 )
	    ret = LVM_VG_HAS_NONE_PV;
	list<Pv>::const_iterator p = pv_add.begin();
	while( ret==0 && p!=pv_add.end() )
	    {
	    if( !devices.empty() )
		devices += " ";
	    devices += quote(p->realDevice());
	    ret = doCreatePv(*p);
	    ++p;
	    }
	if( ret==0 )
	    {
	    if (access(device().c_str(), R_OK) == 0)
		{
		SystemCmd c("find " + device() + " -type l | xargs -r rm");
		rmdir(device().c_str());
		}
	    string cmd = VGCREATEBIN " " + instSysString() + metaString() +
		"-s " + decString(pe_size) + "k " + quote(name()) + " " + devices;
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		{
		ret = LVM_VG_CREATE_FAILED;
		}
	    }
	if( ret==0 )
	    {
	    setCreated( false );
	    getVgData( name() );
	    if( !pv_add.empty() )
		{
		y2err( "still added:" << pv_add );
		pv_add.clear();
		ret = LVM_PV_STILL_ADDED;
		}
	    checkConsistency();
	    checkCreateConstraints();
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LvmVg::doRemoveVg()
    {
    y2mil("Vg:" << name());
    int ret = 0;
    if( deleted() )
	{
	if( !active )
	    activate(true);
	if( !silent )
	    {
	    getStorage()->showInfoCb(removeText(true));
	    }
	checkConsistency();
	string cmd = VGREMOVEBIN " " + quote(name());
	SystemCmd c( cmd );
	if( c.retcode()!=0 )
	    {
	    ret = LVM_VG_REMOVE_FAILED;
	    setExtError( c );
	    }
	if( ret==0 )
	    {
	    setDeleted( false );
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LvmVg::doExtendVg()
    {
    y2mil("Vg:" << name());
    y2mil( "this:" << *this );
    int ret = 0;
    if( !active )
	activate(true);
    list<Pv> devs = pv_add;
    list<Pv>::const_iterator d = devs.begin();
    while( ret==0 && d!=devs.end() )
	{
	checkConsistency();
	if( !silent )
	    {
	    getStorage()->showInfoCb(extendText(true, d->device));
	    }
	ret = doCreatePv(*d);
	if( ret==0 )
	    {
	    string cmd = VGEXTENDBIN " " + instSysString() + quote(name()) + " " + quote(d->realDevice());
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		{
		ret = LVM_VG_EXTEND_FAILED;
		setExtError( c );
		}
	    }
	if( ret==0 )
	    {
	    getVgData( name() );
	    checkConsistency();
	    }
	list<Pv>::iterator p = find(pv_add.begin(), pv_add.end(), d->device);
	if( p!=pv_add.end() )
	    {
	    pv_add.erase( p );
	    if( ret==0 )
		ret = LVM_PV_STILL_ADDED;
	    }
	++d;
	}
    if (!devs.empty())
	checkCreateConstraints();
    y2mil( "this:" << *this );
    y2mil("ret:" << ret);
    return ret;
    }

int
LvmVg::doReduceVg()
    {
    y2mil("Vg:" << name());
    y2mil( "this:" << *this );
    int ret = 0;
    if( !active )
	activate(true);
    list<Pv> devs = pv_remove;
    list<Pv>::const_iterator d = devs.begin();
    while( ret==0 && d!=devs.end() )
	{
	checkConsistency();
	if( !silent )
	    {
	    getStorage()->showInfoCb(reduceText(true, d->device));
	    }
	string cmd = VGREDUCEBIN " " + instSysString() + quote(name()) + " " + quote(d->realDevice());
	SystemCmd c( cmd );
	if( c.retcode()!=0 )
	    {
	    ret = LVM_VG_REDUCE_FAILED;
	    setExtError( c );
	    }
	if( ret==0 )
	    {
	    getVgData( name() );
	    checkConsistency();
	    }
	list<Pv>::iterator p = find(pv_remove.begin(), pv_remove.end(), d->device);
	if( p!=pv_remove.end() )
	    pv_remove.erase( p );
	else if( ret==0 )
	    ret = LVM_PV_REMOVE_NOT_FOUND;
	++d;
	}
    y2mil( "this:" << *this );
    y2mil("ret:" << ret);
    return ret;
    }

int
LvmVg::doCreate( Volume* v )
    {
    y2mil("Vg:" << name() << " name:" << v->name());
    LvmLv * l = dynamic_cast<LvmLv *>(v);
    int ret = 0;
    if( l != NULL )
	{
	if( !active )
	    activate(true);
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->createText(true) );
	    }
	checkConsistency();
	string cmd = LVCREATEBIN " " + instSysString() + " -l " + decString(l->getLe());
	if (l->getOrigin().empty())
	{
	    if( l->stripes()>1 )
	    {
		cmd += " -i " + decString(l->stripes());
		if( l->stripeSize()>0 )
		    cmd += " -I " + decString(l->stripeSize());
	    }
	    cmd += " --name " + quote(l->name());
	    cmd += " " + quote(name());
	}
	else
	{
	    cmd += " --snapshot";
	    cmd += " --name " + quote(l->name());
	    cmd += " " + quote(name() + "/" + l->getOrigin());
	}
	SystemCmd c( cmd );
	if( c.retcode()!=0 )
	    {
	    ret = LVM_LV_CREATE_FAILED;
	    setExtError( c );
	    }
	if( ret==0 )
	    {
	    Storage::waitForDevice(l->device());
	    l->setCreated(false);
	    getVgData( name() );
	    checkConsistency();
	    }
	}
    else
	ret = LVM_CREATE_LV_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }

int LvmVg::doRemove( Volume* v )
    {
    y2mil("Vg:" << name() << " name:" << v->name());
    y2mil( "this:" << *this );
    LvmLv * l = dynamic_cast<LvmLv *>(v);
    int ret = 0;
    if( l != NULL )
	{
	y2mil( "lv:" << *l );
	if( !active )
	    activate(true);
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->removeText(true) );
	    }
	checkConsistency();
	ret = v->prepareRemove();
	if( ret==0 )
	    {
	    string cmd = LVREMOVEBIN " -f " + instSysString() + " " + quote(l->device());
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		{
		ret = LVM_LV_REMOVE_FAILED;
		setExtError( c );
		}
	    }
	if( ret==0 )
	    {
	    string tbl = lvm2()?"lvm2":"lvm";
	    getStorage()->removeDmTable( tbl+'|'+name()+'|'+l->name() );
	    if( !removeFromList( l ) )
		ret = LVM_LV_NOT_IN_LIST;
	    getVgData( name() );
	    checkConsistency();
	    }
	}
    else
	ret = LVM_REMOVE_LV_INVALID_VOLUME;
    y2mil( "this:" << *this );
    y2mil("ret:" << ret);
    return( ret );
    }

int LvmVg::doResize( Volume* v )
    {
    y2mil("Vg:" << name() << " name:" << v->name());
    LvmLv * l = dynamic_cast<LvmLv *>(v);
    int ret = 0;
    if( l != NULL )
	{
	if( !active )
	    activate(true);
	FsCapabilities caps;
	bool remount = false;
	unsigned long new_le = l->getLe();
	unsigned long old_le = sizeToLe(v->origSizeK());
	getStorage()->getFsCapabilities( l->getFs(), caps );
	if( !silent && old_le!=new_le )
	    {
	    getStorage()->showInfoCb( l->resizeText(true) );
	    }
	checkConsistency();
	if( v->isMounted() &&
	    ((old_le>new_le&&!caps.isReduceableWhileMounted)||
	     (old_le<new_le&&!caps.isExtendableWhileMounted)))
	    {
	    ret = v->umount();
	    if( ret==0 )
		remount = true;
	    }
	if( ret==0 && old_le>new_le && l->getFs()!=FSNONE )
	    ret = v->resizeFs();
	if( ret==0 && old_le>new_le )
	    {
	    string cmd = LVREDUCEBIN " -f " + instSysString() +
		" -l -" + decString(old_le-new_le) + " " + quote(l->device());
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		{
		ret = LVM_LV_RESIZE_FAILED;
		setExtError( c );
		}
	    }
	if( ret==0 && old_le<new_le )
	    {
	    string cmd = LVEXTENDBIN " " + instSysString() +
		" -l +" + decString(new_le-old_le) + " " + quote(l->device());
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		{
		ret = LVM_LV_RESIZE_FAILED;
		setExtError( c );
		}
	    }
	if( ret==0 && old_le<new_le && l->getFs()!=FSNONE )
	    ret = v->resizeFs();
	if( old_le!=new_le )
	    l->calcSize();
	if( ret==0 && remount )
	    ret = v->mount();
	if( ret==0 )
	    {
	    getVgData( name() );
	    checkConsistency();
	    }
	}
    else
	ret = LVM_RESIZE_LV_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }


string LvmVg::metaString() const
    {
    return( (lvm1)?"-M1 ":"-M2 " );
    }


string LvmVg::instSysString() const
    {
    string ret;
    if( getStorage()->instsys() )
	ret = "-A n ";
    return( ret );
    }


    int
    LvmVg::doCreatePv(const Pv& pv)
    {
    int ret = 0;
    y2mil("device:" << pv.device << " realDevice:" << pv.realDevice());
    getStorage()->unaccessDev(pv.device);
    SystemCmd c;
    string cmd = MDADMBIN " --zero-superblock " + quote(pv.realDevice());
    c.execute( cmd );
    getStorage()->removeDmTableTo(pv.realDevice());
    if (getStorage()->isDisk(pv.realDevice()))
	{
	cmd = PARTEDCMD + quote(pv.realDevice()) + " mklabel msdos";
	c.execute( cmd );
	}
    cmd = "echo y | " PVCREATEBIN " -ff " + metaString() + quote(pv.realDevice());
    c.execute( cmd );
    if( c.retcode()!=0 )
	{
	ret = LVM_CREATE_PV_FAILED;
	setExtError( c );
	}
    y2mil("ret:" << ret);
    return ret;
    }


void LvmVg::normalizeDmDevices()
    {
    y2mil( "normalizeDmDevices:" << name() );
    string dm = decString(Dm::dmMajor());
    for( list<Pv>::iterator i=pv.begin(); i!=pv.end(); ++i )
	{
	if( i->device.find( "/dev/dm-" )==0 )
	    {
	    string dev = getDeviceByNumber( dm+":"+i->device.substr( 8 ) );
	    if( !dev.empty() )
		{
		y2mil( "dev:" << i->device << " normal dev:" << dev );
		if( getStorage()->knownDevice( dev ) )
		    {
		    y2mil( "replace " << i->device << " with " << dev );
		    i->device = dev;
		    }
		}
	    }
	}
    }

void LvmVg::getInfo( LvmVgInfo& tinfo ) const
    {
    info.sizeK = sizeK();
    info.peSizeK = peSize();
    info.peCount = peCount();
    info.peFree = peFree();
    info.lvm2 = lvm2();
    info.create = created();
    info.uuid = uuid;
    info.devices.clear();
    list<Pv>::const_iterator i=pv.begin();
    while( i!=pv.end() )
	{
	if( !info.devices.empty() )
	    info.devices += ' ';
	info.devices += i->device;
	++i;
	}
    info.devices_add.clear();
    i=pv_add.begin();
    while( i!=pv_add.end() )
	{
	if( !info.devices_add.empty() )
	    info.devices_add += ' ';
	info.devices_add += i->device;
	++i;
	}
    info.devices_rem.clear();
    i=pv_remove.begin();
    while( i!=pv_remove.end() )
	{
	if( !info.devices_rem.empty() )
	    info.devices_rem += ' ';
	info.devices_rem += i->device;
	++i;
	}
    y2mil( "device:" << info.devices );
    if( !info.devices_add.empty() )
	y2mil( " devices_add:" << info.devices_add );
    if( !info.devices_rem.empty() )
        y2mil( " devices_rem:" << info.devices_rem );
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const LvmVg& d )
    {
    s << dynamic_cast<const PeContainer&>(d);
    s << " status:" << d.status;
    if( d.lvm1 )
      s << " lvm1";
    s << " UUID:" << d.uuid;
    return( s );
    }


    void
    LvmVg::logDifference(std::ostream& log, const LvmVg& rhs) const
    {
	PeContainer::logDifference(log, rhs);

	logDiff(log, "status", status, rhs.status);
	logDiff(log, "lvm1", lvm1, rhs.lvm1);
	logDiff(log, "uuid", uuid, rhs.uuid);
    }


    void
    LvmVg::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
	const LvmVg& rhs = dynamic_cast<const LvmVg&>(rhs_c);

	logDifference(log, rhs);
	log << endl;

	ConstLvmLvPair pp = lvmLvPair();
	ConstLvmLvPair pc = rhs.lvmLvPair();
	logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


bool LvmVg::equalContent( const Container& rhs ) const
    {
    const LvmVg * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const LvmVg*>(&rhs);
    if( ret && p )
	ret = PeContainer::equalContent(*p,false) &&
	    status==p->status && uuid==p->uuid && lvm1==p->lvm1;
    if( ret && p )
	{
	ConstLvmLvPair pp = lvmLvPair();
	ConstLvmLvPair pc = p->lvmLvPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
	}
    return( ret );
    }


    void
    LvmVg::logData(const string& Dir) const
    {
	string fname(Dir + "/lvmvg_" + name() + ".info.tmp");

	XmlFile xml;
	xmlNode* node = xmlNewNode("volume_group");
	xml.setRootElement(node);
	saveData(node);
	xml.save(fname);

	getStorage()->handleLogFile( fname );
    }


bool LvmVg::active = false;

}
