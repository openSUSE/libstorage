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

#include "storage/DmCo.h"
#include "storage/Dm.h"
#include "storage/SystemCmd.h"
#include "storage/SystemInfo/SystemInfo.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    DmCo::DmCo(Storage* s)
	: PeContainer(s, "mapper", "/dev/mapper", staticType())
    {
	y2deb("constructing DmCo");
    }


    DmCo::DmCo(Storage* s, SystemInfo& systeminfo, bool only_crypt)
	: PeContainer(s, "mapper", "/dev/mapper", staticType(), systeminfo)
    {
	y2deb("constructing DmCo");
        if(!only_crypt)
            getDmData(systeminfo);
        getDmDataCrypt(systeminfo);
    }


    void
    DmCo::second(SystemInfo& systeminfo, bool only_crypt)
    {
	y2deb("second DmCo");
        if(!only_crypt)
            getDmData(systeminfo);
        getDmDataCrypt(systeminfo);
    }


    DmCo::~DmCo()
    {
    y2deb("destructed DmCo");
    }


void DmCo::updateDmMaps()
    {
    DmPair dp = dmPair();
    bool success;
    do
	{
	success = false;
	for (auto &i : dp)
	    {
	    if (i.getPeMap().empty())
		{
		y2mil("dm:" << i);
		i.getTableInfo();
		if (!i.getPeMap().empty())
		    {
		    success = true;
		    y2mil("dm:" << i);
		    }
		}
	    }
	}
    while( success );
    }


    // device should be something like /dev/mapper/cr_test
    storage::EncryptType
    DmCo::detectEncryption(SystemInfo& systeminfo, const string& device) const
    {
	return systeminfo.getCmdCryptsetup(device.substr(12)).encrypt_type;
    }


