#include <iostream> 
#include <sstream> 

#include <ycp/y2log.h>

#include "y2storage/LvmVg.h"
#include "y2storage/LvmLv.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

LvmVg::LvmVg( Storage * const s, const string& Name ) :
    Container(s,Name,staticType())
    {
    y2milestone( "construcing lvm vg %s", Name.c_str() );
    init();
    if( Name.size()>0 )
	{
	getVgData( Name );
	}
    else
	{
	y2error( "empty name in constructor" );
	}
    }

LvmVg::LvmVg( Storage * const s, const string& Name, bool lv1 ) :
    Container(s,Name,staticType())
    {
    y2milestone( "construcing lvm vg %s lvm1:%d", Name.c_str(), lvm1 );
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
	while( peSizeK>1 && peSizeK%2==0 )
	    peSizeK /= 2;
	if( peSizeK!=1 )
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
static bool lvNotDeleted( const LvmLv& l ) { return( !l.deleted() ); }

int
LvmVg::removeVg()
    {
    int ret = 0;
    y2milestone( "begin" );
    if( !created() )
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
    std::ostringstream buf;
    buf << devs;
    y2milestone( "devices:%s", buf.str().c_str() );

    list<string>::const_iterator i=devs.begin();
    list<Pv>::iterator p;
    while( ret==0 && i!=devs.end() )
	{
	if( (p=find( pv.begin(), pv.end(), *i ))!=pv.end() || 
	    (p=find( pv_add.begin(), pv_add.end(), *i ))!=pv_add.end())
	    ret = LVM_PV_ALREADY_CONTAINED;
	else if( (p=find( pv_remove.begin(), pv_remove.end(), *i )) != 
	         pv_remove.end() )
	    {
	    }
	else if( !getStorage()->knownDevice( *i, true ) )
	    {
	    ret = LVM_PV_DEVICE_UNKNOWN;
	    }
	else if( !getStorage()->canUseDevice( *i, true ) )
	    {
	    ret = LVM_PV_DEVICE_USED;
	    }
	++i;
	}
    i=devs.begin();
    while( ret==0 && i!=devs.end() )
	{
	unsigned long pe = 0;
	if( (p=find( pv_remove.begin(), pv_remove.end(), *i )) != 
	     pv_remove.end() )
	    {
	    pv.push_back( *p );
	    pe = p->num_pe;
	    pv_remove.erase( p );
	    }
	else
	    {
	    Pv pvn;
	    unsigned long long s = getStorage()->deviceSize( *i );
	    pe = (s - 500)/pe_size;
	    pvn.num_pe = pvn.free_pe = pe;
	    pvn.device = *i;
	    pv_add.push_back( pvn );
	    getStorage()->setUsedBy( *i, UB_LVM, name() );
	    }
	free_pe += pe;
	num_pe += pe;
	++i;
	}
    if( ret==0 && pv_add.size()+pv.size()-pv_remove.size()<=0 )
	ret = LVM_VG_HAS_NONE_PV;
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
    std::ostringstream buf;
    buf << devs;
    y2milestone( "devices:%s", buf.str().c_str() );

    list<string>::const_iterator i=devs.begin();
    list<Pv>::iterator p;
    unsigned long long rem = 0;
    unsigned long to_remove_lv = leByLvRemove();
    while( ret==0 && i!=devs.end() )
	{
	if( (p=find( pv.begin(), pv.end(), *i ))!=pv.end() )
	    {
	    if( p->free_pe-p->num_pe > to_remove_lv )
		ret = LVM_REMOVE_PV_IN_USE;
	    else
		to_remove_lv += p->free_pe-p->num_pe;
	    rem += p->num_pe;
	    }
	else if( (p=find( pv_add.begin(), pv_add.end(), *i ))!=pv_add.end())
	    rem += p->num_pe;
	else
	    ret = LVM_PV_NOT_FOUND;
	++i;
	}
    if( ret==0 && rem>free_pe )
	ret = LVM_REMOVE_PV_SIZE_NEEDED;
    if( ret==0 && pv_add.size()+pv.size()-pv_remove.size()<=0 )
	ret = LVM_VG_HAS_NONE_PV;
    i=devs.begin();
    while( ret==0 && i!=devs.end() )
	{
	unsigned long pe = 0;
	if( (p=find( pv_add.begin(), pv_add.end(), *i ))==pv_add.end())
	    {
	    pe = p->num_pe;
	    getStorage()->setUsedBy( *i, UB_NONE, "" );
	    pv_add.erase( p );
	    }
	else
	    {
	    p = find( pv.begin(), pv.end(), *i );
	    pe = p->num_pe;
	    getStorage()->setUsedBy( *i, UB_NONE, "" );
	    pv_remove.push_back( *p );
	    pv.erase( p );
	    }
	free_pe -= pe;
	num_pe -= pe;
	++i;
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
    if( name.find( "\"\' /\n\t:*?" )!=string::npos )
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
    if( ret==0 && free_pe<num_le )
	{
	ret = LVM_LV_NO_SPACE;
	}
    map<string,unsigned long> pe_map;
    map<string,unsigned long>::iterator mit;
    if( ret==0 && stripe>1 )
	{
	// this is only a very rough estimate if the creation of the striped 
	// lv may be possible, no sense in emulating LVM allocation strategy
	// here
	unsigned long per_stripe = (num_le+stripe-1)/stripe;
	list<Pv> tmp = pv;
	tmp.insert( tmp.end(), pv_add.begin(), pv_add.end() );
	tmp.sort( Pv::comp_le );
	tmp.remove_if( Pv::no_free );
	list<Pv>::iterator i;
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
	    i = pv.begin();
	    while( i!=pv.end() )
		{
		if( (p=find( tmp.begin(), tmp.end(), i->device))!=tmp.end())
		    {
		    i->free_pe = p->free_pe;
		    }
		else
		    i->free_pe = 0;
		++i;
		}
	    i = pv_add.begin();
	    while( i!=pv_add.end() )
		{
		if( (p=find( tmp.begin(), tmp.end(), i->device))!=tmp.end())
		    {
		    i->free_pe = p->free_pe;
		    }
		else
		    i->free_pe = 0;
		++i;
		}
	    }
	}
    if( ret==0 && stripe==1 )
	{
	unsigned long rest = num_le;
	list<Pv>::iterator i;
	while( rest>0 && i!=pv.end() )
	    {
	    i->free_pe -= min(rest,i->free_pe);
	    rest -= min(rest,i->free_pe);
	    pe_map[i->device] = min(rest,i->free_pe);
	    ++i;
	    }
	i = pv_add.begin();
	while( rest>0 && i!=pv_add.end() )
	    {
	    i->free_pe -= min(rest,i->free_pe);
	    rest -= min(rest,i->free_pe);
	    pe_map[i->device] = min(rest,i->free_pe);
	    ++i;
	    }
	}
    if( ret==0 )
	{
	LvmLv* l = new LvmLv( *this, name, num_le, stripe );
	l->setPeMap( pe_map );
	}
    y2milestone( "ret:%d device:%s", ret, ret?device.c_str():"" );
    return( ret );
    }

void LvmVg::getVgData( const string& name )
    {
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
		line.erase( 0, line.find_first_not_of( " \t\n" ));
		if( line.find( "Format" ) == 0 )
		    {
		    lvm1 = extractNthWord( 1, line )=="lvm1";
		    }
		if( line.find( "PE Size" ) == 0 )
		    {
		    tmp = extractNthWord( 2, line );
		    pos = tmp.find( '.' );
		    if( pos!=string::npos )
			tmp.erase( pos );
		    tmp >> pe_size;
		    }
		if( line.find( "Total PE" ) == 0 )
		    {
		    extractNthWord( 2, line ) >> num_pe;
		    }
		if( line.find( "Free  PE" ) == 0 )
		    {
		    extractNthWord( 4, line ) >> free_pe;
		    }
		if( line.find( "VG Status" ) == 0 )
		    {
		    status = extractNthWord( 2, line );
		    }
		if( line.find( "VG Access" ) == 0 )
		    {
		    ronly = extractNthWord( 2, line, true ).find( "write" ) == string::npos;
		    }
		if( line.find( "VG UUID" ) == 0 )
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
		line.erase( 0, line.find_first_not_of( " \t\n" ));
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
		if( line.find( "LV Write Access" ) == 0 )
		    {
		    readOnly = extractNthWord( 3, line, true ).find( "only" ) != string::npos;
		    }
		if( line.find( "LV Status" ) == 0 )
		    {
		    status = extractNthWord( 2, line, true );
		    }
		if( line.find( "Current LE" ) == 0 )
		    {
		    extractNthWord( 2, line ) >> num_le;
		    }
		if( line.find( "Allocation" ) == 0 )
		    {
		    allocation = extractNthWord( 1, line );
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
		line.erase( 0, line.find_first_not_of( " \t\n" ));
		if( line.find( "PV Name" ) == 0 )
		    {
		    if( p->device.size()>0 )
			{
			addPv( p );
			}
		    p->device = extractNthWord( 2, line );
		    }
		if( line.find( "PV UUID" ) == 0 )
		    {
		    p->uuid = extractNthWord( 2, line );
		    }
		if( line.find( "PV Status" ) == 0 )
		    {
		    p->status = extractNthWord( 2, line );
		    }
		if( line.find( "Total PE" ) == 0 )
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
    }

void LvmVg::addLv( unsigned long& le, string& name, string& uuid,
                   string& status, string& alloc, bool& ro )
    {
    LvmLvPair p=lvmLvPair();
    LvmLvIter i=p.begin();
    while( i!=p.end() && i->name()!=name )
	{
	cout << "Lv Name:" << i->name() << " Name:" << name << endl;
	++i;
	}
    if( i!=p.end() )
	{
	if( i->created() )
	    {
	    i->setCreated( false );
	    i->setLe( le );
	    i->setUuid( uuid );
	    i->setStatus( status );
	    i->setAlloc( alloc );
	    i->getTableInfo();
	    }
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

int LvmVg::commitChanges( CommitStage stage )
    {
    y2milestone( "name %s stage %d", name().c_str(), stage );
    int ret = 0;
    switch( stage )
	{
	case DECREASE:
	    if( ret==0 )
		ret = Container::commitChanges( stage );
	    if( deleted() )
		{
		ret = doRemoveVg();
		}
	    break;
	case INCREASE:
	    if( created() )
		{
		ret = doCreateVg();
		}
	    if( ret==0 )
		{
		ret = Container::commitChanges( stage );
		}
	    break;
	default:
	    ret = Container::commitChanges( stage );
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
				       removeVgText(false), true ));
	}
    else if( created() )
	{
	l.push_front( new commitAction( INCREASE, staticType(), 
				        createVgText(false), true ));
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

string LvmVg::metaString()
    {
    return( (lvm1)?"M1":"M2" );
    }

int LvmVg::doCreatePv( const string& device )
    {
    int ret = 0;
    y2milestone( "dev:%s", device.c_str() );
    SystemCmd c;
    string cmd = "mdadm --zero-superblock " + device;
    c.execute( cmd );
    cmd = "pvcreate -ff " + metaString() + " " + device;
    c.execute( cmd );
    if( c.retcode()!=0 )
	ret = LVM_CREATE_PV_FAILED;
    y2milestone( "ret:%d", ret );
    return( ret );
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
	pos=vgname.find_first_not_of( " \t\n\"" );
	if( pos>0 )
	    vgname.erase( 0, pos );
	pos=vgname.find_first_of( " \t\n\"" );
	if( pos>0 )
	    vgname.erase( pos );
	l.push_back(vgname);
	}
    std::ostringstream buf;
    buf << l;
    y2milestone( "detected Vgs %s", buf.str().c_str() );
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

int LvmVg::doCreate( Volume* v ) { return( 0 ); }
int LvmVg::doRemove( Volume* v ) { return( 0 ); }
int LvmVg::doResize( Volume* v ) { return( 0 ); }

int
LvmVg::checkResize( Volume* v, unsigned long long newSizeK ) const
    {
    return( false );
    }

void LvmVg::logData( const string& Dir ) {;}

bool LvmVg::lvm_active = false;

