/*
  Textdomain    "storage"
*/

#include <iostream>
#include <sstream>
#include <utility>

#include "y2storage/PeContainer.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

using namespace std;
using namespace storage;

PeContainer::PeContainer( Storage * const s, CType t ) :
    Container(s,"",t)
    {
    y2debug( "constructing pe container type %d", t );
    init();
    }

PeContainer::~PeContainer()
    {
    y2debug( "destructed pe container %s", dev.c_str() );
    }

void PeContainer::unuseDev()
    {
    for( list<Pv>::const_iterator s=pv.begin(); s!=pv.end(); ++s )
	getStorage()->setUsedBy( s->device, UB_NONE, "" );
    for( list<Pv>::const_iterator s=pv_add.begin(); s!=pv_add.end(); ++s )
	getStorage()->setUsedBy( s->device, UB_NONE, "" );
    }

int
PeContainer::setPeSize( unsigned long long peSizeK, bool lvm1 )
    {
    int ret = 0;
    y2milestone( "peSize:%llu", peSizeK );

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
	}
    y2milestone( "ret:%d", ret );
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
    cur = find( pl.begin(), pl.end(), dev );
    if( cur==pl.end() )
	{
	cur = find( pladd.begin(), pladd.end(), dev );
	if( cur!=pladd.end() )
	    added_pv = true;
	else
	    ret = PEC_PV_NOT_FOUND;
	}
    if( ret==0 )
	cur_pv = *cur;
    if( ret==0 && cur->free_pe<cur->num_pe )
	{
	list<Dm*> li;
	VolPair lp=volPair(Volume::notDeleted);
	VolIterator i=lp.begin();
	while( ret==0 && i!=lp.end() )
	    {
	    Dm* dm = static_cast<Dm*>(&(*i));
	    if( dm->usingPe( dev )>0 )
		{
		if( i->created() )
		    li.push_back( dm );
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
	getStorage()->setUsedBy( dev, UB_NONE, "" );
	removed_pe += cur_pv.num_pe;
	if( !added_pv )
	    plrem.push_back( cur_pv );
	}
    y2milestone( "ret:%d removed_pe:%lu dev:%s", ret, removed_pe,
                 cur_pv.device.c_str() );
    return( ret );
    }

int
PeContainer::addLvPeDistribution( unsigned long le, unsigned stripe, list<Pv>& pl,
				  list<Pv>& pladd, map<string,unsigned long>& pe_map )
    {
    int ret=0;
    y2milestone( "le:%lu stripe:%u", le, stripe );
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
    y2milestone( "ret:%d", ret );
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
	if( (p=find( pl.begin(), pl.end(), mit->first))!=pl.end() ||
	    (p=find( pladd.begin(), pladd.end(), mit->first))!=pladd.end())
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
    bool ret = find( pv_add.begin(), pv_add.end(), dev )!=pv_add.end();
    y2mil( "dev:" << dev << " ret:" << ret );
    return( ret );
    }

bool PeContainer::checkCreateConstraints()
    {
    y2mil( "peContainer:" << *this )
    int ret = false;
    unsigned long increase = 0;
    unsigned long current = 0;
    typedef pair<unsigned long,Dm*> tpair;
    list< tpair > li;
    VolPair lp=volPair();
    VolIterator i=lp.begin();
    if( pv_add.size()>0 )
	y2war( "should not happen pv_add:" << pv_add )
    if( pv_remove.size()>0 )
	y2war( "should not happen pv_rem:" << pv_remove )
    while( i!=lp.end() )
	{
	unsigned long long tmp;
	if( i->deleted() || i->needShrink() )
	    y2war( "should not happen vol:" << *i )
	else if( i->created() || i->extendSize()>0 )
	    {
	    tmp = sizeToLe(i->created() ? i->sizeK() : i->extendSize());
	    if( !i->created() )
		current += sizeToLe( i->origSizeK() );
	    li.push_back( make_pair(tmp,static_cast<Dm*>(&(*i))) );
	    increase += tmp;
	    y2mil( "inc:" << tmp << " sum:" << increase );
	    y2mil( "vol:" << *i )
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void PeContainer::addPv( const Pv* p )
    {
    list<Pv>::iterator i = find( pv.begin(), pv.end(), *p );
    if( i != pv.end() )
	*i = *p;
    else if( find( pv_remove.begin(), pv_remove.end(), *p ) == 
             pv_remove.end() )
	{
	i = find( pv_add.begin(), pv_add.end(), *p );
	if( i!=pv_add.end() )
	    pv_add.erase(i);
	pv.push_back( *p );
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

void
PeContainer::init()
    {
    y2mil( "init:" << nm );
    mjr = Dm::dmMajor();
    num_pe = free_pe = 0;
    pe_size = 1;
    }

unsigned long
PeContainer::leByLvRemove() const
    {
    unsigned long ret=0;
    ConstVolPair p=volPair(Volume::isDeleted);
    for( ConstVolIterator i=p.begin(); i!=p.end(); ++i )
	ret += static_cast<const Dm*>(&(*i))->getLe();
    y2milestone( "ret:%lu", ret );
    return( ret );
    }

bool
PeContainer::checkConsistency() const
    {
    bool ret = true;
    unsigned long sum = 0;
    ConstVolPair lp=volPair(Volume::notDeleted);
    map<string,unsigned long> peg;
    map<string,unsigned long>::iterator mi;
    for( ConstVolIterator l = lp.begin(); l!=lp.end(); ++l )
	{
	const Dm * dm = static_cast<const Dm*>(&(*l));
	ret = ret && dm->checkConsistency();
	map<string,unsigned long> pem = dm->getPeMap();
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
	if( (p=find( pv.begin(), pv.end(), mit->first ))!=pv.end()||
	    (p=find( pv_add.begin(), pv_add.end(), mit->first ))!=pv_add.end())
	    {
	    if( mit->second != p->num_pe-p->free_pe )
		{
		y2warning( "Vg:%s used pv %s is %lu should be %lu",
		           name().c_str(), mit->first.c_str(),
		           mit->second,  p->num_pe-p->free_pe );
		ret = false;
		}
	    }
	else
	    {
	    y2warning( "Vg:%s pv %s not found", name().c_str(),
	               mit->first.c_str() );
	    ret = false;
	    }
	}
    if( sum != num_pe-free_pe )
	{
	y2warning( "Vg:%s used PE is %lu should be %lu", name().c_str(),
	           sum, num_pe-free_pe );
	ret = false;
	}
    return( ret );
    }

namespace storage
{

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
    s << *((Container*)&d);
    s << " SizeM:" << d.sizeK()/1024
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
    return( s );
    }

std::ostream& operator<< (std::ostream& s, const PeContainer::Pv& v )
    {
    s << "device:" << v.device
      << " PE:" << v.num_pe
      << " Free:" << v.free_pe
      << " Status:" << v.status
      << " UUID:" << v.uuid;
    return( s );
    }

}

string PeContainer::logDiff( const PeContainer& rhs ) const
    {
    string ret = Container::logDifference( rhs );
    if( pe_size!=rhs.pe_size )
	ret += " PeSize:" + decString(pe_size) + "-->" + decString(rhs.pe_size);
    if( num_pe!=rhs.num_pe )
	ret += " PE:" + decString(num_pe) + "-->" + decString(rhs.num_pe);
    if( free_pe!=rhs.free_pe )
	ret += " Free:" + decString(free_pe) + "-->" + decString(rhs.free_pe);
    string tmp;
    list<Pv>::const_iterator i = pv.begin();
    list<Pv>::const_iterator j;
    while( i!=pv.end() )
	{
	j = find( rhs.pv.begin(), rhs.pv.end(), *i );
	if( j==rhs.pv.end() )
	    tmp += i->device + "-->";
	++i;
	}
    i = rhs.pv.begin();
    while( i!=rhs.pv.end() )
	{
	j = find( pv.begin(), pv.end(), *i );
	if( j==pv.end() )
	    tmp += "<--" + i->device;
	++i;
	}
    if( !tmp.empty() )
	ret += " Pv:" + tmp;
    tmp.erase();
    i = pv_add.begin();
    while( i!=pv_add.end() )
	{
	j = find( rhs.pv_add.begin(), rhs.pv_add.end(), *i );
	if( j==rhs.pv_add.end() )
	    tmp += i->device + "-->";
	++i;
	}
    i = rhs.pv_add.begin();
    while( i!=rhs.pv_add.end() )
	{
	j = find( pv_add.begin(), pv_add.end(), *i );
	if( j==pv_add.end() )
	    tmp += "<--" + i->device;
	++i;
	}
    if( !tmp.empty() )
	ret += " PvAdd:" + tmp;
    tmp.erase();
    i = pv_remove.begin();
    while( i!=pv_remove.end() )
	{
	j = find( rhs.pv_remove.begin(), rhs.pv_remove.end(), *i );
	if( j==rhs.pv_remove.end() )
	    tmp += i->device + "-->";
	++i;
	}
    i = rhs.pv_remove.begin();
    while( i!=rhs.pv_remove.end() )
	{
	j = find( pv_remove.begin(), pv_remove.end(), *i );
	if( j==pv_remove.end() )
	    tmp += "<--" + i->device;
	++i;
	}
    if( !tmp.empty() )
	ret += " PvRemove:" + tmp;
    tmp.erase();
    return( ret );
    }

bool PeContainer::equalContent( const PeContainer& rhs, bool comp_vol ) const
    {
    bool ret = Container::equalContent(rhs) &&
	       pe_size==rhs.pe_size && num_pe==rhs.num_pe &&
	       free_pe==rhs.free_pe && pv==rhs.pv && pv_add==rhs.pv_add &&
	       pv_remove==rhs.pv_remove;
    if( ret )
	{
	list<Pv>::const_iterator i = rhs.pv.begin();
	list<Pv>::const_iterator j = pv.begin();
	while( ret && i!=rhs.pv.end() )
	    {
	    ret = ret && i->equalContent(*j);
	    ++i;
	    ++j;
	    }
	i = rhs.pv_add.begin();
	j = pv_add.begin();
	while( ret && i!=rhs.pv_add.end() )
	    {
	    ret = ret && i->equalContent(*j);
	    ++i;
	    ++j;
	    }
	i = rhs.pv_remove.begin();
	j = pv_remove.begin();
	while( ret && i!=rhs.pv_remove.end() )
	    {
	    ret = ret && i->equalContent(*j);
	    ++i;
	    ++j;
	    }
	}
    if( ret && comp_vol )
	{
	CVIter i = rhs.begin();
	CVIter j = begin();
	while( ret && i!=rhs.end() && j!=end() )
	    ret = ret && ((Dm*)(&(*j)))->equalContent( *(Dm*)(&(*i)));
	ret == ret && i==rhs.end() && j==end();
	}
    return( ret );
    }

PeContainer& PeContainer::operator=( const PeContainer& rhs )
    {
    pe_size = rhs.pe_size;
    num_pe = rhs.num_pe;
    free_pe = rhs.free_pe;
    pv = rhs.pv;
    pv_add = rhs.pv_add;
    pv_remove = rhs.pv_remove;
    return( *this );
    }

PeContainer::PeContainer( const PeContainer& rhs ) : Container(rhs)
    {
    y2debug( "constructed PeContainer by copy constructor from %s",
	     rhs.nm.c_str() );
    *this = rhs;
    }

