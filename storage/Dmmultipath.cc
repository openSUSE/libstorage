/*
 * Copyright (c) [2004-2009] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#include <sstream>

#include "storage/Dmmultipath.h"
#include "storage/DmmultipathCo.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"


namespace storage
{
    using namespace std;


    Dmmultipath::Dmmultipath(const DmmultipathCo& c, const string& name, const string& device,
			     unsigned nr, Partition* p)
	: DmPart(c, name, device, nr, p)
    {
	y2mil("constructed Dmmultipath " << dev << " on " << cont->device());
    }


    Dmmultipath::Dmmultipath(const DmmultipathCo& c, const Dmmultipath& v)
	: DmPart(c, v)
    {
	y2deb("copy-constructed Dmmultipath from " << v.dev);
    }


    Dmmultipath::~Dmmultipath()
    {
	y2deb("destructed Dmmultipath " << dev);
    }


Text Dmmultipath::removeText( bool doing ) const
    {
    Text txt;
    string d = dev.substr(12);
    if( p && p->OrigNr()!=p->nr() )
	d = co()->getPartName(p->OrigNr());
    if( doing )
	{
	// displayed text during action, %1$s is replaced by multipath partition name e.g. 3600508b400105f590000900000300000_part1
	txt = sformat( _("Deleting multipath partition %1$s"), d.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by multipath partition name e.g. 3600508b400105f590000900000300000_part1
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete multipath partition %1$s (%2$s)"), d.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

Text Dmmultipath::createText( bool doing ) const
    {
    Text txt;
    string d = dev.substr(12);
    if( doing )
	{
	// displayed text during action, %1$s is replaced by multipath partition name e.g. 3600508b400105f590000900000300000_part1
	txt = sformat( _("Creating multipath partition %1$s"), d.c_str() );
	}
    else
	{
	if( mp=="swap" )
	    {
	    // displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create swap multipath partition %1$s (%2$s)"),
	                   d.c_str(), sizeString().c_str() );
	    }
	else if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create multipath partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create encrypted multipath partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else if( p && p->type()==EXTENDED )
	    {
	    // displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create extended multipath partition %1$s (%2$s)"),
			   d.c_str(), sizeString().c_str() );
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create multipath partition %1$s (%2$s)"),
			   d.c_str(), sizeString().c_str() );
	    }
	}
    return( txt );
    }

Text Dmmultipath::formatText( bool doing ) const
    {
    Text txt;
    string d = dev.substr(12);
    if( doing )
	{
	// displayed text during action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting multipath partition %1$s (%2$s) with %3$s"),
		       d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( !mp.empty() )
	    {
	    if( mp=="swap" )
		{
		// displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
		// %2$s is replaced by size (e.g. 623.5 MB)
		txt = sformat( _("Format multipath partition %1$s (%2$s) for swap"),
			       d.c_str(), sizeString().c_str() );
		}
	    else if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format multipath partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted multipath partition %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format multipath partition %1$s (%2$s) with %3$s"),
			   d.c_str(), sizeString().c_str(),
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

Text Dmmultipath::resizeText( bool doing ) const
    {
    Text txt;
    string d = dev.substr(12);
    if( doing )
        {
	if( needShrink() )
	    // displayed text during action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrinking multipath partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	else
	    // displayed text during action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extending multipath partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	txt += Text(" ", " ");
	// text displayed during action
	txt += _("(progress bar might not move)");
        }
    else
        {
	if( needShrink() )
	    // displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Shrink multipath partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );
	else
	    // displayed text before action, %1$s is replaced by multipath partition e.g. 3600508b400105f590000900000300000_part1
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Extend multipath partition %1$s to %2$s"), d.c_str(), sizeString().c_str() );

        }
    return( txt );
    }

Text Dmmultipath::setTypeText( bool doing ) const
    {
    Text txt;
    string d = dev.substr(12);
    if( doing )
        {
        // displayed text during action, %1$s is replaced by partition name (e.g. 3600508b400105f590000900000300000_part1),
        // %2$s is replaced by hexadecimal number (e.g. 8E)
        txt = sformat( _("Setting type of multipath partition %1$s to %2$X"),
                      d.c_str(), id() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by partition name (e.g. 3600508b400105f590000900000300000_part1),
        // %2$s is replaced by hexadecimal number (e.g. 8E)
        txt = sformat( _("Set type of multipath partition %1$s to %2$X"),
                      d.c_str(), id() );
        }
    return( txt );
    }

void Dmmultipath::getInfo( DmmultipathInfo& tinfo ) const
    {
    DmPart::getInfo( info );
    tinfo.p = info;
    }


std::ostream& operator<< (std::ostream& s, const Dmmultipath &p )
    {
    s << dynamic_cast<const DmPart&>(p);
    return( s );
    }


bool Dmmultipath::equalContent( const Dmmultipath& rhs ) const
    {
    return( DmPart::equalContent(rhs) );
    }


    void
    Dmmultipath::logDifference(std::ostream& log, const Dmmultipath& rhs) const
    {
	DmPart::logDifference(log, rhs);
    }

}
