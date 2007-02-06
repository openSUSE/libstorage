/*
  Textdomain    "storage"
*/

#include <iostream> 
#include <sstream> 

#include "y2storage/LvmVg.h"
#include "y2storage/LvmLv.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

using namespace std;
using namespace storage;

static bool lvNotCreated( const LvmLv& l ) { return( !l.created() ); }
static bool lvNotDeletedCreated( const LvmLv& l ) { return( !l.created()&&!l.deleted() ); }

LvmVg::LvmVg( Storage * const s, const string& Name ) :
    PeContainer(s,staticType())
    {
    nm = Name;
    y2debug( "constructing lvm vg %s", Name.c_str() );
    init();
    if( !Name.empty() )
	{
	getVgData( Name, false );
	LvmLvPair p=lvmLvPair(lvNotCreated);
	if( !p.empty() )
	    getStorage()->waitForDevice( p.begin()->device() );
	}
    else
	{
	y2error( "empty name in constructor" );
	}
    }

LvmVg::LvmVg( Storage * const s, const string& Name, bool lv1 ) :
    PeContainer(s,staticType())
    {
    nm = Name;
    y2debug( "constructing lvm vg %s lvm1:%d", Name.c_str(), lv1 );
    init();
    lvm1 = lv1;
    if( Name.empty() )
	{
	y2error( "empty name in constructor" );
	}
    }

LvmVg::LvmVg( Storage * const s, const string& file, int ) :
    PeContainer(s,staticType())
    {
    y2debug( "constructing lvm vg %s from file %s", dev.c_str(), file.c_str() );
    }

LvmVg::~LvmVg()
    {
    y2debug( "destructed lvm vg %s", dev.c_str() );
    }

static bool lvDeleted( const LvmLv& l ) { return( l.deleted() ); }
static bool lvCreated( const LvmLv& l ) { return( l.created() ); }
static bool lvResized( const LvmLv& l ) { return( l.extendSize()!=0 ); }

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
    if( ret==0 )
	{
	unuseDev();
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
    y2mil( "this:" << *this );

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
	    pv_add.push_back( pvn );
	    if( !getStorage()->isDisk(d))
		getStorage()->changeFormatVolume( d, false, FSNONE );
	    }
	getStorage()->setUsedBy( d, UB_LVM, name() );
	free_pe += pe;
	num_pe += pe;
	++i;
	}
    if( ret==0 && pv_add.size()+pv.size()-pv_remove.size()<=0 )
	ret = LVM_VG_HAS_NONE_PV;
    if( ret==0 )
	checkConsistency();
    y2mil( "this:" << *this );
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
    y2mil( "this:" << *this );

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
    y2mil( "add:" << pv_add.size() << " pv:" << pv.size() << " remove:" << devs.size() );
    if( ret==0 && pv_add.size()+pv.size()-devs.size()<=0 )
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
    y2mil( "this:" << *this );
    y2milestone( "ret:%d", ret );
    return( ret );
    }


