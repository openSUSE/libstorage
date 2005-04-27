#include <iostream> 
#include <sstream> 

#include "y2storage/LvmVg.h"
#include "y2storage/LvmLv.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

using namespace std;
using namespace storage;

LvmVg::LvmVg( Storage * const s, const string& Name ) :
    Container(s,Name,staticType())
    {
    y2milestone( "construcing lvm vg %s", Name.c_str() );
    init();
    if( Name.size()>0 )
	{
	getVgData( Name, false );
	}
    else
	{
	y2error( "empty name in constructor" );
	}
    }

LvmVg::LvmVg( Storage * const s, const string& Name, bool lv1 ) :
    Container(s,Name,staticType())
    {
    y2milestone( "constructing lvm vg %s lvm1:%d", Name.c_str(), lv1 );
    init();
    lvm1 = lv1;
    if( Name.size()==0 )
	{
	y2error( "empty name in constructor" );
	}
    }

LvmVg::LvmVg( Storage * const s, const string& file, int ) :
    Container(s,"",staticType())
    {
    y2milestone( "construcing lvm vg %s from file %s", dev.c_str(), 
                 file.c_str() );
    }

LvmVg::~LvmVg()
    {
    y2milestone( "destructed lvm vg %s", dev.c_str() );
    }

int
LvmVg::setPeSize( unsigned long long peSizeK )
    {
    int ret = 0;
    y2milestone( "peSize:%llu", peSizeK );
    
    if( lvm1 )
	{
	if( peSizeK<8 || peSizeK>16*1024*1024 )
	    ret = LVM_PE_SIZE_INVALID;
	}
    if( ret==0 )
	{
	unsigned long long sz = peSizeK;
	while( sz>1 && sz%2==0 )
	    sz /= 2;
	if( sz!=1 )
	    ret = LVM_PE_SIZE_INVALID;
	}
    if( ret==0 )
	{
	pe_size = peSizeK;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

static bool lvDeleted( const LvmLv& l ) { return( l.deleted() ); }
static bool lvCreated( const LvmLv& l ) { return( l.created() ); }
static bool lvResized( const LvmLv& l ) { return( l.extendSize()!=0 ); }
static bool lvNotDeleted( const LvmLv& l ) { return( !l.deleted() ); }

int
LvmVg::removeVg()
    {
    int ret = 0;
    y2milestone( "begin" );
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 && !created() )
	{
	LvmLvPair p=lvmLvPair(lvNotDeleted);
	for( LvmLvIter i=p.begin(); i!=p.end(); ++i )
	    ret = removeLv( i->name() );
	setDeleted( true );
	}
    y2milestone( "ret:%d", ret );
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

    checkConsistency();
    list<string>::const_iterator i=devs.begin();
    list<Pv>::iterator p;
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	if( (p=find( pv.begin(), pv.end(), d ))!=pv.end() || 
	    (p=find( pv_add.begin(), pv_add.end(), d ))!=pv_add.end())
	    ret = LVM_PV_ALREADY_CONTAINED;
	else if( (p=find( pv_remove.begin(), pv_remove.end(), d )) != 
	         pv_remove.end() )
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
	     pv_remove.end() )
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
	    pv_add.push_back( pvn );
	    getStorage()->setUsedBy( d, UB_LVM, name() );
	    }
	free_pe += pe;
	num_pe += pe;
	++i;
	}
    if( ret==0 && pv_add.size()+pv.size()-pv_remove.size()<=0 )
	ret = LVM_VG_HAS_NONE_PV;
    if( ret==0 )
	checkConsistency();
    y2milestone( "ret:%d", ret );
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
    y2mil( "name:" << name() << " devices:" << devs );

    checkConsistency();
    list<string>::const_iterator i=devs.begin();
    list<Pv>::iterator p;
    list<Pv> pl = pv;
    list<Pv> pladd = pv_add;
    list<Pv> plrem = pv_remove;
    unsigned long rem_pe = 0;
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	ret = tryUnusePe( d, pl, pladd, plrem, rem_pe );
	++i;
	}
    if( ret==0 && pv_add.size()+pv.size()-pv_remove.size()<=0 )
	ret = LVM_VG_HAS_NONE_PV;
    if( ret == 0 )
	{
	pv = pl;
	pv_add = pladd;
	pv_remove = plrem;
	free_pe -= rem_pe;
	num_pe -= rem_pe;
	}
    if( ret==0 )
	checkConsistency();
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
LvmVg::tryUnusePe( const string& dev, list<Pv>& pl, list<Pv>& pladd, 
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
	    ret = LVM_PV_NOT_FOUND;
	}
    if( ret==0 )
	cur_pv = *cur;
    if( ret==0 && cur->free_pe<cur->num_pe )
	{
	list<LvmLvIter> li;
	LvmLvPair lp=lvmLvPair(lvNotDeleted);
	LvmLvIter i=lp.begin();
	while( ret==0 && i!=lp.end() )
	    {
	    if( i->usingPe( dev )>0 )
		{
		if( i->created() )
		    li.push_back( i );
		else
		    ret = LVM_REMOVE_PV_IN_USE;
		}
	    ++i;
	    }
	list<LvmLvIter>::iterator lii=li.begin(); 
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
		    ret = LVM_REMOVE_PV_SIZE_NEEDED;
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
LvmVg::createLv( const string& name, unsigned long long sizeK, unsigned stripe,
                 string& device )
    {
    int ret = 0;
    y2milestone( "name:%s sizeK:%llu stripe:%u", name.c_str(), sizeK, stripe );
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
	LvmLvPair p=lvmLvPair(lvNotDeleted);
	LvmLvIter i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if( i!=p.end() )
	    ret = LVM_LV_DUPLICATE_NAME;
	}
    unsigned long num_le = (sizeK + pe_size - 1)/pe_size;
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
	LvmLv* l = new LvmLv( *this, name, num_le, stripe );
	l->setCreated( true );
	l->setPeMap( pe_map );
	device = l->device();
	free_pe -= num_le;
	addToList( l );
	}
    if( ret==0 )
	checkConsistency();
    y2milestone( "ret:%d device:%s", ret, ret?device.c_str():"" );
    return( ret );
    }

