/*
 * Copyright (c) [2004-2014] Novell, Inc.
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


#include <ostream>
#include <sstream>

#include "storage/DmraidCo.h"
#include "storage/Dmraid.h"
#include "storage/Utils/SystemCmd.h"
#include "storage/Utils/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"
#include "storage/SystemInfo/SystemInfo.h"


namespace storage
{
    using namespace std;


    DmraidCo::DmraidCo(Storage* s, const string& name, const string& device, SystemInfo& systeminfo)
	: DmPartCo(s, name, device, staticType(), systeminfo)
    {
	getRaidData(name, systeminfo);

	const UdevMap& by_id = systeminfo.getUdevMap("/dev/disk/by-id");
	UdevMap::const_iterator it = by_id.find("dm-" + decString(minorNr()));
	if (it != by_id.end())
	    setUdevData(it->second);

	y2deb("constructing DmraidCo " << name);
    }


    DmraidCo::DmraidCo(const DmraidCo& c)
	: DmPartCo(c), raidtype(c.raidtype), controller(c.controller)
    {
	y2deb("copy-constructed DmraidCo from " << c.dev);
    }


    DmraidCo::~DmraidCo()
    {
	y2deb("destructed DmraidCo " << dev);
    }


    void
    DmraidCo::getRaidData(const string& name, SystemInfo& systeminfo)
    {
	y2mil("name:" << name);

	CmdDmraid::Entry entry;
	if (systeminfo.getCmdDmraid().getEntry(name, entry))
	{
	    controller = entry.controller;
	    raidtype = entry.raidtype;

	    for (list<string>::const_iterator it = entry.devices.begin(); it != entry.devices.end(); ++it)
	    {
		Pv pv;
		pv.device = *it;
		addPv(pv);
	    }
	}
    }


    void
    DmraidCo::setUdevData( const list<string>& id )
    {
	y2mil("disk:" << nm << " id:" << id);
	udev_id = id;
	udev_id.remove_if(string_starts_with("dm-"));
	y2mil("id:" << udev_id);

	DmPartCo::setUdevData(udev_id);

	DmraidPair pp = dmraidPair();
	for( DmraidIter p=pp.begin(); p!=pp.end(); ++p )
	{
	    p->addUdevData();
	}
    }


    void
    DmraidCo::newP( DmPart*& dm, unsigned num, Partition* p )
    {
	y2mil( "num:" << num );
	dm = new Dmraid( *this, getPartName(num), getPartDevice(num), num, p );
    }

    void
    DmraidCo::newP( DmPart*& dm, unsigned num, Partition* p, SystemInfo& si )
    {
	y2mil( "num:" << num );
	dm = new Dmraid( *this, getPartName(num), getPartDevice(num), num, p, si );
    }

    void
    DmraidCo::addPv(const Pv& pv)
    {
	PeContainer::addPv(pv);
	if (!deleted())
	    getStorage()->addUsedBy(pv.device, UB_DMRAID, device());
    }


    void DmraidCo::activate( bool val )
    {
	if (getenv("LIBSTORAGE_NO_DMRAID") != NULL)
	    return;

	y2mil("old active:" << active << " val:" << val);
	if( active != val )
	{
	    SystemCmd c;
	    if( val )
	    {
		Dm::activate(true);
		// option '-p' since udev creates the partition nodes
		c.execute(DMRAIDBIN " -ay -p");
	    }
	    else
	    {
		c.execute(DMRAIDBIN " -an");
	    }
	    active = val;
	}
	Storage::waitForDevice();
    }


    list<string>
    DmraidCo::getRaids(SystemInfo& systeminfo)
    {
        list<string> l;

	list<string> entries = systeminfo.getCmdDmraid().getEntries();
	for (list<string>::const_iterator it = entries.begin(); it != entries.end(); ++it)
        {
	    CmdDmsetupInfo::Entry entry;
	    if (systeminfo.getCmdDmsetupInfo().getEntry(*it, entry) && entry.segments > 0)
      		l.push_back(*it);
	    else
		y2mil("ignoring inactive dmraid " << *it);
        }

        y2mil("detected dmraids " << l);
        return l;
    }


    Text DmraidCo::removeText( bool doing ) const
    {
	Text txt;
	if( doing )
        {
	    // displayed text during action, %1$s is replaced by a name (e.g. pdc_igeeeadj),
	    txt = sformat( _("Removing RAID %1$s"), name().c_str() );
        }
	else
        {
	    // displayed text before action, %1$s is replaced by a name (e.g. pdc_igeeeadj),
	    txt = sformat( _("Remove RAID %1$s"), name().c_str() );
        }
	return( txt );
    }


    int
    DmraidCo::doRemove()
    {
	y2mil("Raid:" << name());
	int ret = 0;
	if( deleted() )
	{
	    if( active )
	    {
		activate_part(false);
		activate(false);
	    }
	    getStorage()->showInfoCb( removeText(true), silent );
	    string cmd = "cd " + quote(getStorage()->logdir()) + " && echo y | " DMRAIDBIN " -E -r";
	    SystemCmd c;
	    for( list<Pv>::const_iterator i=pv.begin(); i!=pv.end(); ++i )
	    {
		c.execute(cmd + " " + quote(i->device));
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
	y2mil("ret:" << ret);
	return( ret );
    }

    void DmraidCo::getInfo( DmraidCoInfo& info ) const
    {
	DmPartCo::getInfo( info.p );
    }


    std::ostream& operator<< (std::ostream& s, const DmraidCo& d )
    {
	s << dynamic_cast<const DmPartCo&>(d);
	s << " Cont:" << d.controller
	  << " RType:" << d.raidtype;
	return( s );
    }


    void
    DmraidCo::logDifference(std::ostream& log, const DmraidCo& rhs) const
    {
	DmPartCo::logDifference(log, rhs);

	logDiff(log, "controller", controller, rhs.controller);
	logDiff(log, "raidtype", raidtype, rhs.raidtype);
    }


    void
    DmraidCo::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
	const DmraidCo& rhs = dynamic_cast<const DmraidCo&>(rhs_c);

	logDifference(log, rhs);
	log << endl;

	ConstDmPartPair pp = dmpartPair();
	ConstDmPartPair pc = rhs.dmpartPair();
	logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


    bool DmraidCo::equalContent( const Container& rhs ) const
    {
	bool ret = Container::equalContent(rhs);
	if( ret )
	{
	    const DmraidCo *p = dynamic_cast<const DmraidCo*>(&rhs);
	    ret = p && DmPartCo::equalContent( *p ) &&
		controller==p->controller && raidtype==p->raidtype;
	}
	return( ret );
    }


    bool DmraidCo::active = false;

}