void DmCo::getDmData(SystemInfo& systeminfo)
    {
    y2mil( "begin:" );
    Storage::ConstLvmLvPair lv = getStorage()->lvmLvPair();
    Storage::ConstDmraidCoPair dmrco = getStorage()->dmraidCoPair();
    Storage::ConstDmraidPair dmr = getStorage()->dmrPair();
    Storage::ConstDmmultipathCoPair dmmco = getStorage()->dmmultipathCoPair();
    Storage::ConstDmmultipathPair dmm = getStorage()->dmmPair();

    const CmdDmsetupInfo& cmddmsetupinfo = systeminfo.getCmdDmsetupInfo();
    list<string> lvm_pools;
    list<string> lvm_tmeta;
    for (auto const &it1 : cmddmsetupinfo)
        {
        if (boost::ends_with(it1.first, "-tpool"))
            {
            string name = it1.first.substr(0, it1.first.size() - 6);
            if( find( lvm_pools.begin(), lvm_pools.end(), name )==lvm_pools.end() )
                {
                string tb = it1.first;
                Dm* m = new Dm(*this, tb, "/dev/mapper/" + tb, tb, systeminfo);
                if( m && m->getTargetName()=="thin-pool" )
                    {
                    lvm_pools.push_back( name );
                    }
                if(m)
                    delete(m);
                }
            }
        if (boost::ends_with(it1.first, "_tmeta"))
            {
            string name = it1.first.substr(0, it1.first.size() - 6);
	    if( find( lvm_tmeta.begin(), lvm_tmeta.end(), name )==lvm_tmeta.end() )
		lvm_tmeta.push_back( name );
	    }
        }
    y2mil( "lvm_pools:" << lvm_pools );
    y2mil( "lvm_tmeta:" << lvm_tmeta );
    for (auto const &it1 : cmddmsetupinfo)
        {
        if (boost::ends_with(it1.first, "_tdata"))
            {
            string name = it1.first.substr(0, it1.first.size() - 6);
	    if( find( lvm_tmeta.begin(), lvm_tmeta.end(), name )!=lvm_tmeta.end() &&
	        find( lvm_pools.begin(), lvm_pools.end(), name )==lvm_pools.end() )
		lvm_pools.push_back( name );
	    }
        }
    y2mil( "lvm_pools:" << lvm_pools );
    for (auto const &it1 : cmddmsetupinfo)
    {
	string table = it1.first;
	bool found=false;
	if (!found)
	{
	    Storage::ConstLvmLvIterator i=lv.begin();
	    while( !found && i!=lv.end() )
	    {
		found = i->getTableName()==table;
		++i;
	    }
	}
	if( !found )
	    {
	    Storage::ConstDmraidCoIterator i=dmrco.begin();
	    while( !found && i!=dmrco.end() )
		{
		found = i->name()==table;
		++i;
		}
	    }
	if( !found )
	    {
	    Storage::ConstDmraidIterator i=dmr.begin();
	    while( !found && i!=dmr.end() )
		{
		found = i->getTableName()==table;
		++i;
		}
	    }
	if( !found )
	    {
	    Storage::ConstDmmultipathCoIterator i=dmmco.begin();
	    while( !found && i!=dmmco.end() )
		{
		found = i->name()==table;
		++i;
		}
	    }
	if( !found )
	    {
	    Storage::ConstDmmultipathIterator i=dmm.begin();
	    while( !found && i!=dmm.end() )
		{
		found = i->getTableName()==table;
		++i;
		}
	    }
	if( !found )
	    {
	    const CmdDmsetupInfo::Entry& entry = it1.second;

	    Dm* m = new Dm(*this, table, "/dev/mapper/" + table, table, systeminfo);
	    y2mil("new Dm:" << *m);
	    bool in_use = false;
	    bool multipath = m->getTargetName()=="multipath" ||
			     m->getTargetName()=="emc";
	    for (auto const &it : m->getPeMap())
		{
		if (!getStorage()->canUseDevice(it.first, true))
		    in_use = true;
                y2mil("dev:" << it.first << " in_use:" << in_use);
		if (!in_use || multipath)
		    getStorage()->setUsedBy(it.first, UB_DM, "/dev/mapper/" + table);
		}
	    string tmp = m->device();
	    tmp.erase( 5, 7 );
	    bool skip = getStorage()->knownDevice( tmp, true ) || m->getTargetName()=="crypt";
	    y2mil( "in_use:" << in_use << " multipath:" << multipath <<
	           " known " << tmp << " is:" << skip );
            static Regex raid1( "_rimage_[0-9]+$" );
            static Regex raid2( "_rmeta_[0-9]+$" );
	    if( !skip && (raid1.match(table)||raid2.match(table)))
		{
                string::size_type off = std::max( raid1.so(0), raid2.so(0) );
                string nm = Dm::lvmTableToDev( table.substr( 0, off ) );
                skip = getStorage()->knownDevice( nm );
                y2mil( "raid table:" << table << " name:" << nm << " skip:" << skip );
                }
	    if( !skip && (boost::ends_with(table,"-tpool")||
                          boost::ends_with(table,"_tdata")||
                          boost::ends_with(table,"_tmeta")))
		{
                string nm = table.substr( 0, table.size()-6 );
                skip = find( lvm_pools.begin(), lvm_pools.end(), nm )!=lvm_pools.end();
                y2mil( "pool table:" << table << " name:" << nm << " skip:" << skip );
                }
	    if( !skip && (boost::ends_with(table,"-real")||
                          boost::ends_with(table,"-cow")))
		{
                string on = table;
                if( boost::ends_with(on,"-real"))
                    on.erase( on.size()-5 );
                if( boost::ends_with(tmp,"-cow"))
                    on.erase( on.size()-4 );
                on = Dm::lvmTableToDev( on );
                skip = getStorage()->knownDevice( on );
                y2mil( "snap devname:" << on << " skip:" << skip );
		}
            if( !skip )
                {
                unsigned long long s = 0;
                if( getProcSize( systeminfo, entry.mnr, s ))
                    {
                    y2mil( "new dm size:" << s );
                    m->setSize( s );
                    }
                }
	    if (!skip && m->sizeK()>0)
		addDm( m );
	    else
		delete( m );
	    }
	}
    }

void
DmCo::addDm( Dm* m )
    {
    if( !findDm( m->nr() ))
	addToList( m );
    else
	{
	y2war("addDm already exists " << m->nr());
	delete m;
	}
    }

