/*
  Textdomain    "storage"
*/

#include <errno.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <iostream> 
#include <sstream> 

#include "y2storage/StorageTypes.h"
#include "y2storage/EvmsCo.h"
#include "y2storage/Evms.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

using namespace std;
using namespace storage;

EvmsCo::EvmsCo( Storage * const s, const EvmsTree& data ) :
    PeContainer(s,staticType())
    {
    nm = "";
    y2milestone( "constructing volume evms co" );
    init();
    getNormalVolumes( data );
    }

EvmsCo::EvmsCo( Storage * const s, const EvmsCont& cont, const EvmsTree& data ) :
    PeContainer(s,staticType())
    {
    nm = cont.name;
    y2milestone( "constructing evms co %s lvm1:%d", nm.c_str(), cont.lvm1 );
    init();
    lvm1 = cont.lvm1;
    getCoData( cont.name, data, false );
    }

EvmsCo::EvmsCo( Storage * const s, const string& name, bool lv1 ) :
    PeContainer(s,staticType())
    {
    if( lv1 )
	nm = "lvm/" + name;
    else
	nm = "lvm2/" + name;
    y2milestone( "constructing evms co %s lvm1:%d", nm.c_str(), lv1 );
    init();
    lvm1 = lv1;
    }



EvmsCo::EvmsCo( Storage * const s, const string& file, int ) :
    PeContainer(s,staticType())
    {
    y2milestone( "constructing evms co %s from file %s", dev.c_str(), 
                 file.c_str() );
    }

EvmsCo::~EvmsCo()
    {
    y2milestone( "destructed evms co %s", dev.c_str() );
    }

static bool lvDeleted( const Evms& l ) { return( l.deleted() ); }
static bool lvCreated( const Evms& l ) { return( l.created() ); }
static bool lvResized( const Evms& l ) { return( l.extendSize()!=0 ); }