int LvmVg::resizeVolume( Volume* v, unsigned long long newSize )
    {
    int ret = 0;
    y2milestone( "newSizeK:%llu vol:%s", newSize, v->name().c_str() );
    checkConsistency();
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    else 
	{
	LvmLv * l = dynamic_cast<LvmLv *>(v);
	unsigned long new_le = (newSize+pe_size-1)/pe_size;
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
    y2milestone( "ret:%d", ret );
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
    y2milestone( "name:%s", name.c_str() );
    LvmLvIter i;
    checkConsistency();
    if( readonly() )
	{
	ret = LVM_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	LvmLvPair p=lvmLvPair(lvNotDeleted);
	i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if( i==p.end() )
	    ret = LVM_LV_UNKNOWN_NAME;
	}
    if( ret==0 && i->getUsedByType() != UB_NONE )
	{
	if( getStorage()->getRecursiveRemoval() )
	    ret = getStorage()->removeUsing( &(*i) );
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
	if( i->created() )
	    {
	    if( !removeFromList( &(*i) ))
		ret = LVM_LV_NOT_IN_LIST;
	    }
	else
	    i->setDeleted( true );
	free_pe += i->getLe();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
LvmVg::addLvPeDistribution( unsigned long le, unsigned stripe, list<Pv>& pl, 
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
	    ret = LVM_LV_NO_SPACE_STRIPED;
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
	    ret = LVM_LV_NO_SPACE_SINGLE;
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
LvmVg::remLvPeDistribution( unsigned long le, map<string,unsigned long>& pe_map, 
                            list<Pv>& pl, list<Pv>& pladd )
    {
    int ret=0;
    y2mil( "le:" << le << " pe_map:" << pe_map );
    list<Pv>::iterator p;
    map<string,unsigned long>::iterator mit = pe_map.begin();
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
	    ret = LVM_LV_PE_DEV_NOT_FOUND;
	++mit;
	}
    y2mil( "pe_map:" << pe_map );
    y2mil( "ret:" << ret );
    return( ret );
    }

void LvmVg::getVgData( const string& name, bool exists )
    {
    y2milestone( "name:%s", name.c_str() );
    SystemCmd c( "/sbin/vgdisplay --units k -v " + name );
    unsigned cnt = c.numLines();
    unsigned i = 0;
    string line;
    string tmp;
    string::size_type pos;
    while( i<cnt )
	{
	line = *c.getLine( i++ );
	if( line.find( "Volume group" )!=string::npos )
	    {
	    line = *c.getLine( i++ );
	    while( line.find( "Logical volume" )==string::npos &&
		   line.find( "Physical volume" )==string::npos &&
		   i<cnt )
		{
		line.erase( 0, line.find_first_not_of( app_ws ));
		if( line.find( "Format" ) == 0 )
		    {
		    bool lv1 = extractNthWord( 1, line )=="lvm1";
		    if( exists && lv1 != lvm1 )
			y2warning( "inconsistent lvm1 my:%d lvm:%d", 
			           lvm1, lv1 );
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
			y2warning( "inconsistent pe_size my:%llu lvm:%llu", 
			           pe_size, pes );
		    pe_size = pes;
		    }
		else if( line.find( "Total PE" ) == 0 )
		    {
		    extractNthWord( 2, line ) >> num_pe;
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
		line = *c.getLine( i++ );
		}
	    string vname;
	    string uuid;
	    string status;
	    string allocation;
	    unsigned long num_le = 0;
	    bool readOnly = false;
	    while( line.find( "Physical volume" )==string::npos && i<cnt )
		{
		line.erase( 0, line.find_first_not_of( app_ws ));
		if( line.find( "LV Name" ) == 0 )
		    {
		    if( vname.size()>0 )
			{
			addLv( num_le, vname, uuid, status, allocation,
			       readOnly );
			}
		    vname = extractNthWord( 2, line );
		    if( (pos=vname.rfind( "/" ))!=string::npos )
			vname.erase( 0, pos+1 );
		    }
		else if( line.find( "LV Write Access" ) == 0 )
		    {
		    readOnly = extractNthWord( 3, line, true ).find( "only" ) != string::npos;
		    }
		else if( line.find( "LV Status" ) == 0 )
		    {
		    status = extractNthWord( 2, line, true );
		    }
		else if( line.find( "Current LE" ) == 0 )
		    {
		    extractNthWord( 2, line ) >> num_le;
		    }
		else if( line.find( "Allocation" ) == 0 )
		    {
		    allocation = extractNthWord( 1, line );
		    }
		else if( line.find( "LV UUID" ) == 0 )
		    {
		    uuid = extractNthWord( 2, line );
		    }
		line = *c.getLine( i++ );
		}
	    if( vname.size()>0 )
		{
		addLv( num_le, vname, uuid, status, allocation, readOnly );
		}
	    Pv *p = new Pv;
	    while( i<cnt )
		{
		line.erase( 0, line.find_first_not_of( app_ws ));
		if( line.find( "PV Name" ) == 0 )
		    {
		    if( p->device.size()>0 )
			{
			addPv( p );
			}
		    p->device = extractNthWord( 2, line );
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
		    }
		line = *c.getLine( i++ );
		}
	    if( p->device.size()>0 )
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
	long size_diff = i->getLe() - (i->origSizeK()+pe_size-1)/pe_size;
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

void LvmVg::addLv( unsigned long& le, string& name, string& uuid,
                   string& status, string& alloc, bool& ro )
    {
    y2milestone( "addLv:%s", name.c_str() );
    LvmLvPair p=lvmLvPair(lvNotDeleted);
    LvmLvIter i=p.begin();
    while( i!=p.end() && i->name()!=name )
	{
	++i;
	}
    y2milestone( "addLv exists %d", i!=p.end() );
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
	i->setAlloc( alloc );
	i->getTableInfo();
	i->setReadonly(ro);
	}
    else
	{
	LvmLv *n = new LvmLv( *this, name, le, uuid, status, alloc );
	if( ro )
	    n->setReadonly();
	addToList( n );
	}
    name = uuid = status = alloc = "";
    le = 0; 
    ro = false;
    }

void LvmVg::addPv( Pv*& p )
    {
    list<Pv>::iterator i = find( pv.begin(), pv.end(), *p );
    if( i != pv.end() )
	*i = *p;
    else
	{
	i = find( pv_add.begin(), pv_add.end(), *p );
	if( i!=pv_add.end() )
	    pv_add.erase(i);
	pv.push_back( *p );
	}
    getStorage()->setUsedBy( p->device, UB_LVM, name() );
    p = new Pv;
    }

int LvmVg::getToCommit( CommitStage stage, list<Container*>& col,
                        list<Volume*>& vol )
    {
    int ret = 0;
    unsigned long oco = col.size(); 
    unsigned long ovo = vol.size(); 
    Container::getToCommit( stage, col, vol );
    if( stage==DECREASE )
        {
	if( pv_remove.size()>0 && 
	    find( col.begin(), col.end(), this )==col.end() )
	    {
	    col.push_back( this );
	    }
        }
    else if( stage==INCREASE )
        {
	if( pv_add.size()>0 && 
	    find( col.begin(), col.end(), this )==col.end() )
	    {
	    col.push_back( this );
	    }
        }
    if( col.size()!=oco || vol.size()!=ovo )
	y2milestone( "ret:%d col:%d vol:%d", ret, col.size(), vol.size());
    return( ret );
    }

int LvmVg::commitChanges( CommitStage stage )
    {
    y2milestone( "name %s stage %d", name().c_str(), stage );
    int ret = 0;
    switch( stage )
	{
	case DECREASE:
	    if( deleted() )
		{
		ret = doRemoveVg();
		}
	    else if( pv_remove.size()>0 )
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
	    else if( pv_add.size()>0 )
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void LvmVg::getCommitActions( list<commitAction*>& l ) const
    {
    Container::getCommitActions( l );
    if( deleted() )
	{
	l.push_back( new commitAction( DECREASE, staticType(), 
				       removeVgText(false), true, true ));
	}
    else if( created() )
	{
	l.push_front( new commitAction( INCREASE, staticType(), 
				        createVgText(false), true, true ));
	}
    else 
	{
	if( pv_add.size()>0 )
	    for( list<Pv>::const_iterator i=pv_add.begin(); i!=pv_add.end(); 
	         ++i )
		l.push_back( new commitAction( INCREASE, staticType(),
					       extendVgText(false,i->device), 
					       true, true ));
	if( pv_remove.size()>0 )
	    for( list<Pv>::const_iterator i=pv_remove.begin(); 
	         i!=pv_remove.end(); ++i )
		l.push_back( new commitAction( DECREASE, staticType(),
					       reduceVgText(false,i->device), 
					       false, true ));
	}
    }

string 
LvmVg::removeVgText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. system),
        txt = sformat( _("Removing Volume group %1$s"), name().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. system),
        txt = sformat( _("Remove Volume group %1$s"), name().c_str() );
        }
    return( txt );
    }

string 
LvmVg::createVgText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. system),
        txt = sformat( _("Creating Volume group %1$s"), name().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. system),
        txt = sformat( _("Create Volume group %1$s"), name().c_str() );
        }
    return( txt );
    }

string 
LvmVg::extendVgText( bool doing, const string& dev ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Extending Volume group %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Extend Volume group %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    return( txt );
    }

string 
LvmVg::reduceVgText( bool doing, const string& dev ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Reducing Volume group %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Reduce Volume group %1$s by %2$s"), name().c_str(),
	               dev.c_str() );
        }
    return( txt );
    }

