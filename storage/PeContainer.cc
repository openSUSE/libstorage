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
#include <utility>

#include "storage/PeContainer.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/SystemCmd.h"
#include "storage/StorageDefines.h"
#include "storage/Regex.h"


namespace storage
{
    using namespace std;


    PeContainer::PeContainer(Storage* s, const string& name, const string& device, CType t)
	: Container(s, name, device, t), pe_size(1), num_pe(0), free_pe(0)
    {
	y2deb("constructing PeContainer name:" << name << " ctype:" << toString(t));
    }


    PeContainer::PeContainer(Storage* s, const string& name, const string& device, CType t,
			     SystemInfo& systeminfo)
	: Container(s, name, device, t, systeminfo), pe_size(1), num_pe(0), free_pe(0)
    {
	y2deb("constructing PeContainer name:" << name << " ctype:" << toString(t));
    }


    PeContainer::PeContainer(Storage* s, CType t, const xmlNode* node)
	: Container(s, t, node), pe_size(1), num_pe(0), free_pe(0)
    {
	getChildValue(node, "pe_size_k", pe_size);
	getChildValue(node, "pe_count", num_pe);
	getChildValue(node, "pe_free", free_pe);

	const list<const xmlNode*> l = getChildNodes(node, "physical_extent");
	for (list<const xmlNode*>::const_iterator it = l.begin(); it != l.end(); ++it)
	{
		Pv tmp = Pv(*it);
		pv.push_back(tmp);

		switch (t)
		{
		    case LVM:
			s->addUsedBy(tmp.device, UB_LVM, dev);
			break;
		    case DM:
			s->addUsedBy(tmp.device, UB_DM, dev);
			break;
		    case DMRAID:
			s->addUsedBy(tmp.device, UB_DMRAID, dev);
			break;
		    case DMMULTIPATH:
			s->addUsedBy(tmp.device, UB_DMMULTIPATH, dev);
			break;
		    default:
			break;
		}
	}

	y2deb("constructed PeContainer " << dev);
    }


    PeContainer::PeContainer(const PeContainer& c)
	: Container(c), pe_size(c.pe_size), num_pe(c.num_pe),
	  free_pe(c.free_pe), pv(c.pv), pv_add(c.pv_add),
	  pv_remove(c.pv_remove)
    {
	y2deb("copy-constructed PeContainer " << dev);
    }


    PeContainer::~PeContainer()
    {
	y2deb("destructed PeContainer " << dev);
    }