int
EvmsCo::removeCo()
    {
    int ret = 0;
    y2milestone( "begin" );
    if( readonly() )
	{
	ret = EVMS_CHANGE_READONLY;
	}
    if( ret==0 && !created() )
	{
	EvmsPair p=evmsPair(lvNotDeleted);
	for( EvmsIter i=p.begin(); i!=p.end(); ++i )
	    ret = removeVol( i->name() );
	setDeleted( true );
	}
    if( ret==0 )
	{
	for( list<Pv>::const_iterator s=pv.begin(); s!=pv.end(); ++s )
	    getStorage()->setUsedBy( s->device, UB_NONE, "" );
	for( list<Pv>::const_iterator s=pv_add.begin(); s!=pv_add.end(); ++s )
	    getStorage()->setUsedBy( s->device, UB_NONE, "" );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
EvmsCo::extendCo( const string& dev )
    {
    list<string> l;
    l.push_back( dev );
    return( extendCo( l ) );
    }

int
EvmsCo::extendCo( const list<string>& devs )
    {
    int ret = 0;
    y2mil( "name:" << name() << " devices:" << devs );

    checkConsistency();
    list<string>::const_iterator i=devs.begin();
    list<Pv>::iterator p;
    if( readonly() )
	{
	ret = EVMS_CHANGE_READONLY;
	}
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	if( (p=find( pv.begin(), pv.end(), d ))!=pv.end() || 
	    (p=find( pv_add.begin(), pv_add.end(), d ))!=pv_add.end())
	    ret = EVMS_PV_ALREADY_CONTAINED;
	else if( (p=find( pv_remove.begin(), pv_remove.end(), d )) != 
	         pv_remove.end() )
	    {
	    }
	else if( !getStorage()->knownDevice( d, true ) )
	    {
	    ret = EVMS_PV_DEVICE_UNKNOWN;
	    }
	else if( !getStorage()->canUseDevice( d, true ) )
	    {
	    ret = EVMS_PV_DEVICE_USED;
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
	    getStorage()->setUsedBy( d, UB_EVMS, name() );
	    d = "/dev/evms/"+dev.substr(5);
	    getStorage()->setUsedBy( d, UB_EVMS, name() );
	    }
	else
	    {
	    Pv pvn;
	    unsigned long long s = getStorage()->deviceSize( d );
	    pe = (s - 500)/pe_size;
	    pvn.num_pe = pvn.free_pe = pe;
	    pvn.device = d;
	    pv_add.push_back( pvn );
	    getStorage()->changeFormatVolume( d, false, FSNONE );
	    getStorage()->setUsedBy( d, UB_EVMS, name() );
	    d = "/dev/evms/"+dev.substr(5);
	    getStorage()->changeFormatVolume( d, false, FSNONE );
	    getStorage()->setUsedBy( d, UB_EVMS, name() );
	    }
	free_pe += pe;
	num_pe += pe;
	++i;
	}
    if( ret==0 && pv_add.size()+pv.size()-pv_remove.size()<=0 )
	ret = EVMS_CO_HAS_NONE_PV;
    if( ret==0 )
	checkConsistency();
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
EvmsCo::reduceCo( const string& dev )
    {
    list<string> l;
    l.push_back( dev );
    return( reduceCo( l ) );
    }

int
EvmsCo::reduceCo( const list<string>& devs )
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
	ret = EVMS_CHANGE_READONLY;
	}
    while( ret==0 && i!=devs.end() )
	{
	string d = normalizeDevice( *i );
	ret = tryUnusePe( d, pl, pladd, plrem, rem_pe );
	++i;
	}
    if( ret==0 && pv_add.size()+pv.size()-pv_remove.size()<=0 )
	ret = EVMS_CO_HAS_NONE_PV;
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
EvmsCo::createVol( const string& name, unsigned long long sizeK, 
                   unsigned stripe, string& device )
    {
    int ret = 0;
    y2milestone( "name:%s sizeK:%llu stripe:%u", name.c_str(), sizeK, stripe );
    checkConsistency();
    if( readonly() )
	{
	ret = EVMS_CHANGE_READONLY;
	}
    if( ret==0 && name.find( "\"\' /\n\t:*?" )!=string::npos )
	{
	ret = EVMS_LV_INVALID_NAME;
	}
    if( ret==0 )
	{
	EvmsPair p=evmsPair(lvNotDeleted);
	EvmsIter i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if( i!=p.end() )
	    ret = EVMS_LV_DUPLICATE_NAME;
	}
    unsigned long num_le = (sizeK + pe_size - 1)/pe_size;
    if( stripe>1 )
	num_le = ((num_le+stripe-1)/stripe)*stripe;
    if( ret==0 && free_pe<num_le )
	{
	ret = EVMS_LV_NO_SPACE;
	}
    map<string,unsigned long> pe_map;
    if( ret==0 )
	ret = addLvPeDistribution( num_le, stripe, pv, pv_add, pe_map );
    if( ret==0 )
	{
	Evms* l = new Evms( *this, name, num_le, stripe );
	l->setCreated( true );
	l->setPeMap( pe_map );
	device = l->device();
	free_pe -= num_le;
	addToList( l );
	}
    if( ret==0 )
	checkConsistency();
    y2milestone( "ret:%d device:%s", ret, ret?"":device.c_str() );
    return( ret );
    }

int EvmsCo::resizeVolume( Volume* v, unsigned long long newSize )
    {
    int ret = 0;
    y2milestone( "newSizeK:%llu vol:%s", newSize, v->name().c_str() );
    checkConsistency();
    if( readonly() )
	{
	ret = EVMS_CHANGE_READONLY;
	}
    else 
	{
	Evms * l = dynamic_cast<Evms *>(v);
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
	    ret = EVMS_CHECK_RESIZE_INVALID_VOLUME;
	    }
	}
    if( ret==0 )
	checkConsistency();
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EvmsCo::removeVolume( Volume* v )
    {
    return( removeVol( v->name() ));
    }

int 
EvmsCo::removeVol( const string& name )
    {
    int ret = 0;
    y2milestone( "name:%s", name.c_str() );
    EvmsIter i;
    checkConsistency();
    if( readonly() )
	{
	ret = EVMS_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	EvmsPair p=evmsPair(lvNotDeleted);
	i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if( i==p.end() )
	    ret = EVMS_LV_UNKNOWN_NAME;
	}
    if( ret==0 && i->getUsedByType() != UB_NONE )
	{
	if( getStorage()->getRecursiveRemoval() )
	    ret = getStorage()->removeUsing( i->device(), i->getUsedBy() );
	else
	    ret = EVMS_LV_REMOVE_USED_BY;
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
		ret = EVMS_LV_NOT_IN_LIST;
	    }
	else
	    i->setDeleted( true );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int 
EvmsCo::changeStripeSize( const string& name, unsigned long long stripeSize )
    {
    int ret = 0;
    y2milestone( "name:%s stripeSize:%llu", name.c_str(), stripeSize );
    EvmsIter i;
    checkConsistency();
    if( readonly() )
	{
	ret = EVMS_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	EvmsPair p=evmsPair(lvNotDeleted);
	i=p.begin();
	while( i!=p.end() && i->name()!=name )
	    ++i;
	if( i==p.end() )
	    ret = EVMS_LV_UNKNOWN_NAME;
	}
    if( ret==0 && !i->created() )
	{
	ret = EVMS_LV_ALREADY_ON_DISK;
	}
    if( ret==0 && i->stripes()<=1 )
	{
	ret = EVMS_LV_NO_STRIPE_SIZE;
	}
    if( ret==0 )
	{
	i->setStripeSize( stripeSize );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void EvmsCo::getCoData( const string& name, const EvmsTree& data, bool check )
    {
    y2milestone( "name:%s check:%d", name.c_str(), check );
    list<EvmsCont>::const_iterator co=data.cont.begin(); 
    while( co!=data.cont.end() && name!=co->name )
	++co;
    if( co == data.cont.end() )
	{
	y2error( "container %s not found", name.c_str() );
	}
    else
	{
	y2milestone( "name:%s", co->name.c_str() );
	if( check && co->lvm1 != lvm1 )
	    y2warning( "inconsistent lvm1 my:%d evms:%d", lvm1, co->lvm1 );
	lvm1 = co->lvm1;
	if( check && co->peSize != pe_size )
	    y2warning( "inconsistent pe_size my:%llu lvm:%llu", 
		       pe_size, co->peSize );
	uuid = co->uuid;
	pe_size = co->peSize;
	num_pe = co->sizeK/pe_size;
	free_pe = co->free/pe_size;
	for( list<unsigned>::const_iterator i=co->creates.begin(); 
	     i!=co->creates.end(); ++i )
	    {
	    map<unsigned,EvmsObj>::const_iterator oi = data.objects.find( *i );
	    if( oi!=data.objects.end() )
		{
		map<unsigned,EvmsVol>::const_iterator mi = 
		    data.volumes.find( oi->second.vol );
		if( mi!=data.volumes.end() )
		    {
		    addLv( mi->second.sizeK/pe_size, mi->second.name, mi->second.native );
		    }
		else
		    y2warning( "volume not found:%u", oi->second.vol );
		}
	    else
		y2warning( "object not found:%u", *i );
	    }
	for( list<EvmsCont::peinfo>::const_iterator i=co->consumes.begin(); 
	     i!=co->consumes.end(); ++i )
	    {
	    map<unsigned,EvmsObj>::const_iterator oi = data.objects.find( i->id );
	    if( oi!=data.objects.end() )
		{
		if( oi->second.vol>0 )
		    {
		    map<unsigned,EvmsVol>::const_iterator mi = 
			data.volumes.find( oi->second.vol );
		    if( mi!=data.volumes.end() )
			{
			Pv p;
			p.device = unEvmsDevice( mi->second.device );
			p.status = "allocatable";
			p.uuid = i->uuid;
			p.num_pe = i->size;
			p.free_pe = i->free;
			addPv( &p );
			}
		    else
			y2warning( "volume not found:%u", oi->second.vol );
		    }
		else
		    {
		    Pv p;
		    p.device = "/dev/" + oi->second.name;
		    p.status = "allocatable";
		    p.uuid = i->uuid;
		    p.num_pe = i->size;
		    p.free_pe = i->free;
		    addPv( &p );
		    }
		}
	    else
		y2warning( "object not found:%u", i->id );
	    }

	EvmsPair p=evmsPair(lvDeleted);
	for( EvmsIter i=p.begin(); i!=p.end(); ++i )
	    {
	    //cout << "Deleted:" << *i << endl;
	    map<string,unsigned long> pe_map = i->getPeMap();
	    remLvPeDistribution( i->getLe(), pe_map, pv, pv_add );
	    free_pe += i->getLe();
	    }
	p=evmsPair(lvCreated);
	for( EvmsIter i=p.begin(); i!=p.end(); ++i )
	    {
	    //cout << "Created:" << *i << endl;
	    map<string,unsigned long> pe_map;
	    if( addLvPeDistribution( i->getLe(), i->stripes(), pv, pv_add, 
				     pe_map ) == 0 )
		i->setPeMap( pe_map );
	    free_pe -= i->getLe();
	    }
	p=evmsPair(lvResized);
	for( EvmsIter i=p.begin(); i!=p.end(); ++i )
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
    }

void EvmsCo::getNormalVolumes( const EvmsTree& data )
    {
    y2milestone( "begin" );
    map<unsigned,EvmsVol>::const_iterator v=data.volumes.begin(); 
    while( v!=data.volumes.end() )
	{
	pe_size = 1;
	if( v->second.name.find( "lvm/" )!=0 && 
	    v->second.name.find( "lvm2/" )!=0 )
	    {
	    addLv( v->second.sizeK, v->second.name, v->second.native );
	    if( v->second.native )
		{
		map<unsigned,EvmsObj>::const_iterator oi = 
		    data.objects.find( v->second.uses );
		if( oi!=data.objects.end() )
		    {
		    string s = normalizeDevice(oi->second.name);
		    getStorage()->setUsedBy( s, UB_EVMS, v->second.name );
		    s = undevDevice(unEvmsDevice(s));
		    s = "/dev/evms/" + s;
		    getStorage()->setUsedBy( s, UB_EVMS, v->second.name );
		    }
		}
	    }
	++v;
	}
    y2milestone( "end" );
    }

void EvmsCo::addLv( unsigned long le, const string& name, bool native )
    {
    y2milestone( "addLv:%s le:%lu", name.c_str(), le );
    string n( name );
    string::size_type pos = n.rfind( '/' );
    if( pos!=string::npos )
	{
	n.erase( 0, pos+1 );
	}
    EvmsPair p=evmsPair(lvNotDeleted);
    EvmsIter i=p.begin();
    while( i!=p.end() && i->name()!=n )
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
	i->getTableInfo();
	}
    else
	{
	Evms *e = new Evms( *this, n, le, native );
	addToList( e );
	}
    }

string EvmsCo::unEvmsDevice( const string& dev )
    {
    string ret( dev );
    if( ret.find( "/dev/evms/" )==0 )
	ret.erase( 5, 5 );
    return( ret );
    }

void EvmsCo::addPv( const Pv* p )
    {
    PeContainer::addPv( p );
    if( !deleted() && 
        find( pv_remove.begin(), pv_remove.end(), *p )==pv_remove.end() )
	{
	string dev = unEvmsDevice( p->device );
	UsedByType t = getStorage()->usedBy( dev );
	if( t==UB_EVMS || t==UB_NONE )
	    getStorage()->setUsedBy( dev, UB_EVMS, name() );
	dev = "/dev/evms/"+dev.substr(5);
	t = getStorage()->usedBy( dev );
	if( t==UB_EVMS || t==UB_NONE )
	    getStorage()->setUsedBy( dev, UB_EVMS, name() );
	}
    }

int EvmsCo::getToCommit( CommitStage stage, list<Container*>& col,
                        list<Volume*>& vol )
    {
    int ret = 0;
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
	y2milestone( "ret:%d col:%d vol:%d", ret, col.size(), vol.size());
    return( ret );
    }

int EvmsCo::commitChanges( CommitStage stage )
    {
    y2milestone( "name %s stage %d", name().c_str(), stage );
    int ret = 0;
    switch( stage )
	{
	case DECREASE:
	    if( deleted() )
		{
		ret = doRemoveCo();
		}
	    else if( !pv_remove.empty() )
		{
		ret = doReduceCo();
		}
	    else 
		ret = EVMS_COMMIT_NOTHING_TODO;
	    break;
	case INCREASE:
	    if( created() )
		{
		ret = doCreateCo();
		}
	    else if( !pv_add.empty() )
		{
		ret = doExtendCo();
		}
	    else
		ret = EVMS_COMMIT_NOTHING_TODO;
	    break;
	default:
	    ret = EVMS_COMMIT_NOTHING_TODO;
	    break;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void EvmsCo::getCommitActions( list<commitAction*>& l ) const
    {
    Container::getCommitActions( l );
    if( deleted() )
	{
	l.push_back( new commitAction( DECREASE, staticType(), 
				       removeCoText(false), true, true ));
	}
    else if( created() )
	{
	l.push_front( new commitAction( INCREASE, staticType(), 
				        createCoText(false), false, true ));
	}
    else 
	{
	if( !pv_add.empty() )
	    for( list<Pv>::const_iterator i=pv_add.begin(); i!=pv_add.end(); 
	         ++i )
		l.push_back( new commitAction( INCREASE, staticType(),
					       extendCoText(false,i->device), 
					       true, true ));
	if( !pv_remove.empty() )
	    for( list<Pv>::const_iterator i=pv_remove.begin(); 
	         i!=pv_remove.end(); ++i )
		l.push_back( new commitAction( DECREASE, staticType(),
					       reduceCoText(false,i->device), 
					       false, true ));
	}
    }

string 
EvmsCo::removeCoText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. lvm2/system),
        txt = sformat( _("Removing container %1$s"), name().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. lvm2/system),
        txt = sformat( _("Remove container %1$s"), name().c_str() );
        }
    return( txt );
    }

string 
EvmsCo::createCoText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. lvm2/system),
        txt = sformat( _("Creating container %1$s"), name().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. lvm2/system),
        txt = sformat( _("Create container %1$s"), name().c_str() );
        }
    return( txt );
    }