void 
LvmVg::init()
    {
    pe_size = num_pe = free_pe = 0;
    lvm1 = false;
    }


void LvmVg::activate( bool val )
    {
    y2milestone( "old active:%d val:%d", lvm_active, val );
    if( lvm_active!=val )
	{
	SystemCmd c;
	if( lvm_active )
	    {
	    c.execute( "vgscan --mknodes" );
	    c.execute( "vgchange -a y" );
	    }
	else
	    {
	    c.execute( "vgchange -a n" );
	    }
	lvm_active = val;
	}
    }

void LvmVg::getVgs( list<string>& l )
    {
    l.clear();
    string vgname;
    string::size_type pos;
    SystemCmd c( "vgdisplay -s" );
    if( !lvm_active && c.numLines()>0 )
	lvm_active = true;
    for( unsigned i=0; i<c.numLines(); ++i )
	{
	vgname = *c.getLine(i);
	pos=vgname.find_first_not_of( app_ws+"\"" );
	if( pos>0 )
	    vgname.erase( 0, pos );
	pos=vgname.find_first_of( app_ws+"\"" );
	if( pos>0 )
	    vgname.erase( pos );
	l.push_back(vgname);
	}
    y2mil( "detected Vgs " << l );
    }

unsigned long
LvmVg::leByLvRemove() const
    {
    unsigned long ret=0;
    ConstLvmLvPair p=lvmLvPair(lvDeleted);
    for( ConstLvmLvIter i=p.begin(); i!=p.end(); ++i )
	ret += i->getLe();
    y2milestone( "ret:%lu", ret );
    return( ret );
    }