void DmCo::getDmDataCrypt(SystemInfo& systeminfo)
    {
    y2mil( "begin:" );
    const CmdDmsetupInfo& cmddmsetupinfo = systeminfo.getCmdDmsetupInfo();
    for (auto const &it1 : cmddmsetupinfo)
        {
	string table = it1.first;
        const CmdDmsetupInfo::Entry& entry = it1.second;
        string dev = "/dev/mapper/" + table;

        if( boost::starts_with(entry.uuid, "CRYPT"))
            {
            Dm* m = new Dm(*this, table, "/dev/mapper/" + table, table, systeminfo);
            y2mil("new Dm:" << *m);
	    const map<string,unsigned long>& pe = m->getPeMap();
	    map<string,unsigned long>::const_iterator it;
	    it=pe.begin();
	    if( m->getTargetName()=="crypt" && it!=pe.end() &&
		getStorage()->knownDevice( it->first ))
		{
                unsigned long long s = 0;
                if( getProcSize( systeminfo, entry.mnr, s ))
                    m->setSize(s);
		getStorage()->setDmcryptData(it->first, m->device(), entry.mnr, m->sizeK(),
					     detectEncryption(systeminfo, m->device()));
		if (getStorage()->isUsedBy(it->first, UB_DM))
		    getStorage()->clearUsedBy(it->first);
		}
            delete( m );
	    }
	}
    }

bool DmCo::getProcSize(SystemInfo& si, unsigned nr, unsigned long long& s)
    {
    s=0;
    string dev = "/dev/dm-" + decString(nr);
    return( si.getProcParts().getSize(dev, s) );
    }

bool
DmCo::findDm( unsigned num, DmIter& i )
    {
    DmPair p=dmPair(Dm::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->nr()!=num )
	++i;
    return( i!=p.end() );
    }

bool
DmCo::findDm( unsigned num )
    {
    DmIter i;
    return( findDm( num, i ));
    }

bool
DmCo::findDm( const string& tname, DmIter& i )
    {
    DmPair p=dmPair(Dm::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->getTableName()!=tname )
	++i;
    return( i!=p.end() );
    }

bool
DmCo::findDm( const string& tname )
    {
    DmIter i;
    return( findDm( tname, i ));
    }

int 
DmCo::removeDm( const string& tname )
    {
    int ret = 0;
    y2mil("tname:" << tname);
    DmIter i;
    if( readonly() )
	{
	ret = DM_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findDm( tname, i ))
	    ret = DM_UNKNOWN_TABLE;
	}
    if (ret == 0 && i->isUsedBy())
	{
	ret = DM_REMOVE_USED_BY;
	}
    if( ret==0 )
	{
	if( i->created() )
	    {
	    if( !removeFromList( &(*i) ))
		ret = DM_REMOVE_CREATE_NOT_FOUND;
	    }
	else
	    {
	    for (auto const &it : i->getPeMap())
		getStorage()->clearUsedBy(it.first);
	    i->setDeleted();
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int DmCo::removeVolume( Volume* v )
    {
    int ret = 0;
    y2mil("name:" << v->name());
    Dm * m = dynamic_cast<Dm *>(v);
    if( m != NULL )
	ret = removeDm( m->getTableName() );
    else 
	ret = DM_REMOVE_INVALID_VOLUME;
    return( ret );
    }

int 
DmCo::doRemove( Volume* v )
    {
    y2mil("name:" << v->name());
    Dm * m = dynamic_cast<Dm *>(v);
    int ret = 0;
    if( m != NULL )
	{
	getStorage()->showInfoCb( m->removeText(true), silent );
	ret = m->prepareRemove();
	if( ret==0 )
	    {
	    string cmd = DMSETUPBIN " remove " + quote(m->getTableName());
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		ret = DM_REMOVE_FAILED;
	    else
		Storage::waitForDevice();
	    y2mil( "this:" << *this );
	    getStorage()->logProcData( cmd );
	    }
	if( ret==0 )
	    {
	    if( !removeFromList( m ) )
		ret = DM_NOT_IN_LIST;
	    }
	}
    else
	ret = DM_REMOVE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }


    std::ostream& operator<<(std::ostream& s, const DmCo& d)
    {
    s << dynamic_cast<const Container&>(d);
    return( s );
    }


    void
    DmCo::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
	const DmCo& rhs = dynamic_cast<const DmCo&>(rhs_c);

	logDifference(log, rhs);
	log << endl;

	ConstDmPair pp = dmPair();
	ConstDmPair pc = rhs.dmPair();
	logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


bool DmCo::equalContent( const Container& rhs ) const
    {
    const DmCo * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const DmCo*>(&rhs);
    if( ret && p )
	{
	ConstDmPair pp = dmPair();
	ConstDmPair pc = p->dmPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
	}
    return( ret );
    }

DmCo::DmCo( const DmCo& rhs ) : PeContainer(rhs)
    {
    y2deb("constructed DmCo by copy constructor from " << rhs.nm);
    for (auto const &i : rhs.dmPair())
        vols.push_back(new Dm(*this, i));
    }

}