int 
LvmVg::setPeSize( long long unsigned peSizeK )
    {
    int ret = 0;
    y2milestone( "old:%llu new:%llu", pe_size, peSizeK );
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
    y2milestone( "ret:%d", ret );
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
	LvmLv* l = new LvmLv( *this, name, num_le, stripe );
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
	    i->setDeleted( true );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int 
LvmVg::changeStripe( const string& name, unsigned long stripe )
    {
    int ret = 0;
    y2milestone( "name:%s stripe:%lu", name.c_str(), stripe );
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int 
LvmVg::changeStripeSize( const string& name, unsigned long long stripeSize )
    {
    int ret = 0;
    y2milestone( "name:%s stripeSize:%llu", name.c_str(), stripeSize );
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void LvmVg::getVgData( const string& name, bool exists )
    {
    y2milestone( "name:%s", name.c_str() );
    SystemCmd c( "/sbin/vgdisplay --units k -v " + name );
    unsigned cnt = c.numLines();
    unsigned i = 0;
    num_lv = 0;
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
		    if( !vname.empty() )
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
	    if( !vname.empty() )
		{
		addLv( num_le, vname, uuid, status, allocation, readOnly );
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
	num_lv++;
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
    if( num_lv>0 && lvmLvPair().empty() )
	{
	y2milestone( "inactive VG %s num_lv:%u", nm.c_str(), num_lv );
	inactiv = true;
	}
    }

void LvmVg::addLv( unsigned long& le, string& name, string& uuid,
                   string& status, string& alloc, bool& ro )
    {
    y2milestone( "addLv:%s", name.c_str() );
    LvmLvPair p=lvmLvPair(lvNotDeletedCreated);
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
	y2milestone( "addLv exists deleted %d", i!=p.end() );
	if( i==p.end() )
	    {
	    num_lv++;
	    LvmLv *n = new LvmLv( *this, name, le, uuid, status, alloc );
	    if( ro )
		n->setReadonly();
	    if( !n->inactive() )
		addToList( n );
	    else
		{
		y2milestone( "inactive Lv %s", name.c_str() );
		delete n;
		}
	    }
	}
    name = uuid = status = alloc = "";
    le = 0; 
    ro = false;
    }

void LvmVg::addPv( Pv*& p )
    {
    PeContainer::addPv( p );
    if( !deleted() &&
        find( pv_remove.begin(), pv_remove.end(), *p )==pv_remove.end() )
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
	y2milestone( "ret:%d col:%zd vol:%zd", ret, col.size(), vol.size());
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void LvmVg::getCommitActions( list<commitAction*>& l ) const
    {
    Container::getCommitActions( l );
    y2mil( "Container::getCommitActions:" << l );
    if( deleted() )
	{
	l.push_back( new commitAction( DECREASE, staticType(), 
				       removeVgText(false), this, true ));
	}
    else if( created() )
	{
	l.push_front( new commitAction( INCREASE, staticType(), 
				        createVgText(false), this, true ));
	}
    else 
	{
	if( !pv_add.empty() )
	    for( list<Pv>::const_iterator i=pv_add.begin(); i!=pv_add.end(); 
	         ++i )
		l.push_back( new commitAction( INCREASE, staticType(),
					       extendVgText(false,i->device), 
					       this, true ));
	if( !pv_remove.empty() )
	    for( list<Pv>::const_iterator i=pv_remove.begin(); 
	         i!=pv_remove.end(); ++i )
		l.push_back( new commitAction( DECREASE, staticType(),
					       reduceVgText(false,i->device), 
					       this, false ));
	}
    }

string 
LvmVg::removeVgText( bool doing ) const
    {
    string txt;
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

string 
LvmVg::createVgText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. system),
	// %2$ is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
        txt = sformat( _("Creating volume group %1$s from %2$s"), name().c_str(),
	               addList().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. system),
	// %2$ is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
        txt = sformat( _("Create volume group %1$s from %2$s"), name().c_str(),
	               addList().c_str() );
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

string 
LvmVg::reduceVgText( bool doing, const string& dev ) const
    {
    string txt;
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
LvmVg::init()
    {
    PeContainer::init();
    dev = nm;
    normalizeDevice(dev);
    num_lv = 0;
    inactiv = lvm1 = false;
    }

void LvmVg::activate( bool val )
    {
    y2milestone( "old active:%d val:%d", active, val );
    if( active!=val )
	{
	SystemCmd c;
	if( val )
	    {
	    Dm::activate(true);
	    c.execute( "vgscan --mknodes" );
	    c.execute( "vgchange -a y" );
	    }
	else
	    {
	    c.execute( "vgchange -a n" );
	    }
	active = val;
	}
    }

void LvmVg::getVgs( list<string>& l )
    {
    l.clear();
    string vgname;
    string::size_type pos;
    SystemCmd c( "vgdisplay -s" );
    if( !active && c.numLines()>0 )
	active = true;
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
	if( pv_add.size()+pv.size()-pv_remove.size()<=0 )
	    ret = LVM_VG_HAS_NONE_PV;
	list<Pv>::iterator p = pv_add.begin();
	while( ret==0 && p!=pv_add.end() )
	    {
	    if( !devices.empty() )
		devices += " ";
	    devices += p->device;
	    ret = doCreatePv( p->device );
	    ++p;
	    }
	if( ret==0 )
	    {
	    string ddir = "/dev/" + name();
	    if( access( ddir.c_str(), R_OK )==0 )
		{
		SystemCmd c( "find " + ddir + " -type l | xargs -r rm" );
		rmdir( ddir.c_str() );
		}
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
	if( !active )
	    activate(true);
	if( !silent )
	    {
	    getStorage()->showInfoCb( removeVgText(true) );
	    }
	checkConsistency();
	string cmd = "vgremove " + name();
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
LvmVg::doExtendVg()
    {
    y2milestone( "Vg:%s", name().c_str() );
    y2mil( "this:" << *this );
    int ret = 0;
    list<string> devs;
    if( !active )
	activate(true);
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
	p = find( pv_add.begin(), pv_add.end(), *d );
	if( p!=pv_add.end() )
	    {
	    pv_add.erase( p );
	    if( ret==0 )
		ret = LVM_PV_STILL_ADDED;
	    }
	++d;
	}
    if( devs.size()>0 )
	checkCreateConstraints();
    y2mil( "this:" << *this );
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int
LvmVg::doReduceVg()
    {
    y2milestone( "Vg:%s", name().c_str() );
    y2mil( "this:" << *this );
    int ret = 0;
    if( !active )
	activate(true);
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
	    {
	    ret = LVM_VG_REDUCE_FAILED;
	    setExtError( c );
	    }
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
    y2mil( "this:" << *this );
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
	if( !active )
	    activate(true);
	if( !silent )
	    {
	    getStorage()->showInfoCb( l->createText(true) );
	    }
	checkConsistency();
	string cmd = "lvcreate " + instSysString() + " -l " + decString(l->getLe());
	if( l->stripes()>1 )
	    {
	    cmd += " -i " + decString(l->stripes());
	    if( l->stripeSize()>0 )
		cmd += " -I " + decString(l->stripeSize());
	    }
	cmd += " -n " + l->name();
	cmd += " " + name();
	SystemCmd c( cmd );
	if( c.retcode()!=0 )
	    {
	    ret = LVM_LV_CREATE_FAILED;
	    setExtError( c );
	    }
	if( ret==0 )
	    {
	    getStorage()->waitForDevice( l->device() );
	    l->setCreated(false);
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
	    string cmd = "lvremove -f " + instSysString() + " " + l->device();
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
	    string cmd = "lvreduce -f " + instSysString() + 
	                 " -l -" + decString(old_le-new_le) + " " + l->device();
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		{
		ret = LVM_LV_RESIZE_FAILED;
		setExtError( c );
		}
	    }
	if( ret==0 && old_le<new_le )
	    {
	    string cmd = "lvextend " + instSysString() + 
	                 " -l +" + decString(new_le-old_le) + " " + l->device();
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
    getStorage()->removeDmTableTo( device );
    if( getStorage()->isDisk(device) )
	{
	cmd = "parted " + device + " mklabel msdos";
	c.execute( cmd );
	}
    cmd = "echo y | pvcreate -ff " + metaString() + device;
    c.execute( cmd );
    if( c.retcode()!=0 )
	{
	ret = LVM_CREATE_PV_FAILED;
	setExtError( c );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void LvmVg::normalizeDmDevices()
    {
    string dm = decString(Dm::dmMajor());
    SystemCmd c;
    for( list<Pv>::iterator i=pv.begin(); i!=pv.end(); ++i )
	{
	if( i->device.find( "/dev/dm-" )==0 )
	    {
	    c.execute( "devmap_name "+dm+":"+i->device.substr( 8 ) );
	    if( c.retcode()==0 )
		{
		string dev = "/dev/" + *c.getLine( 0 );
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
    info.peSize = peSize();
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
    y2mil( "device:" << info.devices << " devices_add:" << info.devices_add <<
           " devices_rem:" << info.devices_rem );
    tinfo = info;
    }

namespace storage
{

std::ostream& operator<< (std::ostream& s, const LvmVg& d )
    {
    s << *((PeContainer*)&d);
    s << " status:" << d.status;
    if( d.lvm1 )
      s << " lvm1";
    if( d.inactiv )
      s << " inactive";
    s << " UUID:" << d.uuid 
      << " lv:" << d.num_lv;
    return( s );
    }

}

void LvmVg::logDifference( const LvmVg& d ) const
    {
    string log = PeContainer::logDiff( d );
    if( status!=d.status )
	log += " status:" + status + "-->" + d.status;
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
    ConstLvmLvPair p=lvmLvPair();
    ConstLvmLvIter i=p.begin();
    while( i!=p.end() )
	{
	ConstLvmLvPair pc=d.lvmLvPair();
	ConstLvmLvIter j = pc.begin();
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
    p=d.lvmLvPair();
    i=p.begin();
    while( i!=p.end() )
	{
	ConstLvmLvPair pc=lvmLvPair();
	ConstLvmLvIter j = pc.begin();
	while( j!=pc.end() && 
	       (i->device()!=j->device() || i->created()!=j->created()) )
	    ++j;
	if( j==pc.end() )
	    y2mil( "  <--" << *i );
	++i;
	}
    }

bool LvmVg::equalContent( const LvmVg& rhs ) const
    {
    bool ret = PeContainer::equalContent(rhs,false) &&
	       status==rhs.status && uuid==rhs.uuid && lvm1==rhs.lvm1 &&
	       inactiv==rhs.inactiv && num_lv==rhs.num_lv;
    if( ret )
	{
	ConstLvmLvPair p = lvmLvPair();
	ConstLvmLvPair pc = rhs.lvmLvPair();
	ConstLvmLvIter i = p.begin();
	ConstLvmLvIter j = pc.begin();
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

LvmVg::LvmVg( const LvmVg& rhs ) : PeContainer(rhs)
    {
    y2debug( "constructed LvmVg by copy constructor from %s", rhs.nm.c_str() );
    status = rhs.status;
    uuid = rhs.uuid;
    lvm1 = rhs.lvm1;
    inactiv = rhs.inactiv;
    num_lv = rhs.num_lv;
    ConstLvmLvPair p = rhs.lvmLvPair();
    for( ConstLvmLvIter i = p.begin(); i!=p.end(); ++i )
	{
	LvmLv * p = new LvmLv( *this, *i );
	vols.push_back( p );
	}
    }

void LvmVg::logData( const string& Dir ) {;}

bool LvmVg::active = false;