    void
    PeContainer::saveData(xmlNode* node) const
    {
	Container::saveData(node);

	setChildValue(node, "pe_size_k", peSize());
	setChildValue(node, "pe_count", peCount());
	setChildValue(node, "pe_free", peFree());

	for (list<Pv>::const_iterator it = pv.begin(); it != pv.end(); ++it)
	    it->saveData(xmlNewChild(node, "physical_extent"));
    }


void PeContainer::unuseDev() const
    {
    for( list<Pv>::const_iterator s=pv.begin(); s!=pv.end(); ++s )
	getStorage()->clearUsedBy(s->device);
    for( list<Pv>::const_iterator s=pv_add.begin(); s!=pv_add.end(); ++s )
	getStorage()->clearUsedBy(s->device);
    }

int
PeContainer::setPeSize( unsigned long long peSizeK, bool lvm1 )
    {
    int ret = 0;
    y2mil("peSize:" << peSizeK);

    if( pe_size!=peSizeK )
	{
	if( lvm1 )
	    {
	    if( peSizeK<8 || peSizeK>16*1024*1024 )
		ret = PEC_PE_SIZE_INVALID;
	    }
	if( ret==0 )
	    {
	    unsigned long long sz = peSizeK;
	    while( sz>1 && sz%2==0 )
		sz /= 2;
	    if( sz!=1 )
		ret = PEC_PE_SIZE_INVALID;
	    }
	if( ret==0 )
	    {
	    num_pe = num_pe * pe_size / peSizeK;
	    free_pe = free_pe * pe_size / peSizeK;
	    list<Pv>::iterator p;
	    for( p=pv.begin(); p!=pv.end(); ++p )
		{
		p->num_pe = p->num_pe * pe_size / peSizeK;
		p->free_pe = p->free_pe * pe_size / peSizeK;
		}
	    for( p=pv_add.begin(); p!=pv_add.end(); ++p )
		{
		p->num_pe = p->num_pe * pe_size / peSizeK;
		p->free_pe = p->free_pe * pe_size / peSizeK;
		}
	    for( p=pv_remove.begin(); p!=pv_remove.end(); ++p )
		{
		p->num_pe = p->num_pe * pe_size / peSizeK;
		p->free_pe = p->free_pe * pe_size / peSizeK;
		}
	    pe_size = peSizeK;
	    }
	calcSize();
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
PeContainer::tryUnusePe( const string& dev, list<Pv>& pl, list<Pv>& pladd,
		         list<Pv>& plrem, unsigned long& removed_pe )
    {
    int ret = 0;
    int added_pv = false;
    Pv cur_pv;
    list<Pv>::iterator cur;
    if( !findPe( dev, pl, cur ))
	{
	if( findPe( dev, pladd, cur ))
	    added_pv = true;
	else
	    ret = PEC_PV_NOT_FOUND;
	}
    if( ret==0 )
	cur_pv = *cur;
    if( ret==0 && cur->free_pe<cur->num_pe )
	{
	list<Dm*> li;
	DmPair lp=dmPair(Dm::notDeleted);
	DmIter i=lp.begin();
	while( ret==0 && i!=lp.end() )
	    {
	    if( i->usingPe( dev )>0 )
		{
		if( i->created() )
		    li.push_back( &(*i) );
		else
		    ret = PEC_REMOVE_PV_IN_USE;
		}
	    ++i;
	    }
	list<Dm*>::iterator lii=li.begin();
	if( ret==0 )
	    {
	    while( ret==0 && lii!=li.end() )
		{
		map<string,unsigned long> pe_map = (*lii)->getPeMap();
		ret = remLvPeDistribution( (*lii)->getLe(), pe_map, pl, pladd );
		++lii;
		}
	    }
	if( ret==0 )
	    {
	    if( added_pv )
		pladd.erase( cur );
	    else
		pl.erase( cur );
	    lii=li.begin();
	    while( ret==0 && lii!=li.end() )
		{
		map<string,unsigned long> pe_map;
		ret = addLvPeDistribution( (*lii)->getLe(), (*lii)->stripes(),
		                           pl, pladd, pe_map );
		if( ret==0 )
		    {
		    (*lii)->setPeMap( pe_map );
		    }
		else
		    {
		    ret = PEC_REMOVE_PV_SIZE_NEEDED;
		    }
		++lii;
		}
	    }
	}
    else if( ret==0 )
	{
	if( added_pv )
	    pladd.erase( cur );
	else
	    pl.erase( cur );
	}
    if( ret==0 )
	{
	getStorage()->clearUsedBy(dev);
	removed_pe += cur_pv.num_pe;
	if( !added_pv )
	    plrem.push_back( cur_pv );
	}
    y2mil("ret:" << ret << " removed_pe:" << removed_pe << " dev:" << cur_pv.device);
    return( ret );
    }

int
PeContainer::addLvPeDistribution( unsigned long le, unsigned stripe, list<Pv>& pl,
				  list<Pv>& pladd, map<string,unsigned long>& pe_map )
    {
    int ret=0;
    y2mil("le:" << le << " stripe:" << stripe);
    map<string,unsigned long>::iterator mit;
    list<Pv>::iterator i;
    if( stripe>1 )
	{
	// this is only a very rough estimate if the creation of the striped
	// lv may be possible, no sense in emulating LVM allocation strategy
	// here
	unsigned long per_stripe = (le+stripe-1)/stripe;
	list<Pv> tmp = pl;
	tmp.insert( tmp.end(), pladd.begin(), pladd.end() );
	tmp.sort( Pv::comp_le );
	tmp.remove_if( Pv::no_free );
	while( per_stripe>0 && tmp.size()>=stripe )
	    {
	    i = tmp.begin();
	    unsigned rem = min( per_stripe, i->free_pe );
	    for( unsigned cnt=0; cnt<stripe; cnt++ )
		{
		i->free_pe -= rem;
		if( (mit=pe_map.find( i->device ))==pe_map.end() )
		    pe_map[i->device] = rem;
		else
		    mit->second += rem;
		++i;
		}
	    per_stripe -= rem;
	    tmp.remove_if( Pv::no_free );
	    }
	if( per_stripe>0 )
	    ret = PEC_LV_NO_SPACE_STRIPED;
	else
	    {
	    list<Pv>::iterator p;
	    i = pl.begin();
	    while( i!=pl.end() )
		{
		if( (p=find( tmp.begin(), tmp.end(), i->device))!=tmp.end())
		    i->free_pe = p->free_pe;
		else
		    i->free_pe = 0;
		++i;
		}
	    i = pladd.begin();
	    while( i!=pladd.end() )
		{
		if( (p=find( tmp.begin(), tmp.end(), i->device))!=tmp.end())
		    i->free_pe = p->free_pe;
		else
		    i->free_pe = 0;
		++i;
		}
	    }
	}
    else
	{
	unsigned long rest = le;
	i = pl.begin();
	while( rest>0 && i!=pl.end() )
	    {
	    rest -= min(rest,i->free_pe);
	    ++i;
	    }
	i = pladd.begin();
	while( rest>0 && i!=pladd.end() )
	    {
	    rest -= min(rest,i->free_pe);
	    ++i;
	    }
	if( rest>0 )
	    ret = PEC_LV_NO_SPACE_SINGLE;
	else
	    {
	    rest = le;
	    i = pl.begin();
	    while( rest>0 && i!=pl.end() )
		{
		unsigned long tmp = min(rest,i->free_pe);
		i->free_pe -= tmp;
		rest -= tmp;
		if( (mit=pe_map.find( i->device ))==pe_map.end() )
		    pe_map[i->device] = tmp;
		else
		    mit->second += tmp;
		++i;
		}
	    i = pladd.begin();
	    while( rest>0 && i!=pladd.end() )
		{
		unsigned long tmp = min(rest,i->free_pe);
		i->free_pe -= tmp;
		rest -= tmp;
		if( (mit=pe_map.find( i->device ))==pe_map.end() )
		    pe_map[i->device] = tmp;
		else
		    mit->second += tmp;
		++i;
		}
	    }
	}
    if( ret==0 )
	{
	y2mil( "pe_map:" << pe_map );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
PeContainer::remLvPeDistribution( unsigned long le, map<string,unsigned long>& pe_map,
				  list<Pv>& pl, list<Pv>& pladd )
    {
    int ret=0;
    y2mil( "le:" << le << " pe_map:" << pe_map );
    list<Pv>::iterator p;
    map<string,unsigned long>::iterator mit = pe_map.begin();
    y2mil( "pl:" << pl );
    y2mil( "pladd:" << pladd );
    while( le>0 && ret==0 && mit != pe_map.end() )
	{
	if( findPe( mit->first, pl, p ) || findPe( mit->first, pladd, p ) )
	    {
	    int tmp = min(le,mit->second);
	    p->free_pe += tmp;
	    mit->second -= tmp;
	    le -= tmp;
	    }
	else
	    ret = PEC_LV_PE_DEV_NOT_FOUND;
	++mit;
	}
    y2mil( "pe_map:" << pe_map );
    y2mil( "ret:" << ret );
    return( ret );
    }

unsigned long PeContainer::sizeToLe( unsigned long long sizeK ) const
    {
    if( pe_size>0 )
	{
	sizeK += pe_size-1;
	sizeK /= pe_size;
	}
    return( sizeK );
    }

bool PeContainer::addedPv( const string& dev ) const
    {
    list<Pv>::const_iterator i;
    bool ret = findPe( dev, pv_add, i );
    y2mil( "dev:" << dev << " ret:" << ret );
    return( ret );
    }

bool PeContainer::checkCreateConstraints()
    {
    y2mil( "peContainer:" << *this );
    int ret = false;
    unsigned long increase = 0;
    unsigned long current = 0;
    typedef pair<unsigned long,Dm*> tpair;
    list< tpair > li;
    DmPair lp=dmPair();
    DmIter i=lp.begin();
    if (!pv_add.empty())
	y2war( "should not happen pv_add:" << pv_add );
    if (!pv_remove.empty())
	y2war( "should not happen pv_rem:" << pv_remove );
    while( i!=lp.end() )
	{
	unsigned long long tmp;
	if( i->deleted() || i->needShrink() )
	    y2war( "should not happen dm:" << *i );
	else if( i->created() || i->extendSize()>0 )
	    {
	    tmp = sizeToLe(i->created() ? i->sizeK() : i->extendSize());
	    if( !i->created() )
		current += sizeToLe( i->origSizeK() );
	    li.push_back( make_pair(tmp,&(*i)) );
	    increase += tmp;
	    y2mil( "inc:" << tmp << " sum:" << increase );
	    y2mil( "vol:" << *i );
	    }
	else
	    current += sizeToLe( i->sizeK() );
	++i;
	}
    y2mil( "increase:" << increase << " current:" << current << " num_pe:" << num_pe );
    if( increase+current>num_pe )
	{
	unsigned long diff = increase+current - num_pe;
	y2mil( "too much:" << diff );
	if( diff<=5 || diff<=pv.size()*2 )
	    {
	    list<unsigned long> l;
	    y2mil( "li:" << li );
	    li.sort();
	    y2mil( "li:" << li );
	    for( list<tpair>::const_iterator i=li.begin(); i!=li.end(); ++i )
		{
		unsigned long tmp = (diff * i->first + i->first/2) / increase;
		l.push_back(tmp);
		diff -= tmp;
		increase -= i->first;
		}
	    y2mil( "l:" << l );
	    list<unsigned long>::const_iterator di = l.begin();
	    for( list<tpair>::const_iterator i=li.begin(); i!=li.end(); ++i )
		{
		if( *di > 0 )
		    {
		    y2mil( "modified vol:" << *i->second );
		    map<string,unsigned long> pe_map = i->second->getPeMap();
		    ret = remLvPeDistribution( *di, pe_map, pv, pv_add );
		    i->second->setLe( i->second->getLe()-*di );
		    i->second->setPeMap( pe_map );
		    if( i->second->created() )
			i->second->calcSize();
		    else
			i->second->setResizedSize( i->second->getLe()*pe_size );
		    y2mil( "modified vol:" << *i->second );
		    }
		++di;
		}
	    }
	ret = true;
	}
    y2mil("ret:" << ret);
    return( ret );
    }


bool
PeContainer::findPe(const string& dev, const list<Pv>& pl, list<Pv>::const_iterator& i) const
    {
    bool ret = !pl.empty();
    if( ret )
	{
	const Device *vol;
	if( getStorage()->findDevice( dev, vol, true ) )
	    {
	    i = pl.begin();
	    while( i!=pl.end() && !vol->sameDevice( i->device ))
		++i;
	    }
	else
	    {
	    y2war( "unknown volume:" << dev );
	    i = find( pl.begin(), pl.end(), dev );
	    }
	ret = i!=pl.end();
	}
    y2mil( "dev:" << dev << " ret:" << ret );
    return( ret );
    }


bool
PeContainer::findPe(const string& dev, list<Pv>& pl, list<Pv>::iterator& i) const
    {
    bool ret = !pl.empty();
    if( ret )
	{
	const Device *vol;
	if( getStorage()->findDevice( dev, vol, true ) )
	    {
	    i = pl.begin();
	    while( i!=pl.end() && !vol->sameDevice( i->device ))
		++i;
	    }
	else
	    {
	    y2war( "unknown volume:" << dev );
	    i = find( pl.begin(), pl.end(), dev );
	    }
	ret = i!=pl.end();
	}
    y2mil( "dev:" << dev << " ret:" << ret );
    return( ret );
    }


    void
    PeContainer::addPv(const Pv& p)
    {
	getStorage()->eraseLabelVolume(p.device);

	list<Pv>::iterator i;
	if (findPe(p.device, pv, i))
	    *i = p;
	else if (!findPe(p.device, pv_remove, i))
	{
	    if (findPe(p.device, pv_add, i))
		pv_add.erase(i);
	    pv.push_back(p);
	}
    }


string PeContainer::addList() const
    {
    string ret;
    list<Pv>::const_iterator i = pv_add.begin();
    while( i!=pv_add.end() )
	{
	if( !ret.empty() )
	    ret += ' ';
	ret+= i->device;
	++i;
	}
    return( ret );
    }


unsigned long
PeContainer::leByLvRemove() const
    {
    unsigned long ret=0;
    ConstDmPair p = dmPair(Dm::isDeleted);
    for( ConstDmIter i=p.begin(); i!=p.end(); ++i )
	ret += i->getLe();
    y2mil("ret:" << ret);
    return( ret );
    }

bool
PeContainer::checkConsistency() const
    {
    bool ret = true;
    unsigned long sum = 0;
    ConstDmPair lp=dmPair(Dm::notDeleted);
    map<string,unsigned long> peg;
    map<string,unsigned long>::iterator mi;
    for( ConstDmIter l = lp.begin(); l!=lp.end(); ++l )
	{
	ret = ret && l->checkConsistency();
	map<string,unsigned long> pem = l->getPeMap();
	for( map<string,unsigned long>::const_iterator mit=pem.begin();
	     mit!=pem.end(); ++mit )
	    {
	    if( (mi=peg.find( mit->first ))!=peg.end() )
		mi->second += mit->second;
	    else
		peg[mit->first] = mit->second;
	    }
	}
    sum = 0;
    list<Pv>::const_iterator p;
    for( map<string,unsigned long>::const_iterator mit=peg.begin();
	 mit!=peg.end(); ++mit )
	{
	sum += mit->second;
	if( findPe( mit->first, pv, p ) || findPe( mit->first, pv_add, p ) )
	    {
	    if( mit->second != p->num_pe-p->free_pe )
		{
		y2war("Vg:" << name() << " used pv " << mit->first << " is " <<
		      mit->second << " should be " << p->num_pe - p->free_pe);
		ret = false;
		}
	    }
	else
	    {
	    y2war("Vg:" << name() << " pv " << mit->first << " not found");
	    ret = false;
	    }
	}
    if( sum != num_pe-free_pe )
	{
	y2war("Vg:" << name() << " used PE is " << sum << " should be " << num_pe - free_pe);
	ret = false;
	}
    return( ret );
    }

void PeContainer::changeDeviceName( const string& old, const string& nw )
    {
    list<Pv>::iterator i = find( pv_add.begin(), pv_add.end(), old );
    if( i!=pv_add.end() )
	i->device = nw;
    DmPair dp=dmPair();
    for( DmIter di=dp.begin(); di!=dp.end(); ++di )
	{
	di->changeDeviceName( old, nw );
	}
    }

string PeContainer::getDeviceByNumber( const string& majmin ) const
    {
    string ret = getStorage()->deviceByNumber(majmin);
    if( ret.empty() )
	{
	unsigned mj = 0;
	unsigned mi = 0;
	string pair( majmin );
	SystemCmd c;
	mj = mi = 0;
	string::size_type pos = pair.find( ':' );
	if( pos != string::npos )
	    pair[pos] = ' ';
	istringstream i( pair );
	classic(i);
	i >> mj >> mi;
	list<string> ls = splitString(pair);
	if( majorNr()>0 && mj==majorNr() && mi==minorNr())
	    ret = device();
	if( mj==Loop::loopMajor() )
	    ret = Loop::loopDeviceName(mi);
	}
    y2mil( "majmin " << majmin << " ret:" << ret );
    return( ret );
    }

void printDevList( std::ostream& s, const std::list<PeContainer::Pv>& l )
    {
    s << "<";
    for( std::list<PeContainer::Pv>::const_iterator i=l.begin(); 
	 i!=l.end(); ++i )
	{
	if( i!=l.begin() )
	    s << " ";
	s << i->device;
	}
    s << ">";
    }

std::ostream& operator<< (std::ostream& s, const PeContainer& d )
    {
    s << dynamic_cast<const Container&>(d);
    s << " SizeK:" << d.sizeK()
      << " PeSize:" << d.pe_size
      << " NumPE:" << d.num_pe
      << " FreePE:" << d.free_pe;
    if( !d.pv.empty() )
	{
	s << " pv:";
	printDevList( s, d.pv );
	}
    if( !d.pv_add.empty() )
	{
	s << " pv_add:";
	printDevList( s, d.pv_add );
	}
    if( !d.pv_remove.empty() )
	{
	s << " pv_remove:";
	printDevList( s, d.pv_remove );
	}
    return s;
    }


    std::ostream& operator<<(std::ostream& s, const PeContainer::Pv& v)
    {
	s << "device:" << v.device;
	if (!v.dmcryptDevice.empty())
	    s << " dmcryptDevice:" << v.dmcryptDevice;
	s << " PE:" << v.num_pe
	  << " Free:" << v.free_pe
	  << " Status:" << v.status
	  << " UUID:" << v.uuid;
	return s;
    }


    void
    PeContainer::logDifference(std::ostream& log, const PeContainer& rhs) const
    {
	Container::logDifference(log, rhs);

	logDiff(log, "pe_size", pe_size, rhs.pe_size);
	logDiff(log, "num_pe", num_pe, rhs.num_pe);
	logDiff(log, "free_pe", free_pe, rhs.free_pe);

	string tmp;
	for (list<Pv>::const_iterator i = pv.begin(); i != pv.end(); ++i)
	    if (!contains(rhs.pv, *i))
		tmp += i->device + "-->";
	for (list<Pv>::const_iterator i = rhs.pv.begin(); i != rhs.pv.end(); ++i)
	    if (!contains(pv, *i))
		tmp += "<--" + i->device;
	if (!tmp.empty())
	    log << " Pv:" << tmp;

	tmp.erase();
	for (list<Pv>::const_iterator i = pv_add.begin(); i != pv_add.end(); ++i)
	    if (!contains(rhs.pv_add, *i))
		tmp += i->device + "-->";
	for (list<Pv>::const_iterator i = rhs.pv_add.begin(); i != rhs.pv_add.end(); ++i)
	    if (!contains(pv_add, *i))
		tmp += "<--" + i->device;
	if (!tmp.empty())
	    log << " PvAdd:" << tmp;

	tmp.erase();
	for (list<Pv>::const_iterator i = pv_remove.begin(); i != pv_remove.end(); ++i)
	    if (!contains(rhs.pv_remove, *i))
		tmp += i->device + "-->";
	for (list<Pv>::const_iterator i = rhs.pv_remove.begin(); i != rhs.pv_remove.end(); ++i)
	    if (!contains(pv_remove, *i))
		tmp += "<--" + i->device;
	if (!tmp.empty())
	    log << " PvRemove:" << tmp;
    }


bool PeContainer::equalContent( const PeContainer& rhs, bool comp_vol ) const
    {
    bool ret = Container::equalContent(rhs) &&
	       pe_size==rhs.pe_size && num_pe==rhs.num_pe &&
	       free_pe==rhs.free_pe && pv==rhs.pv && pv_add==rhs.pv_add &&
	       pv_remove==rhs.pv_remove;
    if( ret )
	{
	ret = ret && storage::equalContent(pv.begin(), pv.end(),
					   rhs.pv.begin(), rhs.pv.end());
	ret = ret && storage::equalContent(pv_add.begin(), pv_add.end(),
					   rhs.pv_add.begin(), rhs.pv_add.end());
	ret = ret && storage::equalContent(pv_remove.begin(), pv_remove.end(),
					   rhs.pv_remove.begin(), rhs.pv_remove.end());
	}
    if( ret && comp_vol )
	{
	ConstDmPair pp = dmPair();
	ConstDmPair pc = rhs.dmPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
       	}
    return( ret );
    }


    PeContainer::Pv::Pv(const xmlNode* node)
	: device(), num_pe(0), free_pe(0)
    {
	getChildValue(node, "device", device);
	getChildValue(node, "pe_count", num_pe);
	getChildValue(node, "pe_free", free_pe);

	y2deb("constructed Pv");

	assert(!device.empty());
    }


    void
    PeContainer::Pv::saveData(xmlNode* node) const
    {
	setChildValue(node, "device", device);
	setChildValue(node, "pe_count", num_pe);
	setChildValue(node, "pe_free", free_pe);
    }

}
