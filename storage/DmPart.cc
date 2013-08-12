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

#include "storage/DmPart.h"
#include "storage/DmPartCo.h"
#include "storage/SystemCmd.h"
#include "storage/ProcParts.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"


namespace storage
{
    using namespace std;


    DmPart::DmPart(const DmPartCo& c, const string& name, const string& device, unsigned nr,
		   Partition* pa)
	: Dm(c, name, device, name), p(pa)
    {
    Dm::init();
    numeric = true;
    num = nr;
    getTableInfo();
    if( pa )
	setSize( pa->sizeK() );
    y2mil("constructed DmPart " << dev << " on " << cont->device());
}

    DmPart::DmPart(const DmPartCo& c, const string& name, const string& device, unsigned nr,
		   Partition* pa, SystemInfo& si)
	: DmPart(c, name, device, nr, pa)
    {
    Dm::setUdevData(si);
    }


    DmPart::DmPart(const DmPartCo& c, const DmPart& v)
	: Dm(c, v)
    {
	y2deb("copy-constructed DmPart from " << v.dev);
    }


    DmPart::~DmPart()
    {
	y2deb("destructed DmPart " << dev);
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
	setNameDevice(co()->getPartName(num), co()->getPartDevice(num));
	}
    }

void DmPart::updateMinor()
    {
    unsigned long old_mjr = mjr;
    unsigned long old_mnr = mnr;
    getMajorMinor();
    if (mjr != old_mjr || mnr != old_mnr)
	{
	getTableInfo();
	replaceAltName("/dev/dm-", "/dev/dm-" + decString(mnr));
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


    void
    DmPart::updateSize(const ProcParts& parts)
    {
    unsigned long long si = 0;
    updateSize();
    if (mjr > 0 && parts.getSize("/dev/dm-" + decString(mnr), si))
	setSize( si );
    }


void DmPart::addUdevData()
    {
    addAltUdevId( num );
    }


void
DmPart::addAltUdevId(unsigned num)
    {
    list<string> by_id;
    list<string>::iterator e = alt_names.begin();
    while( e!=alt_names.end() )
	{
	if( boost::contains(*e,"/by-id/") )
	    {
	    by_id.push_back(*e);
	    e=alt_names.erase(e);
	    }
	else
	    ++e;
	}
    const list<string> tmp = co()->udevId();
    for (list<string>::const_iterator i = tmp.begin(); i != tmp.end(); ++i)
	{
	string s = "/dev/disk/by-id/"+((num>0)?udevAppendPart(*i, num):*i);
	e = find( by_id.begin(), by_id.end(), s );
	if( e!=by_id.end() )
	    by_id.erase(e);
	alt_names.push_back(s);
	}
    alt_names.splice(alt_names.end(),by_id);
    mount_by = orig_mount_by = defaultMountBy();
    }


    list<string>
    DmPart::udevId() const
    {
	list<string> ret;
	const list<string> tmp = co()->udevId();
	for (list<string>::const_iterator i = tmp.begin(); i != tmp.end(); ++i)
	    ret.push_back(udevAppendPart(*i, num));
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
	    l.push_back(commitAction(INCREASE, cont->type(),
				     setTypeText(false), this, false));
	}
    }


Text DmPart::setTypeText( bool doing ) const
    {
    Text txt;
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


    list<string>
    DmPart::getUsing() const
    {
	list<string> ret;
	ret.push_back(cont->device());
	return ret;
    }


void DmPart::getInfo( DmPartInfo& info ) const
    {
    Volume::getInfo(info.v);
    if( p )
	p->getInfo( info.p );
    info.part = p!=NULL;
    info.table = tname;
    info.target = target;
    }


std::ostream& operator<< (std::ostream& s, const DmPart &p )
    {
    s << dynamic_cast<const Dm&>(p);
    return( s );
    }


bool DmPart::equalContent( const DmPart& rhs ) const
    {
    return( Dm::equalContent(rhs) );
    }


    void
    DmPart::logDifference(std::ostream& log, const DmPart& rhs) const
    {
	Dm::logDifference(log, rhs);
    }

}