bool
LvmVg::checkConsistency() const
    {
    bool ret = true;
    unsigned long sum = 0;
    ConstLvmLvPair lp=lvmLvPair(lvNotDeleted);
    map<string,unsigned long> peg;
    map<string,unsigned long>::iterator mi;
    for( ConstLvmLvIter l = lp.begin(); l!=lp.end(); ++l )
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

int
LvmVg::doCreateVg()
    {
    y2milestone( "Vg:%s", name().c_str() );
    int ret = 0;
    if( created() )
	{
	checkConsistency();
	if( !silent )
	    {
	    getStorage()->showInfoCb( createVgText(true) );
	    }
	string devices;
	list<Pv>::iterator p = pv_add.begin();
	while( ret==0 && p!=pv_add.end() )
	    {
	    if( devices.size()>0 )
		devices += " ";
	    devices += p->device;
	    ret = doCreatePv( p->device );
	    ++p;
	    }
	if( ret==0 )
	    {
	    string cmd = "vgcreate " + instSysString() + metaString() + 
	                 "-s " + decString(pe_size) + "k " + name() + " " + devices;
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
	    if( pv_add.size()>0 )
		{
		pv_add.clear();
		ret = LVM_PV_STILL_ADDED;
		}
	    checkConsistency();
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
LvmVg::doRemoveVg()
    {
    y2milestone( "Vg:%s", name().c_str() );
    int ret = 0;
    if( deleted() )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( removeVgText(true) );
	    }
	checkConsistency();
	string cmd = "vgremove " + instSysString() + name();
	SystemCmd c( cmd );
	if( c.retcode()!=0 )
	    ret = LVM_VG_REMOVE_FAILED;
	if( ret==0 )
	    {
	    setDeleted( false );
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
LvmVg::doExtendVg()
    {
    y2milestone( "Vg:%s", name().c_str() );
    int ret = 0;
    list<string> devs;
    list<Pv>::iterator p;
    for( p=pv_add.begin(); p!=pv_add.end(); ++p )
	devs.push_back( p->device );
    list<string>::iterator d = devs.begin();
    while( ret==0 && d!=devs.end() )
	{
	checkConsistency();
	if( !silent )
	    {
	    getStorage()->showInfoCb( extendVgText(true,*d) );
	    }
	ret = doCreatePv( *d );
	if( ret==0 )
	    {
	    string cmd = "vgextend " + instSysString() + name() + " " + *d;
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		ret = LVM_VG_EXTEND_FAILED;
	    }
	if( ret==0 )
	    {
	    getVgData( name() );
	    checkConsistency();
	    }
	p = find( pv_add.begin(), pv_add.end(), *d );
	if( p!=pv_add.end() )
	    {
	    pv_add.erase( p );
	    if( ret==0 )
		ret = LVM_PV_STILL_ADDED;
	    }
	++d;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
LvmVg::doReduceVg()
    {
    y2milestone( "Vg:%s", name().c_str() );
    int ret = 0;
    list<string> devs;
    list<Pv>::iterator p;
    for( p=pv_remove.begin(); p!=pv_remove.end(); ++p )
	devs.push_back( p->device );
    list<string>::iterator d = devs.begin();
    while( ret==0 && d!=devs.end() )
	{
	checkConsistency();
	if( !silent )
	    {
	    getStorage()->showInfoCb( reduceVgText(true,*d) );
	    }
	string cmd = "vgreduce " + instSysString() + name() + " " + *d;
	SystemCmd c( cmd );
	if( c.retcode()!=0 )
	    ret = LVM_VG_REDUCE_FAILED;
	if( ret==0 )
	    {
	    getVgData( name() );
	    checkConsistency();
	    }
	p = find( pv_remove.begin(), pv_remove.end(), *d );
	if( p!=pv_remove.end() )
	    pv_remove.erase( p );
	else if( ret==0 )
	    ret = LVM_PV_REMOVE_NOT_FOUND;
	++d;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int 
LvmVg::doCreate( Volume* v ) 
    {
    y2milestone( "Vg:%s name:%s", name().c_str(), v->name().c_str() );
    LvmLv * l = dynamic_cast<LvmLv *>(v);
    int ret = 0;
    if( l != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->createText(true) );
	    }
	checkConsistency();
	string cmd = "lvcreate " + instSysString() + " -l " + decString(l->getLe());
	if( l->stripes()>1 )
	    cmd += " -i " + decString(l->stripes());
	cmd += " -n " + l->name();
	cmd += " " + name();
	SystemCmd c( cmd );
	if( c.retcode()!=0 )
	    ret = LVM_LV_CREATE_FAILED;
	if( ret==0 )
	    {
	    getVgData( name() );
	    checkConsistency();
	    }
	}
    else
	ret = LVM_CREATE_LV_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int LvmVg::doRemove( Volume* v )
    {
    y2milestone( "Vg:%s name:%s", name().c_str(), v->name().c_str() );
    LvmLv * l = dynamic_cast<LvmLv *>(v);
    int ret = 0;
    if( l != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->removeText(true) );
	    }
	checkConsistency();
	ret = v->prepareRemove();
	if( ret==0 )
	    {
	    string cmd = "lvremove -f " + instSysString() + " " + l->device();
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		ret = LVM_LV_REMOVE_FAILED;
	    }
	if( ret==0 )
	    {
	    if( !removeFromList( l ) )
		ret = LVM_LV_NOT_IN_LIST;
	    getVgData( name() );
	    checkConsistency();
	    }
	}
    else
	ret = LVM_REMOVE_LV_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int LvmVg::doResize( Volume* v ) 
    {
    y2milestone( "Vg:%s name:%s", name().c_str(), v->name().c_str() );
    LvmLv * l = dynamic_cast<LvmLv *>(v);
    int ret = 0;
    if( l != NULL )
	{
	FsCapabilities caps;
	bool remount = false;
	unsigned long new_le = l->getLe();
	unsigned long old_le = (v->origSizeK()+pe_size-1)/pe_size;
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
	    string cmd = "lvreduce -f " + instSysString() + 
	                 " -l -" + decString(old_le-new_le) + " " + l->device();
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		ret = LVM_LV_RESIZE_FAILED;
	    }
	if( ret==0 && old_le<new_le )
	    {
	    string cmd = "lvextend " + instSysString() + 
	                 " -l +" + decString(new_le-old_le) + " " + l->device();
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		ret = LVM_LV_RESIZE_FAILED;
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string LvmVg::metaString()
    {
    return( (lvm1)?"-M1 ":"-M2 " );
    }

string LvmVg::instSysString()
    {
    string ret;
    if( getStorage()->instsys() )
	ret = "-A n ";
    return( ret );
    }

int LvmVg::doCreatePv( const string& device )
    {
    int ret = 0;
    y2milestone( "dev:%s", device.c_str() );
    SystemCmd c;
    string cmd = "mdadm --zero-superblock " + device;
    c.execute( cmd );
    cmd = "pvcreate -ff " + metaString() + device;
    c.execute( cmd );
    if( c.retcode()!=0 )
	ret = LVM_CREATE_PV_FAILED;
    y2milestone( "ret:%d", ret );
    return( ret );
    }


void LvmVg::logData( const string& Dir ) {;}

bool LvmVg::lvm_active = false;

