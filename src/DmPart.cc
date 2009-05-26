/*
  Textdomain    "storage"
*/

#include <sstream>

#include "storage/DmPart.h"
#include "storage/DmPartCo.h"
#include "storage/SystemCmd.h"
#include "storage/ProcPart.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"


namespace storage
{
    using namespace std;


DmPart::DmPart(const DmPartCo& d, unsigned nr, Partition* pa)
    : Dm(d, "")
{
    init( d.numToName(nr) );
    numeric = true;
    num = nr;
    getTableInfo();
    p = pa;
    if( pa )
	setSize( pa->sizeK() );
    y2mil("constructed DmPart " << dev << " on co " << cont->name());
}


DmPart::~DmPart()
{
    y2deb("destructed DmPart " << dev);
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
	replaceAltName( "/dev/dm-", "/dev/dm-"+decString(mnr) );
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

void DmPart::addUdevData()
    {
    addAltUdevId( num );
    }

static string udevCompleteIdPath( const string& s, unsigned nr )
    {
    return( "/dev/disk/by-id/" + s + "-part" + decString(nr) );
    }


void
DmPart::addAltUdevId( unsigned num )
{
    alt_names.remove_if(string_contains("/by-id/"));

    list<string>::const_iterator j = co()->udevId().begin();
    while( j!=co()->udevId().end() )
	{
	alt_names.push_back( udevCompleteIdPath( *j, num ));
	++j;
	}
    mount_by = orig_mount_by = defaultMountBy();
}


const std::list<string>
DmPart::udevId() const
{
    list<string> ret;
    for (list<string>::const_iterator i = alt_names.begin(); 
	 i != alt_names.end(); i++)
    {
	if (i->find("/by-id/") != string::npos)
	    ret.push_back(*i);
    }
    return ret;
}


void
DmPart::getCommitActions(list<commitAction>& l) const
    {
    unsigned s = l.size();
    Dm::getCommitActions(l);
    if( p )
	{
	if( s==l.size() && Partition::toChangeId( *p ) )
	    l.push_back(commitAction(INCREASE, cont->staticType(),
				     setTypeText(false), this, false));
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


std::ostream& operator<< (std::ostream& s, const DmPart &p )
    {
    s << *(Dm*)&p;
    return( s );
    }


bool DmPart::equalContent( const DmPart& rhs ) const
    {
    return( Dm::equalContent(rhs) );
    }

void DmPart::logDifference( const DmPart& rhs ) const
{
    string log = stringDifference(rhs);
    y2mil(log);
}


DmPart& DmPart::operator=(const DmPart& rhs)
{
    y2deb("operator= from " << rhs.nm);
    *((Dm*)this) = rhs;
    return *this;
}


DmPart::DmPart(const DmPartCo& d, const DmPart& rhs)
    : Dm(d, rhs)
{
    y2deb("constructed dmraid by copy constructor from " << rhs.dev);
    *this = rhs;
}

}
