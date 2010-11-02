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


#include <ostream>
#include <sstream>

#include "storage/DmraidCo.h"
#include "storage/Dmraid.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"
#include "storage/SystemInfo.h"


namespace storage
{
    using namespace std;


    CmdDmraid::CmdDmraid()
    {
	SystemCmd c(DMRAIDBIN " -s -c -c -c");
	if (c.retcode() != 0 || c.stdout().empty())
	    return;

	const vector<string>& lines = c.stdout();
	vector<string>::const_iterator it = lines.begin();
	while (it != lines.end())
	{
	    const list<string> sl = splitString(*it++, ":");
	    if (sl.size() >= 4)
	    {
		Entry entry;

		list<string>::const_iterator ci = sl.begin();
		string name = *ci;
		advance(ci, 3);
		entry.raidtype = *ci;

		while (it != lines.end() && boost::starts_with(*it, "/dev/"))
		{
		    const list<string> sl = splitString(*it, ":");
		    if (sl.size() >= 4)
		    {
			list<string>::const_iterator ci = sl.begin();
			entry.devices.push_back(*ci);
			advance(ci, 1);
			entry.controller = *ci;
		    }

		    ++it;
		}

		data[name] = entry;
	    }
	}

	for (const_iterator it = data.begin(); it != data.end(); ++it)
	    y2mil("data[" << it->first << "] -> controller:" << it->second.controller <<
		  " raidtype:" << it->second.raidtype << " devices:" << it->second.devices);
    }


    list<string>
    CmdDmraid::getEntries() const
    {
	list<string> ret;
	for (const_iterator it = data.begin(); it != data.end(); ++it)
	    ret.push_back(it->first);
	return ret;
    }


    bool
    CmdDmraid::getEntry(const string& name, Entry& entry) const
    {
	const_iterator it = data.find(name);
	if (it == data.end())
	    return false;

	entry = it->second;
	return true;
    }


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
	    CmdDmsetup::Entry entry;
	    if (systeminfo.getCmdDmsetup().getEntry(*it, entry) && entry.segments > 0)
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
        txt = sformat( _("Removing raid %1$s"), name().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by a name (e.g. pdc_igeeeadj),
        txt = sformat( _("Remove raid %1$s"), name().c_str() );
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
	if( !silent )
	    {
	    getStorage()->showInfoCb( removeText(true) );
	    }
	string cmd = "cd /var/log/YaST2 && echo y | " DMRAIDBIN " -E -r";
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

void DmraidCo::getInfo( DmraidCoInfo& tinfo ) const
    {
    DmPartCo::getInfo( info );
    tinfo.p = info;
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