string 
EvmsCo::extendCoText( bool doing, const string& dev ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. lvm2/system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Extending container %1$s with %2$s"), name().c_str(),
	               dev.c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. lvm2/system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Extend container %1$s with %2$s"), name().c_str(),
	               dev.c_str() );
        }
    return( txt );
    }

string 
EvmsCo::reduceCoText( bool doing, const string& dev ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. lvm2/system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Reducing container %1$s with %2$s"), name().c_str(),
	               dev.c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. lvm2/system),
	// %2$s is replaced by a device name (e.g. /dev/hda1)
        txt = sformat( _("Reduce container %1$s with %2$s"), name().c_str(),
	               dev.c_str() );
        }
    return( txt );
    }

void 
EvmsCo::init()
    {
    dev = "evms";
    if( !nm.empty() )
	dev += "/" + nm;
    normalizeDevice(dev);
    pe_size = num_pe = free_pe = 0;
    lvm1 = false;
    }

#define SOCKET_PATH "/var/lib/YaST2/socket_libstorage_evms"
#define EXEC_PATH   "/usr/lib/YaST2/bin/evms_access"

int EvmsCo::getSocketFd()
    {
    if( sockfd>=0 )
	close( sockfd );
    int fd = -1;
    struct sockaddr_un s_addr;
    if( strlen(SOCKET_PATH)>=sizeof(s_addr.sun_path) )
	{
	y2error( "socket path too large %s", SOCKET_PATH );
	}
    else
	{
	memset( &s_addr, 0, sizeof(s_addr));
	s_addr.sun_family = AF_UNIX;
	(void)strncpy(s_addr.sun_path, SOCKET_PATH, sizeof(s_addr.sun_path));
	s_addr.sun_path[sizeof(s_addr.sun_path)-1] = 0;

	if( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0 ) 
	    {
	    fd = -1;
	    y2error( "socket call failed errno=%d (%s)", errno, strerror(errno) );
	    }
	else if( connect( fd, (struct sockaddr *) &s_addr, sizeof(s_addr))<0 )
	    {
	    y2error( "connect call failed errno=%d (%s)", errno, strerror(errno) );
	    if( errno==ECONNREFUSED )
		{
		close( fd );
		unlink( SOCKET_PATH );
		startHelper(true);
		usleep( 1000000 );
		if( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0 ) 
		    y2error( "socket call failed errno=%d (%s)", errno, strerror(errno) );
		else if( connect( fd, (struct sockaddr *) &s_addr, sizeof(s_addr))<0 )
		    {
		    y2error( "connect call failed errno=%d (%s)", errno, strerror(errno) );
		    fd = -1;
		    }
		}
	    else
		fd = -1;
	    }
	y2milestone( "ret:%d", fd );
	}
    return( fd );
    }

