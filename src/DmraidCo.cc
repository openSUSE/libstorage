/*
  Textdomain    "storage"
*/

#include <iostream> 
#include <sstream> 

#include "y2storage/DmraidCo.h"
#include "y2storage/Dmraid.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

using namespace std;
using namespace storage;

DmraidCo::DmraidCo( Storage * const s, const string& Name, ProcPart& ppart ) :
    DmPartCo(s, "/dev/mapper/"+Name, staticType(), ppart )
    {
    DmPartCo::init( ppart );
    getRaidData( Name );
    y2debug( "constructing dmraid co %s", Name.c_str() );
    }

DmraidCo::~DmraidCo()
    {
    y2debug( "destructed raid co %s", dev.c_str() );
    }

void DmraidCo::getRaidData( const string& name )
    {
    y2milestone( "name:%s", name.c_str() );
    SystemCmd c( "dmraid -s -c -c -c \"" + name + "\"" );
    list<string>::const_iterator ci;
    list<string> sl;
    if( c.numLines()>0 )
	sl = splitString( *c.getLine(0), ":" );
    Pv *pve = new Pv;
    if( sl.size()>=4 )
	{
	ci = sl.begin();
	++ci; ++ci; ++ci;
	raidtype = *ci;
	}
    unsigned num = 1;
    while( num<c.numLines() )
	{
	sl = splitString( *c.getLine(num), ":" );
	y2mil( "sl:" << sl );
	if( sl.size()>=3 )
	    {
	    ci = sl.begin();
	    ++ci; ++ci;
	    if( *ci == name )
		{
		--ci;
		if( controller.empty() && !ci->empty() )
		    controller = *ci;
		--ci;
		if( ci->find( "/dev/" )==0 )
		    {
		    pve->device = *ci;
		    addPv( pve );
		    }
		}
	    }
	++num;
	}
    delete( pve );
    }

void
DmraidCo::newP( DmPart*& dm, unsigned num, Partition* p ) 
    {
    y2mil( "num:" << num );
    dm = new Dmraid( *this, num, p );
    }

void DmraidCo::addPv( Pv*& p )
    {
    PeContainer::addPv( p );
    if( !deleted() )
	getStorage()->setUsedBy( p->device, UB_DMRAID, name() );
    p = new Pv;
    }

void DmraidCo::activate( bool val )
    {
    y2milestone( "old active:%d val:%d", active, val );
    if( active != val )
	{
	SystemCmd c;
	if( val )
	    {
	    Dm::activate(true);
	    c.execute( "dmraid -ay -p " );
	    }
	else
	    {
	    c.execute( "dmraid -an " );
	    }
	active = val;
	}
    }

void DmraidCo::getRaids( list<string>& l )
    {
    l.clear();
    SystemCmd c( "dmraid -s -c -c -c" );
    for( unsigned i=0; i<c.numLines(); ++i )
	{
	list<string> sl = splitString( *c.getLine(i), ":" );
	if( sl.size()>=3 )
	    {
	    list<string>::const_iterator ci = sl.begin();
	    if( !ci->empty()
		&& ci->find( "/dev/" )==string::npos
		&& find( l.begin(), l.end(), *ci )==l.end())
		{
		l.push_back( *ci );
		}
	    }
	}
    y2mil( "detected Raids " << l );
    }

string DmraidCo::removeText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by a name (e.g. pdc_igeeeadj),
        txt = sformat( _("Removing raid %1$s"), name().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. pdc_igeeeadj),
        txt = sformat( _("Remove raid %1$s"), name().c_str() );
        }
    return( txt );
    }


string DmraidCo::setDiskLabelText( bool doing ) const
    {
    string txt;
    string d = nm;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by raid name (e.g. pdc_igeeeadj),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Setting disk label of raid %1$s to %2$s"),
		       d.c_str(), labelName().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by raid name (e.g. pdc_igeeeadj),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Set disk label of raid %1$s to %2$s"),
		      d.c_str(), labelName().c_str() );
        }
    return( txt );
    }

int
DmraidCo::doRemove()
    {
    y2milestone( "Raid:%s", name().c_str() );
    int ret = 0;
    if( deleted() )
	{
	if( active )
	    {
	    activate_part(false);
	    activate(false);
	    }
	if( !silent )
	    {
	    getStorage()->showInfoCb( removeText(true) );
	    }
	string cmd = "cd /var/log/YaST2 && echo y | dmraid -E -r";
	SystemCmd c;
	for( list<Pv>::const_iterator i=pv.begin(); i!=pv.end(); ++i )
	    {
	    c.execute( cmd + " " + i->device );
	    }
	if( c.retcode()!=0 )
	    {
	    ret = DMRAID_REMOVE_FAILED;
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

void DmraidCo::getInfo( DmraidCoInfo& tinfo ) const
    {
    DmPartCo::getInfo( info );
    tinfo.p = info;
    }

namespace storage
{

std::ostream& operator<< (std::ostream& s, const DmraidCo& d )
    {
    s << *((DmPartCo*)&d);
    s << " Cont:" << d.controller
      << " RType:" << d.raidtype;
    return( s );
    }

}

string DmraidCo::logDiff( const DmraidCo& d ) const
    {
    string log = DmPartCo::logDiff( d );
    if( controller!=d.controller )
	log += " controller:" + controller + "-->" + d.controller;
    if( raidtype!=d.raidtype )
	log += " raidtype:" + raidtype + "-->" + d.raidtype;
    return( log );
    }

bool DmraidCo::equalContent( const DmraidCo& rhs ) const
    {
    bool ret = DmPartCo::equalContent( rhs ) &&
               controller==rhs.controller && raidtype==rhs.raidtype;
    return( ret );
    }

DmraidCo::DmraidCo( const DmraidCo& rhs ) : DmPartCo(rhs)
    {
    raidtype = rhs.raidtype;
    controller = rhs.controller;
    }

void DmraidCo::logData( const string& Dir ) {;}

bool DmraidCo::active = false;


