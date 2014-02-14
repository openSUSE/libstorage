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

#include "storage/LvmVg.h"
#include "storage/LvmLv.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/SystemInfo.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    CmdVgs::CmdVgs(bool do_probe)
    {
	if (do_probe)
	    probe();
    }


    void
    CmdVgs::probe()
    {
	SystemCmd c(VGSBIN " --noheadings --unbuffered --options vg_name");
	if (c.retcode() == 0 && !c.stdout().empty())
	    parse(c.stdout());
    }


    void
    CmdVgs::parse(const vector<string>& lines)
    {
	vgs.clear();

	for (const string& line : lines)
	    vgs.push_back(boost::trim_copy(line, locale::classic()));

	y2mil(*this);
    }


    std::ostream& operator<<(std::ostream& s, const CmdVgs& cmdvgs)
    {
	s << "vgs:" << cmdvgs.vgs;

	return s;
    }


    CmdVgdisplay::CmdVgdisplay(const string& name, bool do_probe)
	: name(name), pe_size(0), num_pe(0), free_pe(0), read_only(false), lvm1(false)
    {
	if (do_probe)
	    probe();
    }


    void
    CmdVgdisplay::probe()
    {
	SystemCmd c(VGDISPLAYBIN " --units k --verbose " + quote(name));
	if (c.retcode() == 0 && !c.stdout().empty())
	    parse(c.stdout());
    }


    void
    CmdVgdisplay::parse(const vector<string>& lines)
    {
	vector<string>::const_iterator it = lines.begin();

	while (it != lines.end())
	{
	    string line = *it++;
	    if (boost::contains(line, "Volume group"))
	    {
		line = *it++;
		while (!boost::contains(line, "Logical volume") &&
		       !boost::contains(line, "Physical volume") &&
		       it != lines.end())
		{
		    line.erase(0, line.find_first_not_of(app_ws));
		    if (boost::starts_with(line, "Format"))
		    {
			lvm1 = extractNthWord(1, line) == "lvm1";
		    }
		    else if (boost::starts_with(line, "PE Size"))
		    {
			string tmp = extractNthWord(2, line);
			string::size_type pos = tmp.find('.');
			if (pos!=string::npos)
			    tmp.erase(pos);
			tmp >> pe_size;
		    }
		    else if (boost::starts_with(line, "Total PE"))
		    {
			extractNthWord(2, line) >> num_pe;
		    }
		    else if (boost::starts_with(line, "Free  PE"))
		    {
			extractNthWord(4, line) >> free_pe;
		    }
		    else if (boost::starts_with(line, "VG Status"))
		    {
			status = extractNthWord(2, line);
		    }
		    else if (boost::starts_with(line, "VG Access"))
		    {
			read_only = !boost::contains(extractNthWord(2, line, true), "write");
		    }
		    else if (boost::starts_with(line, "VG UUID"))
		    {
			uuid = extractNthWord(2, line);
		    }
		    line = *it++;
		}

		LvEntry lv_entry;
		lv_entry.clear();
		while (!boost::contains(line, "Physical volume") &&
		       it != lines.end())
		{
		    line.erase(0, line.find_first_not_of(app_ws));
		    if (boost::starts_with(line, "LV Name"))
		    {
			if (!lv_entry.name.empty())
			    lv_entries.push_back(lv_entry);
			lv_entry.clear();
			lv_entry.name = extractNthWord(2, line);
			string::size_type pos = lv_entry.name.rfind("/");
			if (pos != string::npos)
			    lv_entry.name.erase(0, pos + 1);
		    }
		    else if (boost::starts_with(line, "LV Write Access"))
		    {
			lv_entry.read_only = boost::contains(extractNthWord(3, line, true), "only");
		    }
		    else if (boost::starts_with(line, "LV snapshot status"))
		    {
			if (boost::contains(line, "destination for"))
			{
			    lv_entry.origin = extractNthWord(6, line, true);
			    string::size_type pos = lv_entry.origin.find("/", 5);
			    if (pos != string::npos)
				lv_entry.origin.erase(0, pos + 1);
			}
		    }
		    else if (boost::starts_with(line, "LV Status"))
		    {
			lv_entry.status = extractNthWord(2, line, true);
		    }
		    else if (boost::starts_with(line, "Current LE"))
		    {
			extractNthWord(2, line) >> lv_entry.num_le;
		    }
		    else if (boost::starts_with(line, "COW-table LE"))
		    {
			extractNthWord(2, line) >> lv_entry.num_cow_le;
		    }
		    else if (boost::starts_with(line, "LV UUID"))
		    {
			lv_entry.uuid = extractNthWord(2, line);
		    }
		    else if (boost::starts_with(line, "LV Pool metadata"))
		    {
			lv_entry.pool = true;
		    }
		    else if (boost::starts_with(line, "LV Pool chunk size"))
                    {
			extractNthWord(4, line) >> lv_entry.pool_chunk;
                    }
		    else if (boost::starts_with(line, "LV Pool name"))
                    {
			lv_entry.used_pool = extractNthWord(3, line);
                    }
		    line = *it++;
		}
		if (!lv_entry.name.empty())
		    lv_entries.push_back(lv_entry);

		PvEntry pv_entry;
		pv_entry.clear();
		while (it != lines.end())
		{
		    line.erase(0, line.find_first_not_of(app_ws));
		    if (boost::starts_with(line, "PV Name"))
		    {
			if (!pv_entry.device.empty())
			    pv_entries.push_back(pv_entry);
			pv_entry.clear();
			pv_entry.device = extractNthWord(2, line);
		    }
		    else if (boost::starts_with(line, "PV UUID"))
		    {
			pv_entry.uuid = extractNthWord(2, line);
		    }
		    else if (boost::starts_with(line, "PV Status"))
		    {
			pv_entry.status = extractNthWord(2, line);
		    }
		    else if (boost::starts_with(line, "Total PE"))
		    {
			extractNthWord(5, line) >> pv_entry.num_pe;
			extractNthWord(7, line) >> pv_entry.free_pe;
		    }
		    line = *it++;
		}
		if (!pv_entry.device.empty())
		    pv_entries.push_back(pv_entry);
	    }
	}

	y2mil(*this);
    }


    void
    CmdVgdisplay::LvEntry::clear()
    {
	name.clear();
	uuid.clear();
	status.clear();
	origin.clear();
	used_pool.clear();
	num_le = 0;
	num_cow_le = 0;
	pool_chunk = 0;
	read_only = false;
	pool = false;
    }


    void
    CmdVgdisplay::PvEntry::clear()
    {
	 device.clear();
	 uuid.clear();
	 status.clear();
	 num_pe = 0;
	 free_pe = 0;
    }


    std::ostream& operator<<(std::ostream& s, const CmdVgdisplay& cmdvgdisplay)
    {
	s << "name:" << cmdvgdisplay.name << " uuid:" << cmdvgdisplay.uuid
	  << " status:" << cmdvgdisplay.status << " pe_size:" << cmdvgdisplay.pe_size
	  << " num_pe:" << cmdvgdisplay.num_pe << " free_pe:" << cmdvgdisplay.free_pe;

	if (cmdvgdisplay.read_only)
	    s << " read_only";

	if (cmdvgdisplay.lvm1)
	    s << " lvm1";

	s << endl;

	for (const CmdVgdisplay::LvEntry& lv_entry : cmdvgdisplay.lv_entries)
	    s << "lv " << lv_entry << endl;

	for (const CmdVgdisplay::PvEntry& pv_entry : cmdvgdisplay.pv_entries)
	    s << "pv " << pv_entry << endl;

	return s;
    }


    std::ostream& operator<<(std::ostream& s, const CmdVgdisplay::LvEntry& lv_entry)
    {
	s << "name:" << lv_entry.name << " uuid:" << lv_entry.uuid
	  << " status:" << lv_entry.status;

	if (!lv_entry.origin.empty())
	    s << " origin:" << lv_entry.origin;

	if (!lv_entry.used_pool.empty())
	    s << " used_pool:" << lv_entry.used_pool;

	s << " num_le:" << lv_entry.num_le;

	if (lv_entry.num_cow_le != 0)
	    s << " num_cow_le:" << lv_entry.num_cow_le;

	if (lv_entry.pool_chunk != 0)
	    s << " pool_chunk:" << lv_entry.pool_chunk;

	if (lv_entry.read_only)
	    s << " read_only";

	if (lv_entry.pool)
	    s << " pool";

	return s;
    }


    std::ostream& operator<<(std::ostream& s, const CmdVgdisplay::PvEntry& pv_entry)
    {
	s << "device:" << pv_entry.device << " uuid:" << pv_entry.uuid
	  << " status:" << pv_entry.status << " num_pe:" << pv_entry.num_pe
	  << " free_pe:" << pv_entry.free_pe;

	return s;
    }


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
	getVgData(name, systeminfo, false);
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
    if( ret==0 )
	{
	LvmLvPair p = lvmLvPair(LvmLv::notDeleted);
	for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
            {
            if( i->isSnapshot() && !created() )
                ret = removeLv( i->name() );
            }
	for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
            {
            if( !i->isSnapshot() && !i->isPool() )
		{
	        if (i->isUsedBy())
		    getStorage()->removeUsing( i->device(), i->getUsedBy() );
		if( !created() )
		    ret = removeLv( i->name() );
		}
            }
	for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
            {
            if( !i->isSnapshot() && i->isPool() && !created())
                ret = removeLv( i->name() );
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
	    pe = (s - 4000)/pe_size;
	    pvn.num_pe = pvn.free_pe = pe;
	    pvn.device = d;

	    const Volume* v;
	    if (getStorage()->findVolume(d, v, true) && v->dmcrypt())
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
    if( ret==0 && findLv(name)!=NULL )
	{
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

int
LvmVg::createPool( const string& name, unsigned long long sizeK,
                   string& device )
    {
    int ret = 0;
    y2mil("name:" << name << " sizeK:" << sizeK);
    ret = createLv( name, sizeK, 1, device );
    if( ret==0 )
        {
        LvmLv* lv=findLv(name);
        if( lv!=NULL )
            lv->setPool();
        else
            ret = LVM_LV_NOT_IN_LIST;
        }
    y2mil("ret:" << ret);
    return( ret );
    }

int
LvmVg::changeChunkSize( const string& name, unsigned long long chunkSizeK )
    {
    int ret = 0;
    y2mil("name:" << name << " sizeK:" << chunkSizeK);
    LvmLv* i;
    checkConsistency();
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 )
	{
        i = findLv(name);
	if( i==NULL )
	    ret = LVM_LV_UNKNOWN_NAME;
        else if( !i->isPool() && !i->isSnapshot())
	    ret = LVM_LV_NO_POOL_OR_SNAP;
        else if( !i->created() )
	    ret = LVM_LV_ALREADY_ON_DISK;
	}
    if( ret==0 && i->chunkSize()!=chunkSizeK )
	{
        if( (i->isPool() && !checkChunk( chunkSizeK, 64, 1048576 )) ||
            (i->isSnapshot() && !checkChunk( chunkSizeK, 4, 512 )))
            ret=LVM_LV_INVALID_CHUNK_SIZE;
        else
            i->setChunkSize( chunkSizeK );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
LvmVg::createThin( const string& name, const string& pool,
                   unsigned long long sizeK, string& device )
    {
    int ret = 0;
    y2mil("name:" << name << " pool:" << pool << " sizeK:" << sizeK);
    checkConsistency();
    if( readonly() )
        {
        ret = LVM_CHANGE_READONLY;
        }
    if( ret==0 && name.find( "\"\' /\n\t:*?" )!=string::npos )
        {
        ret = LVM_LV_INVALID_NAME;
        }
    if( ret==0 && findLv(name)!=NULL )
        {
        ret = LVM_LV_DUPLICATE_NAME;
        }
    if( ret==0 )
        {
        LvmLv* i=findLv(pool);
        if( i==NULL )
            ret = LVM_LV_UNKNOWN_POOL;
        else if( !i->isPool() )
            ret = LVM_LV_NO_POOL;
        }
    if( ret==0 )
        {
        unsigned long num_le = sizeToLe(sizeK);
        LvmLv* l = new LvmLv( *this, name, dev + "/" + name, "", num_le, 1 );
        l->setCreated( true );
        l->setUsedPool( pool );
	l->setTargetName("thin");
        device = l->device();
        addToList( l );
        }
    if( ret==0 )
        checkConsistency();
    y2mil("ret:" << ret << " device:" << (ret?"":device));
    return( ret );
    }

int
LvmVg::removeVolume( Volume* v )
    {
    return( removeLv( v->name() ));
    }

int
LvmVg::removeLv( const string& name )
    {
    int ret = 0;
    y2mil("name:" << name);
    LvmLv* i=NULL;
    checkConsistency();
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 )
	{
        i = findLv(name);
        if( i==NULL )
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
    if( ret==0 && !i->isThin())
	{
	map<string,unsigned long> pe_map = i->getPeMap();
	ret = remLvPeDistribution( i->getLe(), pe_map, pv, pv_add );
	}
    if( ret==0 )
	{
	if( !i->isThin() )
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
    LvmLv* i;
    checkConsistency();
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 )
	{
        i = findLv(name);
        if( i==NULL )
	    ret = LVM_LV_UNKNOWN_NAME;
	}
    if( ret==0 && i->stripes()!=stripe )
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
    LvmLv* i;
    checkConsistency();
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 )
	{
        i = findLv(name);
        if( i==NULL )
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
        if( !checkChunk( stripeSize, 4, peSize() ))
            ret=LVM_LV_INVALID_CHUNK_SIZE;
        else 
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
    bool thin = false;
    if (ret == 0)
	{
        LvmLv* i = findLv(origin);
        if (i == NULL)
	    ret = LVM_LV_UNKNOWN_ORIGIN;
	else
	    {
	    stripe = i->stripes();
	    thin = i->isThin();
	    }
	}
    if (ret == 0)
	{
        if( findLv(name)!=NULL )
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
    if (ret == 0 && !thin )
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
        LvmLv* i = findLv(name);
        if (i==NULL)
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
    checkConsistency();
    LvmLv* i = findLv(name);
    if (i==NULL)
	ret = LVM_LV_UNKNOWN_NAME;
    else if (!i->isSnapshot())
	ret = LVM_LV_NOT_SNAPSHOT;
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


    void
    LvmVg::getVgData(const string& name, SystemInfo& systeminfo, bool exists)
    {
	y2mil("name:" << name);

	const CmdVgdisplay& cmdvgdisplay = systeminfo.getCmdVgdisplay(name);

	if (exists && cmdvgdisplay.lvm1 != lvm1)
	    y2war("inconsistent lvm1 my:" << lvm1 << " lvm:" << cmdvgdisplay.lvm1);

	if (exists && cmdvgdisplay.pe_size != pe_size)
	    y2war("inconsistent pe_size my:" << pe_size << " lvm:" << cmdvgdisplay.pe_size);

	uuid = cmdvgdisplay.uuid;
	status = cmdvgdisplay.status;
	pe_size = cmdvgdisplay.pe_size;
	num_pe = cmdvgdisplay.num_pe;
	free_pe = cmdvgdisplay.free_pe;
	ronly = cmdvgdisplay.read_only;
	lvm1 = cmdvgdisplay.lvm1;

	calcSize();

	for (const CmdVgdisplay::LvEntry& lv_entry : cmdvgdisplay.lv_entries)
	{
	    addLv(lv_entry.origin.empty() ? lv_entry.num_le : lv_entry.num_cow_le,
		  lv_entry.name, lv_entry.origin, lv_entry.uuid, lv_entry.status,
		  lv_entry.read_only, lv_entry.pool, lv_entry.used_pool,
		  lv_entry.pool_chunk, systeminfo);

	    calcSize();
	}

	for (const CmdVgdisplay::PvEntry& pv_entry : cmdvgdisplay.pv_entries)
	{
	    Pv pv;

	    pv.device = pv_entry.device;
	    pv.uuid = pv_entry.uuid;
	    pv.status = pv_entry.status;
	    pv.num_pe = pv_entry.num_pe;
	    pv.free_pe = pv_entry.free_pe;
	    addPv(pv);

	    const Volume* v;
	    if (getStorage()->findVolume(pv.device, v, true))
	    {
		pv.device = v->device();
		pv.dmcryptDevice = v->dmcryptDevice();
	    }

	    calcSize();
        }

    LvmLvPair p=lvmLvPair(lvDeleted);
    for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
	{
	if( !i->isThin() )
	    {
	    map<string,unsigned long> pe_map = i->getPeMap();
	    remLvPeDistribution( i->getLe(), pe_map, pv, pv_add );
	    free_pe += i->getLe();
	    }
	}
    p=lvmLvPair(lvCreated);
    for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
	{
	if( !i->isThin() )
	    {
	    map<string,unsigned long> pe_map;
	    if( addLvPeDistribution( i->getLe(), i->stripes(), pv, pv_add,
				     pe_map ) == 0 )
		i->setPeMap( pe_map );
	    free_pe -= min((unsigned long long) free_pe, i->getLe());
	    }
	}
    p=lvmLvPair(lvResized);
    for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
	{
	if( !i->isThin() )
	    {
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
    }


    void
    LvmVg::addLv(unsigned long le, const string& name, const string& origin, const string& uuid,
		 const string& status, bool ro, bool pool, const string& used_pool,
		 unsigned long long pchunk, SystemInfo& systeminfo)
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
	i->getTableInfo();
	i->updateMajorMinor();
	i->setReadonly(ro);
        i->setPool(pool);
        if(pool||i->isSnapshot())
            i->setChunkSize(pchunk);
        if(!used_pool.empty())
            i->setUsedPool(used_pool);
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
            LvmLv *n = new LvmLv( *this, name, dev + "/" + name, origin,
				  le, uuid, status, systeminfo);
	    if( ro )
		n->setReadonly();
            if( pool )
                n->setPool();
            if( pool || n->isSnapshot())
                n->setChunkSize(pchunk);
            if( !used_pool.empty() )
                n->setUsedPool(used_pool);
	    if( !n->inactive() )
		addToList( n );
	    else
		{
		y2mil("inactive Lv " << name);
		delete n;
		}
	    }
	}
    }


    void
    LvmVg::addPv(const Pv& pv)
    {
	PeContainer::addPv(pv);
	if (!deleted() && pv.device != UNKNOWN_PV_DEVICE &&
	    find(pv_remove.begin(), pv_remove.end(), pv) == pv_remove.end())
	    getStorage()->setUsedBy(pv.device, UB_LVM, device());
    }


LvmLv* LvmVg::findLv( const string& name )
    {
    LvmLvPair p = lvmLvPair(LvmLv::notDeleted);
    LvmLvIter i=p.begin();
    while( i!=p.end() && i->name()!=name )
        ++i;
    return( (i!=p.end())?&(*i):NULL );
    }

bool 
LvmVg::checkChunk( unsigned long long val, unsigned long long mi, 
		   unsigned long long mx )
    {
    y2mil( "val:" << val << " min:" << mi << " max:" << mx );
    bool ret = mi==0 || val>=mi;
    ret = ret && (mx==0 || val<=mx);
    while( val>1 && ret )
        val /= 2;
    ret = val==1;
    y2mil( "ret:" << ret );
    return( ret );
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
	    c.execute(VGCHANGEBIN " -a y");
	    c.execute(VGSCANBIN " --mknodes");
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
    LvmVg::getVgs(SystemInfo& systeminfo)
    {
	list<string> l = systeminfo.getCmdVgs().getVgs();

	if (!l.empty())
	    active = true;

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
	getStorage()->showInfoCb(createText(true),silent);
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
	    SystemInfo systeminfo;
	    getVgData(name(), systeminfo);
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
	getStorage()->showInfoCb(removeText(true),silent);
	checkConsistency();
	string cmd = VGREMOVEBIN " -f " + quote(name());
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
	getStorage()->showInfoCb(extendText(true, d->device),silent);
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
	    SystemInfo systeminfo;
	    getVgData(name(), systeminfo);
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
	getStorage()->showInfoCb(reduceText(true, d->device),silent);
	string cmd = VGREDUCEBIN " " + instSysString() + quote(name()) + " " + quote(d->realDevice());
	SystemCmd c( cmd );
	if( c.retcode()!=0 )
	    {
	    ret = LVM_VG_REDUCE_FAILED;
	    setExtError( c );
	    }
	if( ret==0 )
	    {
	    SystemInfo systeminfo;
	    getVgData(name(), systeminfo);
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
	getStorage()->showInfoCb( l->createText(true), silent );
	checkConsistency();
        string cmd = LVCREATEBIN " " + instSysString(); 
        if( !l->getOrigin().empty() )
	    {
            Storage::loadModuleIfNeeded("dm-snapshot");
            LvmLv* orig = findLv(l->getOrigin());
            if( !orig || !orig->isThin() )
                cmd += " -l " + decString(l->getLe());
            if( l->chunkSize()>0 )
                cmd += " --chunksize " + l->chunkSize();
            cmd += " --snapshot";
            cmd += " --name " + quote(l->name());
            cmd += " " + quote(name() + "/" + l->getOrigin());
	    }
        else if( l->isThin() )
            {
            cmd += " -V " + decString(l->sizeK())+"k";
	    cmd += " --name " + quote(l->name());
	    cmd += " --thin " + quote(name() + "/" + l->usedPool());
            }
        else
            {
            if( l->isPool() )
                Storage::loadModuleIfNeeded("dm-thin-pool");
	    y2mil( "cmd:" << cmd );
	    y2mil( "getLe():" << l->getLe() );
	    y2mil( "dec():" << decString(l->getLe()) );
            cmd += " -l " + decString(l->getLe());
	    y2mil( "cmd:" << cmd );
	    if( l->stripes()>1 )
		{
		cmd += " -i " + decString(l->stripes());
		if( l->stripeSize()>0 )
		    cmd += " -I " + decString(l->stripeSize());
		}
            if( l->isPool() )
                {
                if( l->chunkSize()>0 )
                    cmd += " --chunksize " + decString(l->chunkSize());
                cmd += " --thinpool ";
		}
	    else
                cmd += " --name ";
	    cmd += quote(l->name());
	    cmd += " " + quote(name());
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
	    y2mil( "thin:" << l->isThin() );
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
	getStorage()->showInfoCb( l->removeText(true), silent );
	checkConsistency();
	ret = v->prepareRemove();
	if( ret==0 )
	    {
	    Storage::waitForDevice();
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
	    SystemInfo systeminfo;
	    getVgData(name(), systeminfo);
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
	if( old_le!=new_le )
	    {
	    getStorage()->showInfoCb( l->resizeText(true), silent );
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
	if( ret==0 )
	    ret = v->resizeBefore();
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
	if( ret==0 )
	    ret = v->resizeAfter();
	if( old_le!=new_le )
	    l->calcSize();
	if( ret==0 && remount )
	    ret = v->mount();
	if( ret==0 )
	    {
	    SystemInfo systeminfo;
	    getVgData(name(), systeminfo);
	    checkConsistency();
	    }
	}
    else
	ret = LVM_RESIZE_LV_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }


    string
    LvmVg::metaString() const
    {
	return lvm1 ? "-M1 " : "-M2 ";
    }


    string
    LvmVg::instSysString() const
    {
	return getStorage()->instsys() ? "-A n " : "";
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

void LvmVg::getInfo( LvmVgInfo& info ) const
    {
    info.sizeK = sizeK();
    info.peSizeK = peSize();
    info.peCount = peCount();
    info.peFree = peFree();
    info.lvm2 = lvm2();
    info.create = created();
    info.uuid = uuid;

    info.devices.clear();
    for (list<Pv>::const_iterator it = pv.begin(); it != pv.end(); ++it)
	info.devices.push_back(it->device);

    info.devices_add.clear();
    for (list<Pv>::const_iterator it = pv_add.begin(); it != pv_add.end(); ++it)
	info.devices_add.push_back(it->device);

    info.devices_rem.clear();
    for (list<Pv>::const_iterator it = pv_remove.begin(); it != pv_remove.end(); ++it)
	info.devices_rem.push_back(it->device);

    y2mil( "device:" << info.devices );
    if( !info.devices_add.empty() )
	y2mil( " devices_add:" << info.devices_add );
    if( !info.devices_rem.empty() )
        y2mil( " devices_rem:" << info.devices_rem );
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
