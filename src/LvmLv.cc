/*
  Textdomain    "storage"
*/

#include <sstream>
#include <boost/algorithm/string.hpp>

#include "storage/LvmLv.h"
#include "storage/LvmVg.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


LvmLv::LvmLv(const LvmVg& d, const string& name, const string& origi,
	     unsigned long le, const string& uuid, const string& stat, 
	     const string& alloc)
    : Dm(d, makeDmTableName(d.name(), name)),
      origin(origi)
{
    init( name );
    setUuid( uuid );
    setStatus( stat );
    setAlloc( alloc );
    setLe( le );
    calcSize();
    getTableInfo();

    y2deb("constructed lvm lv dev:" << dev << " vg:" << cont->name() << " origin:" << origin);
}


LvmLv::LvmLv(const LvmVg& d, const string& name, const string& origi, 
	     unsigned long le, unsigned str)
    : Dm(d, makeDmTableName(d.name(), name)),
      origin(origi)
{
    init( name );
    setLe( le );
    calcSize();
    stripe = str;
    fs = detected_fs = FSNONE;
    alt_names.push_back("/dev/mapper/" + getTableName());

    y2deb("constructed lvm lv dev:" << dev << " vg:" << cont->name() << " origin:" << origin);
}


LvmLv::~LvmLv()
{
    y2deb("destructed lvm lv dev:" << dev);
}


const LvmVg* LvmLv::vg() const
{
    return(dynamic_cast<const LvmVg* const>(cont));
}


    string
    LvmLv::makeDmTableName(const string& vg_name, const string& lv_name)
    {
	return boost::replace_all_copy(vg_name, "-", "--") + "-" +
	    boost::replace_all_copy(lv_name, "-", "--");
    }


void LvmLv::init( const string& name )
    {
    nm = name;
    dev = normalizeDevice( cont->name() + "/" + name );
    Dm::init();
    }


void LvmLv::calcSize()
{
    if (!isSnapshot())
    {
	Dm::calcSize();
    }
    else
    {
	LvmVg::ConstLvmLvPair p = vg()->LvmVg::lvmLvPair(LvmVg::lvNotDeleted);
	LvmVg::ConstLvmLvIter i = p.begin();
	while( i!=p.end() && i->name()!=origin )
	    ++i;
	if (i != p.end())
	{
	    setSize(i->sizeK());
	}
	else
	{
	    setSize(0);
	    y2err("not found " << origin);
	}
    }
}


bool
LvmLv::hasSnapshots() const
{
    LvmVg::ConstLvmLvPair p = vg()->LvmVg::lvmLvPair(LvmVg::lvNotDeleted);
    LvmVg::ConstLvmLvIter i = p.begin();
    while( i!=p.end() && i->getOrigin()!=name() )
	++i;
    return i != p.end();
}


void
LvmLv::getState(LvmLvSnapshotStateInfo& info)
{
    SystemCmd cmd(LVSBIN " --options lv_name,lv_attr,snap_percent " + quote(cont->name()));

    if (cmd.retcode() == 0 && cmd.numLines() > 0)
    {
	for (unsigned int l = 1; l < cmd.numLines(); l++)
	{
	    string line = cmd.getLine(l);
	    
	    if (extractNthWord(0, line) == name())
	    {
		string attr = extractNthWord(1, line);
		info.active = attr.size() >= 6 && attr[4] == 'a';
		
		string percent = extractNthWord(2, line);
		percent >> info.allocated;
	    }
	}
    }
}


string LvmLv::removeText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	txt = sformat( _("Deleting logical volume %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete logical volume %1$s (%2$s)"), dev.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

string LvmLv::createText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	txt = sformat( _("Creating logical volume %1$s"), dev.c_str() );
	}
    else
	{
	if( mp=="swap" )
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create swap logical volume %1$s (%2$s)"),
	                   dev.c_str(), sizeString().c_str() );
	    }
	else if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create encrypted logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create logical volume %1$s (%2$s)"),
			   dev.c_str(), sizeString().c_str() );
	    }
	}
    return( txt );
    }

string LvmLv::formatText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting logical volume %1$s (%2$s) with %3$s"),
		       dev.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( !mp.empty() )
	    {
	    if( mp=="swap" )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Format logical volume %1$s (%2$s) for swap"),
			       dev.c_str(), sizeString().c_str() );
		}
	    else if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted logical volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format logical volume %1$s (%2$s) with %3$s"),
			   dev.c_str(), sizeString().c_str(), 
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

string LvmLv::resizeText( bool doing ) const
    {
    string txt;
    if( doing )
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrinking logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extending logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	// text displayed during action
	txt += string(" ") + _("(progress bar might not move)");
        }
    else
        {
	if( needShrink() )
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrink logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );
	else
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extend logical volume %1$s to %2$s"), dev.c_str(), sizeString().c_str() );

        }
    return( txt );
    }

void LvmLv::getInfo( LvmLvInfo& tinfo ) const
    {
    ((Volume*)this)->getInfo( info.v );
    info.stripes = stripe;
    info.stripeSizeK = stripe_size;
    info.uuid = vol_uuid;
    info.status = status;
    info.allocation = allocation;
    info.dm_table = tname;
    info.dm_target = target;

    info.sizeK = num_le * pec()->peSize();
    info.origin = origin;

    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const LvmLv &p )
    {
    s << *(Dm*)&p;
    if( !p.vol_uuid.empty() )
      s << " UUID:" << p.vol_uuid;
    if( !p.status.empty() )
      s << " " << p.status;
    if( !p.allocation.empty() )
      s << " " << p.allocation;
    return( s );
    }


bool LvmLv::equalContent( const LvmLv& rhs ) const
    {
    return( Dm::equalContent(rhs) &&
            vol_uuid==rhs.vol_uuid && status==rhs.status && 
            allocation==rhs.allocation );
    }

void LvmLv::logDifference( const LvmLv& rhs ) const
    {
    string log = stringDifference( rhs );
    if( vol_uuid!=rhs.vol_uuid )
	log += " UUID:" + vol_uuid + "-->" + rhs.vol_uuid;
    if( status!=rhs.status )
	log += " Status:" + status + "-->" + rhs.status;
    if( allocation!=rhs.allocation )
	log += " Alloc:" + allocation + "-->" + rhs.allocation;
    y2mil(log);
    }

LvmLv& LvmLv::operator= ( const LvmLv& rhs )
    {
    y2deb("operator= from " << rhs.nm);
    *((Dm*)this) = rhs;
    vol_uuid = rhs.vol_uuid;
    status = rhs.status;
    allocation = rhs.allocation;
    return( *this );
    }

LvmLv::LvmLv( const LvmVg& d, const LvmLv& rhs ) : Dm(d,rhs)
    {
    y2deb("constructed lvm lv by copy constructor from " << rhs.dev);
    *this = rhs;
    }

}
