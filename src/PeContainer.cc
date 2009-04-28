/*
  Textdomain    "storage"
*/

#include <ostream>
#include <sstream>
#include <utility>

#include "y2storage/PeContainer.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"


namespace storage
{
    using namespace std;


PeContainer::PeContainer( Storage * const s, CType t ) :
    Container(s,"",t)
    {
    y2deb("constructing pe container type " << t);
    init();
    }

PeContainer::~PeContainer()
    {
    y2deb("destructed pe container " <<  dev);
    }

void PeContainer::unuseDev()
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
    if( pv_add.size()>0 )
	y2war( "should not happen pv_add:" << pv_add );
    if( pv_remove.size()>0 )
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
	const Volume *vol;
	if( getStorage()->findVolume( dev, vol ))
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
	const Volume *vol;
	if( getStorage()->findVolume( dev, vol ))
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

void
PeContainer::init()
    {
    y2mil( "init:" << nm );
    mjr = Dm::dmMajor();
    num_pe = free_pe = 0;
    pe_size = 1;
    }

static bool isDeleted( const Dm& l ) { return( l.deleted() ); }

unsigned long
PeContainer::leByLvRemove() const
    {
    unsigned long ret=0;
    ConstDmPair p=dmPair(isDeleted);
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


string PeContainer::getDiffString( const Container& rhs ) const
    {
    string ret = Container::getDiffString( rhs );
    const PeContainer* p = dynamic_cast<const PeContainer*>(&rhs);
    if( p )
	{
	if( pe_size!=p->pe_size )
	    ret += " PeSize:" + decString(pe_size) + "-->" + decString(p->pe_size);
	if( num_pe!=p->num_pe )
	    ret += " PE:" + decString(num_pe) + "-->" + decString(p->num_pe);
	if( free_pe!=p->free_pe )
	    ret += " Free:" + decString(free_pe) + "-->" + decString(p->free_pe);
	string tmp;
	list<Pv>::const_iterator i = pv.begin();
	list<Pv>::const_iterator j;
	while( i!=pv.end() )
	    {
	    j = find( p->pv.begin(), p->pv.end(), *i );
	    if( j==p->pv.end() )
		tmp += i->device + "-->";
	    ++i;
	    }
	i = p->pv.begin();
	while( i!=p->pv.end() )
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
	    j = find( p->pv_add.begin(), p->pv_add.end(), *i );
	    if( j==p->pv_add.end() )
		tmp += i->device + "-->";
	    ++i;
	    }
	i = p->pv_add.begin();
	while( i!=p->pv_add.end() )
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
	    j = find( p->pv_remove.begin(), p->pv_remove.end(), *i );
	    if( j==p->pv_remove.end() )
		tmp += i->device + "-->";
	    ++i;
	    }
	i = p->pv_remove.begin();
	while( i!=p->pv_remove.end() )
	    {
	    j = find( pv_remove.begin(), pv_remove.end(), *i );
	    if( j==pv_remove.end() )
		tmp += "<--" + i->device;
	    ++i;
	    }
	if( !tmp.empty() )
	    ret += " PvRemove:" + tmp;
	}
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
	ret = ret && i==rhs.end() && j==end();
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
    y2deb("constructed PeContainer by copy constructor from " << rhs.nm);
    *this = rhs;
    }

}
