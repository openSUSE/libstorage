/*
  Textdomain    "storage"
*/

#include <sstream>

#include "y2storage/DmPart.h"
#include "y2storage/DmPartCo.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/ProcPart.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

using namespace storage;
using namespace std;

DmPart::DmPart( const DmPartCo& d, unsigned nr, Partition* pa ) :
	Dm( d, "" )
    {
    init( d.numToName(nr) );
    numeric = true;
    num = nr;
    getTableInfo();
    p = pa;
    if( pa )
	setSize( pa->sizeK() );
    y2milestone( "constructed DmPart %s on co %s", dev.c_str(),
		 cont->name().c_str() );
    }

DmPart::~DmPart()
    {
    y2debug( "destructed DmPart %s", dev.c_str() );
    }

void DmPart::init( const string& name )
    {
    p = NULL;
    nm = name;
    dev = "/dev/mapper/" + name;
    string::size_type pos =  name.find_last_of( "/" );
    if( pos!=string::npos )
	nm = name.substr( pos+1 );
    else
	nm = name;
    tname = nm;
    Dm::init();
    }

const DmPartCo* DmPart::co() const
    {
    return(dynamic_cast<const storage::DmPartCo*>(cont));
    }

void DmPart::updateName()
    {
    if( p && p->nr() != num )
	{
	num = p->nr();
	nm = co()->numToName(num);
	dev = "/dev/mapper/" + nm;
	}
    }

void DmPart::updateMinor()
    {
    unsigned long mj=mjr;
    unsigned long mi=mnr;
    getMajorMinor( dev, mj, mi );
    if( mi!=mnr || mj!=mjr )
	{
	mnr = mi;
	mjr = mj;
	alt_names.remove_if( find_begin("/dev/dm-" ) );
	alt_names.push_back( "/dev/dm-" + decString(mnr) );
	getTableInfo();
	}
    }

void DmPart::updateSize()
    {
    if( p )
	{
	orig_size_k = p->origSizeK();
	size_k = p->sizeK();
	}
    }

void DmPart::updateSize( ProcPart& pp )
    {
    unsigned long long si = 0;
    updateSize();
    if( mjr>0 && pp.getSize( "dm-"+decString(mnr), si ))
	setSize( si );
    }

void DmPart::getCommitActions( std::list<storage::commitAction*>& l ) const
    {
    unsigned s = l.size();
    Dm::getCommitActions(l);
    if( p )
	{
	if( s==l.size() && Partition::toChangeId( *p ) )
	    l.push_back( new commitAction( INCREASE, cont->staticType(),
					   setTypeText(false), this, false ));
	}
    }

string DmPart::setTypeText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by partition name (e.g. pdc_dabaheedj1),
        // %2$s is replaced by hexadecimal number (e.g. 8E)
        txt = sformat( _("Setting type of partition %1$s to %2$X"),
                      d.c_str(), id() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by partition name (e.g. pdc_dabaheedj1),
        // %2$s is replaced by hexadecimal number (e.g. 8E)
        txt = sformat( _("Set type of partition %1$s to %2$X"),
                      d.c_str(), id() );
        }
    return( txt );
    }

void DmPart::getInfo( DmPartInfo& tinfo ) const
    {
    ((Volume*)this)->getInfo( info.v );
    if( p )
	p->getInfo( info.p );
    info.part = p!=NULL;
    info.table = tname;
    info.target = target;
    tinfo = info;
    }

namespace storage
{

std::ostream& operator<< (std::ostream& s, const DmPart &p )
    {
    s << *(Dm*)&p;
    return( s );
    }

}

bool DmPart::equalContent( const DmPart& rhs ) const
    {
    return( Dm::equalContent(rhs) );
    }

void DmPart::logDifference( const DmPart& rhs ) const
    {
    string log = stringDifference( rhs );
    y2milestone( "%s", log.c_str() );
    }

DmPart& DmPart::operator= ( const DmPart& rhs )
    {
    y2debug( "operator= from %s", rhs.nm.c_str() );
    *((Dm*)this) = rhs;
    return( *this );
    }

DmPart::DmPart( const DmPartCo& d, const DmPart& rhs ) : Dm(d,rhs)
    {
    y2debug( "constructed dmraid by copy constructor from %s",
	     rhs.dev.c_str() );
    *this = rhs;
    }
