/*
  Textdomain    "storage"
*/

#include <sstream>

#include "y2storage/Evms.h"
#include "y2storage/EvmsCo.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

using namespace storage;
using namespace std;

Evms::Evms( const EvmsCo& d, const string& name, unsigned long long le, bool native ) :
	Dm( d, getMapperName(d,name) )
    {
    init( name );
    y2mil( "init:" << *this );
    setLe( le );
    calcSize();
    if( majorNr()==Dm::dmMajor())
	getTableInfo();
    compat = !native;
    y2debug( "constructed evms vol %s on vg %s", dev.c_str(),
	     cont->name().c_str() );
    }

Evms::Evms( const EvmsCo& d, const string& name, unsigned long long le,
	    unsigned str ) :
	Dm( d, getMapperName(d,name) )
    {
    init( name );
    setLe( le );
    calcSize();
    stripe = str;
    fs = detected_fs = FSNONE;
    y2debug( "constructed evms vol %s on vg %s", dev.c_str(),
	     cont->name().c_str() );
    }

Evms::~Evms()
    {
    y2debug( "destructed evms vol %s", dev.c_str() );
    }

string Evms::getMapperName( const EvmsCo& d, const string& name )
    {
    string ret( name );
    if( !d.name().empty() )
	{
	ret = d.name() + "/" + name;
	}
    string::size_type pos = ret.find( '/' );
    while( pos!=string::npos )
	{
	ret[pos] = '|';
	pos = ret.find( '/', pos+1 );
	}
    y2mil( "name:" << name << " disk:" << d.name() << " ret:" << ret );
    return( ret );
    }

void Evms::init( const string& name )
    {
    compat = true;
    nm = name;
    dev = "/dev/evms/";
    if( !cont->name().empty() )
	dev += cont->name() + "/";
    dev += name;
    Dm::init();
    if( majorNr()!=Dm::dmMajor())
	{
	list<string>::iterator it;
	y2mil( "alt_names:" << alt_names );
	while( (it=find_if( alt_names.begin(), alt_names.end(),
	                    find_begin("/dev/dm-")))!=alt_names.end() )
	    alt_names.erase(it);
	y2mil( "alt_names:" << alt_names );
	while( (it=find_if( alt_names.begin(), alt_names.end(),
	                    find_begin("/dev/mapper/")))!=alt_names.end() )
	    alt_names.erase(it);
	y2mil( "alt_names:" << alt_names );
	}
    }

void Evms::updateMd()
    {
    if( majorNr()==Md::mdMajor() )
	{
	Volume const *v;
	string dev = "/dev/md" + decString( minorNr() );
	if( cont->getStorage()->findVolume( dev, v ))
	    {
	    y2mil( "bef update " << *this );
	    setUuid( v->getUuid() );
	    initLabel( v->getLabel() );
	    alt_names = v->altNames();
	    fs = detected_fs = v->getFs();
	    mp = orig_mp = v->getMount();
	    is_mounted = v->isMounted();
	    y2mil( "aft update " << *this );
	    }
	}
    }

string Evms::removeText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	txt = sformat( _("Deleting EVMS volume %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete EVMS volume %1$s (%2$s)"), dev.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

string Evms::createText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	txt = sformat( _("Creating EVMS volume %1$s"), dev.c_str() );
	}
    else
	{
	if( mp=="swap" )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create swap EVMS volume %1$s (%2$s)"),
	                   dev.c_str(), sizeString().c_str() );
	    }
	else if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create EVMS volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create encrypted EVMS volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create EVMS volume %1$s (%2$s)"),
			   dev.c_str(), sizeString().c_str() );
	    }
	}
    return( txt );
    }

string Evms::formatText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting EVMS volume %1$s (%2$s) with %3$s "),
		       dev.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( !mp.empty() )
	    {
	    if( mp=="swap" )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Format EVMS volume %1$s (%2$s) for swap"),
			       dev.c_str(), sizeString().c_str() );
		}
	    else if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format EVMS volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted EVMS volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format EVMS volume %1$s (%2$s) with %3$s"),
			   dev.c_str(), sizeString().c_str(), 
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

string Evms::resizeText( bool doing ) const
    {
    string txt;
    if( doing )
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrinking EVMS volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extending EVMS volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );

        }
    else
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrink EVMS volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/evms/lvm/system/usr
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extend EVMS volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );

        }
    return( txt );
    }

void Evms::getInfo( EvmsInfo& tinfo ) const
    {
    info.stripe = stripe;
    info.stripe_size = stripe_size;
    info.compatible = compat;
    info.dm_table = tname;
    info.dm_target = target;
    tinfo = info;
    }

namespace storage
{

std::ostream& operator<< (std::ostream& s, const Evms &p )
    {
    s << *(Dm*)&p;
    if( !p.compat )
      s << " native";
    return( s );
    }

}

bool Evms::equalContent( const Evms& rhs ) const
    {
    return( Dm::equalContent(rhs) &&
            compat==rhs.compat );
    }

void Evms::logDifference( const Evms& rhs ) const
    {
    string log = stringDifference( rhs );
    if( compat!=rhs.compat )
	{
	if( rhs.compat )
	    log += " -->compat";
	else
	    log += " compat-->";
	}
    y2milestone( "%s", log.c_str() );
    }

Evms& Evms::operator= ( const Evms& rhs )
    {
    y2debug( "operator= from %s", rhs.nm.c_str() );
    *((Dm*)this) = rhs;
    compat = rhs.compat;
    return( *this );
    }

Evms::Evms( const EvmsCo& d, const Evms& rhs ) : Dm(d,rhs)
    {
    y2debug( "constructed evms vol by copy constructor from %s", 
	     rhs.dev.c_str() );
    *this = rhs;
    }