bool EvmsCo::startHelper( bool retry )
    {
    bool ret = access( EXEC_PATH, X_OK )==0 && 
               getenv( "YAST2_STORAGE_NO_EVMS" )==NULL ;
    if( ret )
	{
	string cmd = EXEC_PATH;
	cmd += " --log-path /var/log/YaST2 --log-file y2log";
	cmd += " --socket " SOCKET_PATH;
	cmd += " --timeout 3000";
	if( retry )
	    cmd += " --retry";
	SystemCmd c;
	c.executeBackground( cmd );
	}
    unsigned count=0;
    while( ret && access( SOCKET_PATH, W_OK )!=0 && count++<1000 )
	{
	usleep( 10000 );
	}
    if( ret && access( SOCKET_PATH, W_OK )!=0 )
	{
	ret = false;
	y2error( "socket not created" );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

bool EvmsCo::attachToSocket(bool attach)
    {
    bool ret = true;
    static int semid = -1;
    if( attach && semid<0 )
	{
	if( access( EXEC_PATH, X_OK )!=0 )
	    {
	    ret = false;
	    }
	if( ret )
	    {
	    key_t k = ftok( EXEC_PATH, IPC_PROJ_ID );
	    semid = semget( k, 1, IPC_CREAT|0600 );
	    y2milestone( "ipc key:%x semid:%d", k, semid );
	    if( semid<0 )
		{
		ret = false;
		y2error( "semget failed errno=%d (%s)", errno, strerror(errno) );
		}
	    }
	if( ret )
	    {
	    struct sembuf s;
	    s.sem_num = 0;
	    s.sem_op = 1;
	    s.sem_flg = SEM_UNDO;
	    if( semop( semid, &s, 1 )<0 )
		{
		ret = false;
		y2error( "semop failed errno=%d (%s)", errno, strerror(errno) );
		}
	    }
	bool sem_incremented = ret;
	if( ret && access( SOCKET_PATH, W_OK )!=0 )
	    {
	    ret = startHelper(false);
	    }
	if( ret )
	    {
	    sockfd = getSocketFd();
	    y2milestone( "sockfd:%d", sockfd );
	    if( sockfd < 0 )
		ret = false;
	    }
	y2milestone( "ret:%d", ret );
	if( !ret && sem_incremented )
	    attachToSocket( false );
	}
    else if( !attach && semid>=0 )
	{
	struct sembuf s;
	s.sem_num = 0;
	s.sem_op = -1;
	s.sem_flg = IPC_NOWAIT;
	semop( semid, &s, 1 );
	semid = -1;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void EvmsCo::activate( bool val )
    {
    y2milestone( "old active:%d val:%d", active, val );
    if( active!=val )
	{
	if( val )
	    {
	    Dm::activate( true );
	    attachToSocket(true);
	    }
	else
	    {
	    attachToSocket(false);
	    }
	active = val;
	}
    }

bool EvmsCo::sendCommand( const string& cmd, bool one_line, list<string>& lines )
    {
    y2milestone( "cmd:%s fd:%d", cmd.c_str(), sockfd );
    bool ret = sockfd>=0;
    if( ret )
	{
	long val;
	string tmp = cmd + "\n";
	bool retry = true;
	int retry_cnt = 0;
	while( retry && retry_cnt++<2 )
	    {
	    retry = false;
	    val = write( sockfd, tmp.c_str(), tmp.size() );
	    if( (size_t)val!=tmp.size() )
		y2error( "write fd:%d ret:%ld size:%zu", sockfd, val, cmd.size() );

	    bool end = false;
	    char l[300];
	    string line;
	    lines.clear();
	    while( !end )
		{
		string::size_type pos;
		long ret = read( sockfd, l, sizeof(l)-1 );
		if( ret>0 )
		    {
		    l[ret] = 0;
		    line += l;
		    while( (pos=line.find( '\n' )) != string::npos )
			{
			end = one_line ? pos!=string::npos : pos==0;
			if( !end || one_line )
			    lines.push_back( line.substr( 0, pos ) );
			line.erase( 0, pos+1 );
			}
		    if( ret<long(sizeof(l)-1) && !end )
			usleep( 10000 );
		    }
		else 
		    {
		    y2error( "read failed ret:%ld errno=%d (%s)", ret, 
		             errno, strerror(errno) );
		    sockfd = getSocketFd();
		    y2error( "retrying read sockfd:%d", sockfd );
		    end = retry = true;
		    }
		}
	    }
	}
    sockfd = getSocketFd();
    y2milestone( "ret:%d", ret );
    if( ret && !lines.empty() )
	y2milestone( "line:%s", lines.front().c_str() );
    return( ret );
    }

void EvmsCo::getEvmsList( EvmsTree& data )
    {
    list<string> l;
    sendCommand( "list", false, l );
    y2milestone( "list size:%u", l.size() );
    for( list<string>::const_iterator i=l.begin(); i!=l.end(); ++i )
	{
	if( i->find( "SEG D " )==0 || i->find( "REG D " )==0 )
	    {
	    EvmsObj obj;
	    map<string,string> m = makeMap( splitString( i->substr(6) ), ":" );
	    map<string,string>::const_iterator mi = m.find( "id" );
	    if( mi != m.end() )
		mi->second >> obj.id;
	    mi = m.find( "vol" );
	    if( mi != m.end() )
		mi->second >> obj.vol;
	    mi = m.find( "name" );
	    if( mi != m.end() )
		obj.name = mi->second;
	    data.objects[obj.id] = obj;
	    }
	}
    for( list<string>::const_iterator i=l.begin(); i!=l.end(); ++i )
	{
	if( i->find( "VOL " )==0 )
	    {
	    EvmsVol obj;
	    map<string,string> m = makeMap( splitString( i->substr(4) ), ":" );
	    map<string,string>::const_iterator mi = m.find( "id" );
	    if( mi != m.end() )
		mi->second >> obj.id;
	    mi = m.find( "size" );
	    if( mi != m.end() )
		mi->second >> obj.sizeK;
	    mi = m.find( "name" );
	    if( mi != m.end() )
		obj.name = mi->second;
	    mi = m.find( "device" );
	    if( mi != m.end() )
		obj.device = mi->second;
	    mi = m.find( "native" );
	    if( mi != m.end() && mi->second=="1" )
		obj.native = true;
	    if( obj.native )
		{
		mi = m.find( "consumes" );
		if( mi != m.end() )
		    mi->second >> obj.uses;
		}
	    data.volumes[obj.id] = obj;
	    }
	}
    for( list<string>::const_iterator i=l.begin(); i!=l.end(); ++i )
	{
	if( i->find( "CNT " )==0 )
	    {
	    EvmsCont obj;
	    map<string,string> m = makeMap( splitString( i->substr(4) ), ":" );
	    map<string,string>::const_iterator mi = m.find( "size" );
	    if( mi != m.end() )
		mi->second >> obj.sizeK;
	    mi = m.find( "free" );
	    if( mi != m.end() )
		mi->second >> obj.free;
	    mi = m.find( "pesize" );
	    if( mi != m.end() )
		mi->second >> obj.peSize;
	    mi = m.find( "name" );
	    if( mi != m.end() )
		obj.name = mi->second;
	    mi = m.find( "uuid" );
	    if( mi != m.end() )
		obj.uuid = mi->second;
	    mi = m.find( "type" );
	    if( mi != m.end() && mi->second=="LVM2" )
		obj.lvm1 = false;
	    string::size_type pos = i->find( "consumes:" );
	    if( pos != string::npos )
		{
		pos += 10;
		list<string> tmp = 
		    splitString( i->substr( pos, i->find( ">", pos )-pos));
		list<string>::const_iterator i=tmp.begin();
		while( i!=tmp.end() )
		    {
		    EvmsCont::peinfo pe;
		    pe.id = 0;
		    list<string> fields = splitString( *i, "," );
		    list<string>::const_iterator f = fields.begin();
		    *f >> pe.id;
		    f++;
		    *f >> pe.size;
		    f++;
		    *f >> pe.free;
		    f++;
		    pe.uuid = *f;
		    if( pe.id>0 )
			obj.consumes.push_back(pe);
		    ++i;
		    }
		}
	    pos = i->find( "creates:" );
	    if( pos != string::npos )
		{
		pos += 9;
		list<string> tmp = 
		    splitString( i->substr( pos, i->find( ">", pos )-pos));
		list<string>::const_iterator i=tmp.begin();
		while( i!=tmp.end() )
		    {
		    unsigned id = 0;
		    *i >> id;
		    if( id>0 )
			obj.creates.push_back(id);
		    ++i;
		    }
		}
	    data.cont.push_back( obj );
	    }
	}
    }

int EvmsCo::executeCmd( const string& cmd )
    {
    int ret = EVMS_COMMUNICATION_FAILED;
    list<string> l;
    sendCommand( cmd, true, l );
    y2milestone( "list size:%u", l.size() );
    if( !l.empty() )
	{
	y2milestone( "ret:%s", l.front().c_str() );
	l.front() >> ret;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
EvmsCo::doCreateCo()
    {
    y2milestone( "Co:%s", name().c_str() );
    int ret = 0;
    if( created() )
	{
	checkConsistency();
	if( !silent )
	    {
	    getStorage()->showInfoCb( createCoText(true) );
	    }
	string devices;
	list<Pv>::iterator p = pv_add.begin();
	while( p!=pv_add.end() )
	    {
	    if( !devices.empty() )
		devices += " ";
	    devices += p->device;
	    ++p;
	    }
	string cmd = "create_co " + name() + " " + decString(pe_size) + " ";
	if( lvm1 )
	    cmd += "0";
	else
	    cmd += "1";
	cmd +=  " " + devices;
	ret = executeCmd( cmd );
	if( ret==0 )
	    {
	    setCreated( false );
	    EvmsTree t;
	    getEvmsList( t );
	    getCoData( name(), t, true );
	    if( !pv_add.empty() )
		{
		pv_add.clear();
		ret = EVMS_PV_STILL_ADDED;
		}
	    checkConsistency();
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
EvmsCo::doRemoveCo()
    {
    y2milestone( "Co:%s", name().c_str() );
    int ret = 0;
    if( deleted() )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( removeCoText(true) );
	    }
	string cmd = "delete_co " + name();
	ret = executeCmd( cmd );
	if( ret==0 )
	    {
	    setDeleted( false );
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
EvmsCo::doExtendCo()
    {
    y2milestone( "Co:%s", name().c_str() );
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
	    getStorage()->showInfoCb( extendCoText(true,*d) );
	    }
	if( ret==0 )
	    {
	    string cmd = "extend_co " + name() + " " + *d;
	    ret = executeCmd( cmd );
	    }
	if( ret==0 )
	    {
	    EvmsTree t;
	    getEvmsList( t );
	    getCoData( name(), t, true );
	    checkConsistency();
	    }
	p = find( pv_add.begin(), pv_add.end(), *d );
	if( p!=pv_add.end() )
	    {
	    pv_add.erase( p );
	    if( ret==0 )
		ret = EVMS_PV_STILL_ADDED;
	    }
	++d;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
EvmsCo::doReduceCo()
    {
    y2milestone( "Co:%s", name().c_str() );
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
	    getStorage()->showInfoCb( reduceCoText(true,*d) );
	    }
	string cmd = "shrink_co " + name() + " " + *d;
	ret = executeCmd( cmd );
	if( ret==0 )
	    {
	    EvmsTree t;
	    getEvmsList( t );
	    getCoData( name(), t, true );
	    checkConsistency();
	    }
	p = find( pv_remove.begin(), pv_remove.end(), *d );
	if( p!=pv_remove.end() )
	    pv_remove.erase( p );
	else if( ret==0 )
	    ret = EVMS_PV_REMOVE_NOT_FOUND;
	++d;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int 
EvmsCo::doCreate( Volume* v ) 
    {
    y2milestone( "Co:%s name:%s", name().c_str(), v->name().c_str() );
    Evms * l = dynamic_cast<Evms *>(v);
    int ret = 0;
    if( l != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->createText(true) );
	    }
	checkConsistency();
	string cmd = "create_lv " + name() + " " + l->name() + " ";
	cmd += decString(l->sizeK());
	if( l->stripes()>1 )
	    {
	    cmd += " " + decString(l->stripes());
	    if( l->stripeSize()>0 )
		cmd += " " + decString(l->stripeSize());
	    }
	ret = executeCmd( cmd );
	if( ret==0 )
	    {
	    EvmsTree t;
	    getEvmsList( t );
	    getCoData( name(), t, true );
	    checkConsistency();
	    }
	}
    else
	ret = EVMS_CREATE_LV_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EvmsCo::doRemove( Volume* v )
    {
    y2milestone( "Co:%s name:%s", name().c_str(), v->name().c_str() );
    Evms * l = dynamic_cast<Evms *>(v);
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
	    string cmd = "delete_lv " + name() + " " + l->name();
	    ret = executeCmd( cmd );
	    }
	if( ret==0 )
	    {
	    if( !removeFromList( l ) )
		ret = EVMS_LV_NOT_IN_LIST;
	    EvmsTree t;
	    getEvmsList( t );
	    getCoData( name(), t, true );
	    checkConsistency();
	    }
	}
    else
	ret = EVMS_REMOVE_LV_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EvmsCo::doResize( Volume* v ) 
    {
    y2milestone( "Co:%s name:%s", name().c_str(), v->name().c_str() );
    Evms * l = dynamic_cast<Evms *>(v);
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
	if( ret==0 && old_le!=new_le )
	    {
	    string cmd = "resize_lv " + name() + " " + l->name() + " " + decString(l->sizeK());
	    ret = executeCmd( cmd );
	    }
	if( ret==0 && old_le<new_le && l->getFs()!=FSNONE )
	    ret = v->resizeFs();
	if( old_le!=new_le )
	    l->calcSize();
	if( ret==0 && remount )
	    ret = v->mount();
	if( ret==0 )
	    {
	    EvmsTree t;
	    getEvmsList( t );
	    getCoData( name(), t, true );
	    checkConsistency();
	    }
	}
    else
	ret = EVMS_RESIZE_LV_INVALID_VOLUME;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

std::ostream& operator<< (std::ostream& s, const EvmsObj& d )
    {
    s << "id:" << d.id << " name:" << d.name;
    if( d.vol!=0 )
	s << " vol:" << d.vol;
    return( s );
    }

std::ostream& operator<< (std::ostream& s, const EvmsVol& d )
    {
    s << "id:" << d.id << " name:" << d.name;
    s << " sizeK:" << d.sizeK << " device:" << d.device;
    if( d.native )
	{
	s << " native";
	if( d.uses!=0 )
	    s << " uses:" << d.uses;
	}
    return( s );
    }

std::ostream& operator<< (std::ostream& s, const EvmsCont::peinfo& d )
    {
    s << d.id << "," << d.size << "," << d.free << "," << d.uuid;
    return( s );
    }

std::ostream& operator<< (std::ostream& s, const EvmsCont& d )
    {
    s << "name:" << d.name << " lvm" << ( d.lvm1 ? "1" : "2" );
    s << " peSize:" << d.peSize << " sizeK:" << d.sizeK << " free:" << d.free;
    s << " cons:" << d.consumes;
    s << " creates:" << d.creates;
    return( s );
    }

std::ostream& operator<< (std::ostream& s, const EvmsTree& d )
    {
    std::map<unsigned,EvmsObj>::const_iterator o = d.objects.begin();
    s << "OBJECTS:" << std::endl;
    while( o != d.objects.end() )
	{
	s << o->first << " -- " << o->second << std::endl;
	++o;
	}
    std::map<unsigned,EvmsVol>::const_iterator v = d.volumes.begin();
    s << "VOLUMES:" << std::endl;
    while( v != d.volumes.end() )
	{
	s << v->first << " -- " << v->second << std::endl;
	++v;
	}
    std::list<EvmsCont>::const_iterator c = d.cont.begin();
    s << "CONTAINER:" << std::endl;
    while( c != d.cont.end() )
	{
	s << *c << std::endl;
	++c;
	}
    return( s );
    }

void EvmsCo::getInfo( EvmsCoInfo& tinfo ) const
    {
    info.sizeK = sizeK();
    info.peSize = peSize();
    info.peCount = peCount();
    info.peFree = peFree();
    info.lvm2 = lvm2();
    info.create = created();
    info.uuid = uuid;
    info.realContainer = !nm.empty();
    list<Pv>::const_iterator i=pv.begin();
    while( i!=pv.end() )
	{
	if( !info.devices.empty() )
	    info.devices += ' ';
	info.devices += i->device;
	++i;
	}
    y2mil( "device:" << info.devices );
    info.devices_add.clear();
    i=pv_add.begin();
    while( i!=pv_add.end() )
	{
	if( !info.devices_add.empty() )
	    info.devices_add += ' ';
	info.devices_add += i->device;
	++i;
	}
    y2mil( "devices_add:" << info.devices_add );
    info.devices_rem.clear();
    i=pv_remove.begin();
    while( i!=pv_remove.end() )
	{
	if( !info.devices_rem.empty() )
	    info.devices_rem += ' ';
	info.devices_rem += i->device;
	++i;
	}
    y2mil( "devices_rem:" << info.devices_rem );
    tinfo = info;
    }

std::ostream& operator<< (std::ostream& s, const EvmsCo& d )
    {
    s << *((PeContainer*)&d);
    if( d.lvm1 )
      s << " lvm1";
    s << " UUID:" << d.uuid;
    return( s );
    }

void EvmsCo::logDifference( const EvmsCo& d ) const
    {
    string log = PeContainer::logDifference( d );
    if( lvm1!=d.lvm1 )
	{
	if( d.lvm1 )
	    log += " -->lvm1";
	else
	    log += " lvm1-->";
	}
    if( uuid!=d.uuid )
	log += " UUID:" + uuid + "-->" + d.uuid;
    y2milestone( "%s", log.c_str() );
    ConstEvmsPair p=evmsPair();
    ConstEvmsIter i=p.begin();
    while( i!=p.end() )
	{
	ConstEvmsPair pc=d.evmsPair();
	ConstEvmsIter j = pc.begin();
	while( j!=pc.end() && 
	       (i->device()!=j->device() || i->created()!=j->created()) )
	    ++j;
	if( j!=pc.end() )
	    {
	    if( !i->equalContent( *j ) )
		i->logDifference( *j );
	    }
	else
	    y2mil( "  -->" << *i );
	++i;
	}
    p=d.evmsPair();
    i=p.begin();
    while( i!=p.end() )
	{
	ConstEvmsPair pc=evmsPair();
	ConstEvmsIter j = pc.begin();
	while( j!=pc.end() && 
	       (i->device()!=j->device() || i->created()!=j->created()) )
	    ++j;
	if( j==pc.end() )
	    y2mil( "  <--" << *i );
	++i;
	}
    }

bool EvmsCo::equalContent( const EvmsCo& rhs ) const
    {
    bool ret = PeContainer::equalContent(rhs,false) &&
	       uuid==rhs.uuid && lvm1==rhs.lvm1;
    if( ret )
	{
	ConstEvmsPair p = evmsPair();
	ConstEvmsPair pc = rhs.evmsPair();
	ConstEvmsIter i = p.begin();
	ConstEvmsIter j = pc.begin();
	while( ret && i!=p.end() && j!=pc.end() ) 
	    {
	    ret = ret && i->equalContent( *j );
	    ++i;
	    ++j;
	    }
	ret == ret && i==p.end() && j==pc.end();
	}
    return( ret );
    }

EvmsCo::EvmsCo( const EvmsCo& rhs ) : PeContainer(rhs)
    {
    y2milestone( "constructed EvmsCo by copy constructor from %s",
                 rhs.nm.c_str() );
    uuid = rhs.uuid;
    lvm1 = rhs.lvm1;
    ConstEvmsPair p = rhs.evmsPair();
    for( ConstEvmsIter i = p.begin(); i!=p.end(); ++i )
        {
        Evms * p = new Evms( *this, *i );
        vols.push_back( p );
        }
    }

void EvmsCo::logData( const string& Dir ) {;}

bool EvmsCo::active = false;
int EvmsCo::sockfd = -1;

