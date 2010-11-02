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

#include "storage/DmCo.h"
#include "storage/Dm.h"
#include "storage/SystemCmd.h"
#include "storage/SystemInfo.h"
#include "storage/ProcParts.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    CmdDmsetup::CmdDmsetup()
    {
	SystemCmd c(DMSETUPBIN " --columns --noheadings -o name,major,minor,segments,uuid info");
	if (c.retcode() != 0 || c.numLines() == 0)
	    return;

	for (vector<string>::const_iterator it = c.stdout().begin(); it != c.stdout().end(); ++it)
	{
	    list<string> sl = splitString(*it, ":");
	    if (sl.size() >= 4)
	    {
		Entry entry;

		list<string>::const_iterator ci = sl.begin();
		string name = *ci++;
		*ci++ >> entry.mjr;
		*ci++ >> entry.mnr;
		*ci++ >> entry.segments;
		entry.uuid = *ci++;

		data[name] = entry;
	    }
	}

	for (const_iterator it = data.begin(); it != data.end(); ++it)
	    y2mil("data[" << it->first << "] -> mjr:" << it->second.mjr << " mnr:" <<
		  it->second.mnr << " segments:" << it->second.segments << " uuid:" <<
		  it->second.uuid);
    }


    bool
    CmdDmsetup::getEntry(const string& name, Entry& entry) const
    {
	const_iterator it = data.find(name);
	if (it == data.end())
	    return false;

	entry = it->second;
	return true;
    }


    list<string>
    CmdDmsetup::getEntries() const
    {
	list<string> ret;
	for (const_iterator i = data.begin(); i != data.end(); ++i)
	    ret.push_back(i->first);
	return ret;
    }


    DmCo::DmCo(Storage* s)
	: PeContainer(s, "mapper", "/dev/mapper", staticType())
    {
	y2deb("constructing DmCo");
    }


    DmCo::DmCo(Storage* s, SystemInfo& systeminfo, bool only_crypt)
	: PeContainer(s, "mapper", "/dev/mapper", staticType(), systeminfo)
    {
	y2deb("constructing DmCo");
	getDmData(systeminfo, only_crypt);
    }


    void
    DmCo::second(SystemInfo& systeminfo, bool only_crypt)
    {
	y2deb("second DmCo");
	getDmData(systeminfo, only_crypt);
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
	for( DmIter i=dp.begin(); i!=dp.end(); ++i )
	    {
	    if( i->getPeMap().empty() )
		{
		y2mil( "dm:" << *i );
		i->getTableInfo();
		if( !i->getPeMap().empty() )
		    {
		    success = true;
		    y2mil( "dm:" << *i );
		    }
		}
	    }
	}
    while( success );
    }


// dev should be something like /dev/mapper/cr_test
storage::EncryptType
DmCo::detectEncryption( const string& dev ) const
{
    storage::EncryptType ret = ENC_UNKNOWN;

    if( dev.substr( 0, 12 ) == "/dev/mapper/")
    {
	string tdev = dev.substr (12);
	SystemCmd c(CRYPTSETUPBIN " status " + quote(tdev));

	string cipher, keysize;
	for( unsigned int i = 0; i < c.numLines(); i++)
	{
	    string line = c.getLine(i);
	    string key = extractNthWord( 0, line );
	    if( key == "cipher:" )
		cipher = extractNthWord( 1, line );
	    if( key == "keysize:" )
		keysize = extractNthWord( 1, line );
	}

	if( cipher == "aes-cbc-essiv:sha256" || cipher == "aes-cbc-plain")
	    ret = ENC_LUKS;
	else if( cipher == "twofish-cbc-plain" )
	    ret = ENC_TWOFISH;
	else if( cipher == "twofish-cbc-null" && keysize == "192" )
	    ret = ENC_TWOFISH_OLD;
	else if( cipher == "twofish-cbc-null" && keysize == "256" )
	    ret = ENC_TWOFISH256_OLD;
    }

    y2mil("ret:" << toString(ret));
    return ret;
}


void
    DmCo::getDmData(SystemInfo& systeminfo, bool only_crypt)
    {
    Storage::ConstLvmLvPair lv = getStorage()->lvmLvPair();
    Storage::ConstDmraidCoPair dmrco = getStorage()->dmraidCoPair();
    Storage::ConstDmraidPair dmr = getStorage()->dmrPair();
    Storage::ConstDmmultipathCoPair dmmco = getStorage()->dmmultipathCoPair();
    Storage::ConstDmmultipathPair dmm = getStorage()->dmmPair();

    const CmdDmsetup& cmddmsetup = systeminfo.getCmdDmsetup();
    for (CmdDmsetup::const_iterator it1 = cmddmsetup.begin(); it1 != cmddmsetup.end(); ++it1)
    {
	string table = it1->first;
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
	    const CmdDmsetup::Entry& entry = it1->second;

	    Dm* m = new Dm(*this, table, "/dev/mapper/" + table, table, systeminfo);
	    y2mil("new Dm:" << *m);
	    unsigned long long s = 0;
	    string dev = "/dev/dm-" + decString(entry.mnr);
	    if (systeminfo.getProcParts().getSize(dev, s))
		{
		y2mil( "new dm size:" << s );
		m->setSize( s );
		}
	    bool in_use = false;
	    const map<string,unsigned long>& pe = m->getPeMap();
	    bool multipath = m->getTargetName()=="multipath" ||
			     m->getTargetName()=="emc";
	    map<string,unsigned long>::const_iterator it;
	    for( it=pe.begin(); it!=pe.end(); ++it )
		{
		if( !getStorage()->canUseDevice( it->first, true ))
		    in_use = true;
		if( !in_use || multipath )
		    getStorage()->setUsedBy(it->first, UB_DM, "/dev/mapper/" + table);
		}
	    string tmp = m->device();
	    tmp.erase( 5, 7 );
	    bool skip = getStorage()->knownDevice( tmp, true );
	    y2mil( "in_use:" << in_use << " multipath:" << multipath <<
	           " known " << tmp << " is:" << skip );
	    it=pe.begin();
	    if( !skip && m->getTargetName()=="crypt" && it!=pe.end() &&
		getStorage()->knownDevice( it->first ))
		{
		skip = true;
		getStorage()->setDmcryptData( it->first, m->device(), entry.mnr,
		                              m->sizeK(), detectEncryption (m->device()) );
		if (getStorage()->isUsedBy(it->first, UB_DM))
		    getStorage()->clearUsedBy(it->first);
		}
	    if (!skip && m->sizeK()>0 && !only_crypt )
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
	    const map<string,unsigned long>& pe = i->getPeMap();
	    for( map<string,unsigned long>::const_iterator it = pe.begin();
		 it!=pe.end(); ++it )
		{
		getStorage()->clearUsedBy(it->first);
		}
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
	if( !silent )
	    {
	    getStorage()->showInfoCb( m->removeText(true) );
	    }
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
    ConstDmPair p = rhs.dmPair();
    for( ConstDmIter i = p.begin(); i!=p.end(); ++i )
	{
	Dm * p = new Dm( *this, *i );
	vols.push_back( p );
        }
    }

}
