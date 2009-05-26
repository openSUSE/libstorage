/*
  Textdomain    "storage"
*/

#include <sstream>

#include "storage/Dmraid.h"
#include "storage/DmraidCo.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"


namespace storage
{
    using namespace std;


Dmraid::Dmraid(const DmraidCo& d, unsigned nr, Partition* p)
    : DmPart(d, nr, p)
{
    y2mil("constructed dmraid " << dev << " on co " << cont->name());
}


Dmraid::~Dmraid()
{
    y2deb("destructed dmraid " << dev);
}


string Dmraid::removeText( bool doing ) const
    {
    string txt;
    string d = dev.substr(12);
    if( p && p->OrigNr()!=p->nr() )
	d = co()->numToName(p->OrigNr());
    if( doing )
	{
	// displayed text during action, %1$s is replaced by raid partition name e.g. pdc_dabaheedj_part1
	txt = sformat( _("Deleting raid partition %1$s"), d.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by raid partition name e.g. pdc_dabaheedj_part1
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete raid partition %1$s (%2$s)"), d.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

string Dmraid::createText( bool doing ) const
    {
    string txt;
    string d = dev.substr(12);
    if( doing )
	{
	// displayed text during action, %1$s is replaced by raid partition name e.g. pdc_dabaheedj_part1
	txt = sformat( _("Creating raid partition %1$s"), d.c_str() );
	}
    else
	{
	if( mp=="swap" )
	    {
	    // displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create swap raid partition %1$s (%2$s)"),
	                   d.c_str(), sizeString().c_str() );
	    }
	else if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create raid partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create encrypted raid partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else if( p && p->type()==EXTENDED )
	    {
	    // displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create extended raid partition %1$s (%2$s)"),
			   d.c_str(), sizeString().c_str() );
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create raid partition %1$s (%2$s)"),
			   d.c_str(), sizeString().c_str() );
	    }
	}
    return( txt );
    }

string Dmraid::formatText( bool doing ) const
    {
    string txt;
    string d = dev.substr(12);
    if( doing )
	{
	// displayed text during action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting raid partition %1$s (%2$s) with %3$s"),
		       d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( !mp.empty() )
	    {
	    if( mp=="swap" )
		{
		// displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Format raid partition %1$s (%2$s) for swap"),
			       d.c_str(), sizeString().c_str() );
		}
	    else if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format raid partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted raid partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format raid partition %1$s (%2$s) with %3$s"),
			   d.c_str(), sizeString().c_str(),
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

string Dmraid::resizeText( bool doing ) const
    {
    string txt;
    string d = dev.substr(12);
    if( doing )
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrinking raid partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extending raid partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	// text displayed during action
	txt += string(" ") + _("(progress bar might not move)");
        }
    else
        {
	if( needShrink() )
	    // displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrink raid partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	else
	    // displayed text before action, %1$s is replaced by raid partition e.g. pdc_dabaheedj_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extend raid partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );

        }
    return( txt );
    }

string Dmraid::setTypeText( bool doing ) const
    {
    string txt;
    string d = dev.substr(12);
    if( doing )
        {
        // displayed text during action, %1$s is replaced by partition name (e.g. pdc_dabaheedj_part1),
        // %2$s is replaced by hexadecimal number (e.g. 8E)
        txt = sformat( _("Setting type of raid partition %1$s to %2$X"),
                      d.c_str(), id() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by partition name (e.g. pdc_dabaheedj_part1),
        // %2$s is replaced by hexadecimal number (e.g. 8E)
        txt = sformat( _("Set type of raid partition %1$s to %2$X"),
                      d.c_str(), id() );
        }
    return( txt );
    }

void Dmraid::getInfo( DmraidInfo& tinfo ) const
    {
    DmPart::getInfo( info );
    tinfo.p = info;
    }


std::ostream& operator<< (std::ostream& s, const Dmraid &p )
    {
    s << *(DmPart*)&p;
    return( s );
    }


bool Dmraid::equalContent( const Dmraid& rhs ) const
    {
    return( DmPart::equalContent(rhs) );
    }

void Dmraid::logDifference( const Dmraid& rhs ) const
    {
    DmPart::logDifference(rhs);
    }


Dmraid& Dmraid::operator=(const Dmraid& rhs)
{
    y2deb("operator= from " << rhs.nm);
    *((DmPart*)this) = rhs;
    return *this;
}

}
