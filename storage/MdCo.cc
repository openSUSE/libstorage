/*
 * Copyright (c) [2004-2010] Novell, Inc.
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
#include <fstream>

#include "storage/MdCo.h"
#include "storage/Md.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/EtcMdadm.h"
#include "storage/StorageDefines.h"
#include "storage/AsciiFile.h"
#include "storage/SystemInfo/SystemInfo.h"


namespace storage
{
    using namespace std;


    MdCo::MdCo(Storage* s)
	: Container(s, "md", "/dev/md", staticType())
    {
	y2deb("constructing MdCo");
    }


    MdCo::MdCo(Storage* s, SystemInfo& systeminfo)
	: Container(s, "md", "/dev/md", staticType(), systeminfo)
    {
	y2deb("constructing MdCo");

	for (auto const &it : systeminfo.getProcMdstat())
	    if (!it.second.is_container && !isHandledByMdPart(it.first))
		addMd(new Md(*this, it.first, "/dev/" + it.first, systeminfo));
    }


    MdCo::MdCo(const MdCo& c)
	: Container(c)
    {
	y2deb("copy-constructed MdCo from " << c.dev);

	for (auto const &i : c.mdPair())
	    vols.push_back(new Md(*this, i));
    }


    MdCo::~MdCo()
    {
	y2deb("destructed MdCo " << dev);
    }


    void
    MdCo::syncMdadm(EtcMdadm* mdadm) const
    {
	for (auto &it : mdPair(Md::notDeleted))
	    it.updateEntry(mdadm);
    }


void
MdCo::addMd( Md* m )
    {
    if( !findMd( m->nr() ))
	addToList( m );
    else
	{
	y2war("addMd already exists " << m->nr()); 
	delete m;
	}
    }


bool
MdCo::findMd( const string& dev, MdIter& i )
    {
    MdPair p=mdPair(Md::notDeleted);
    i=p.begin();
    while( i!=p.end() && i->device()!=dev )
	++i;
    return( i!=p.end() );
    }

bool
MdCo::findMd( unsigned num )
    {
    MdIter i;
    return( findMd( num, i ));
    }

bool
MdCo::findMd( unsigned num, MdIter& i )
    {
    return( findMd( Md::mdDevice(num), i ));
    }

bool
MdCo::findMd( const string& dev )
    {
    MdIter i;
    return( findMd( dev, i ));
    }


    list<unsigned>
    MdCo::usedNumbers() const
    {
	list<unsigned> nums;

	for (auto const &i : mdPair(Md::notDeleted))
	    nums.push_back(i.nr());

	return nums;
    }

int MdCo::getNameNum( const string& dev, string& nm, unsigned& num )
    {
    int ret=0;
    num = 0;
    nm.clear();
    if( boost::starts_with(dev, "/dev/md/"))
	nm = dev.substr(8);
    else if( !Md::mdStringNum( dev, num ))
	ret = STORAGE_MD_INVALID_NAME;
    else if( num>255 )
	ret = MD_NUMBER_TOO_LARGE;
    return( ret );
    }


int 
MdCo::createMd(const string& dev, MdType type, const list<string>& devs, const list<string>& spares)
    {
    int ret = 0;
    string nm;
    unsigned num;
    y2mil("dev:" << dev << " type:" << toString(type) << " devs:" << devs << " spares:" << spares);
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	ret = getNameNum( dev, nm, num );
	}
    if( ret==0 && type==RAID_UNK )
	{
	ret = MD_NO_CREATE_UNKNOWN;
	}
    if( ret==0 )
	{
	if( nm.empty() && findMd( num ))
	    ret = MD_DUPLICATE_NUMBER;
	else if( findMd( dev ))
	    ret = MD_DUPLICATE_NAME;
	}
    if (ret == 0)
	ret = checkUse(devs, spares);
    if( ret==0 )
	{
	if( nm.empty() )
	    nm = "md" + decString(num);
	Md* m = new Md(*this, nm, dev, type, devs, spares);
	m->setCreated( true );
	addToList( m );
	}
    y2mil("ret:" << ret);
    return( ret );
    }


    int
    MdCo::checkUse(const list<string>& devs, const list<string>& spares) const
    {
	int ret = 0;

	list<string> all = devs;
	all.insert(all.end(), spares.begin(), spares.end());

	for (auto const &it : all)
	{
	    const Volume* v = getStorage()->getVolume(it);
	    if (v == NULL)
	    {
		ret = MD_DEVICE_UNKNOWN;
		break;
	    }
	    else if (!v->canUseDevice())
	    {
		ret = MD_DEVICE_USED;
		break;
	    }
	}

	y2mil("devs:" << devs << " spares:" << spares << " ret:" << ret);
	return ret;
    }


int 
MdCo::checkMd( const string& dev )
    {
    int ret = 0;
    y2mil("dev:" << dev);
    MdIter i;
    if( !findMd( dev, i ) )
	ret = MD_DEVICE_UNKNOWN;
    else if( i->created() )
	ret = i->checkDevices();
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::extendMd(const string& dev, const list<string>& devs, const list<string>& spares)
    {
    int ret = 0;
    y2mil("dev:" << dev << " devs:" << devs << " spares:" << spares);
    MdIter i;
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	ret = checkUse(devs, spares);
	}
    if( ret==0 )
	{
	if( !findMd( dev, i ))
	    ret = MD_UNKNOWN_NUMBER;
	}
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_RESIZE_ON_DISK;
	}
    if( ret==0 )
	{
	for (auto const &it : devs)
	    if ((ret = i->addDevice(it)) != 0)
		break;
	}
    if( ret==0 )
	{
	for (auto const &it : spares)
	    if ((ret = i->addDevice(it, true)) != 0)
		break;
	}
    if( ret==0 && !getStorage()->isDisk(dev) )
	{
	getStorage()->changeFormatVolume( dev, false, FSNONE );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::updateMd(const string& dev, const list<string>& devs, const list<string>& spares)
    {
    int ret = 0;
    y2mil("dev:" << dev << " devs:" << devs << " spares:" << spares);
    MdIter i;
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findMd( dev, i ))
	    ret = MD_UNKNOWN_NUMBER;
	}
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_RESIZE_ON_DISK;
	}
    if( ret==0 )
	{
        list<string> ls = i->getDevs();
        for (auto const &it : ls)
	    if ((ret = i->removeDevice(it)) != 0)
		break;
        }
    if( ret==0 )
	{
        ret = checkUse(devs, spares);
        }
    if( ret==0 )
	{
	for (auto const &it : devs)
	    if ((ret = i->addDevice(it)) != 0)
		break;
	}
    if( ret==0 )
	{
	for (auto const &it : spares)
	    if ((ret = i->addDevice(it, true)) != 0)
		break;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::shrinkMd(const string& dev, const list<string>& devs, const list<string>& spares)
    {
    int ret = 0;
    y2mil("dev:" << dev << " devs:" << devs << " spares:" << spares);
    MdIter i;
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findMd( dev, i ))
	    ret = MD_UNKNOWN_NUMBER;
	}
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_RESIZE_ON_DISK;
	}
    if( ret==0 )
	{
	for (auto const &it : devs)
	    if ((ret = i->removeDevice(it)) != 0)
		break;
	}
    if( ret==0 )
	{
	for (auto const &it : spares)
	    if ((ret = i->removeDevice(it)) != 0)
		break;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::changeMdType( const string& dev, MdType ptype )
    {
    int ret = 0;
    y2mil("dev:" << dev << " md_type:" << toString(ptype));
    MdIter i;
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findMd( dev, i ))
	    ret = MD_UNKNOWN_NUMBER;
	}
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_CHANGE_ON_DISK;
	}
    if( ret==0 )
	{
	i->setPersonality( ptype );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::changeMdChunk( const string& dev, unsigned long chunk )
    {
    int ret = 0;
    y2mil("dev:" << dev << " chunk:" << chunk);
    MdIter i;
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findMd( dev, i ))
	    ret = MD_UNKNOWN_NUMBER;
	}
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_CHANGE_ON_DISK;
	}
    if( ret==0 )
	{
	i->setChunkSizeK(chunk);
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
MdCo::changeMdParity( const string& dev, MdParity ptype )
    {
    int ret = 0;
    y2mil("dev:" << dev << " parity:" << toString(ptype));
    MdIter i;
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findMd( dev, i ))
	    ret = MD_UNKNOWN_NUMBER;
	}
    if( ret==0 && !i->created() )
	{
	ret = MD_NO_RESIZE_ON_DISK;
	}
    if( ret==0 )
	{
	ret = i->setParity( ptype );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int
MdCo::getMdState(const string& dev, MdStateInfo& info)
{
    int ret = 0;
    MdIter i;
    if( ret==0 )
    {
	if( !findMd( dev, i ))
	    ret = MD_UNKNOWN_NUMBER;
    }
    if( ret==0 && i->created() )
    {
	ret = MD_STATE_NOT_ON_DISK;
    }
    if( ret==0 )
    {
	ret = i->getState(info);
    }
    y2mil("ret:" << ret);
    return ret;
}

int 
MdCo::removeMd( const string& dev, bool destroySb )
    {
    int ret = 0;
    y2mil("dev:" << dev);
    MdIter i;
    if( readonly() )
	{
	ret = MD_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	if( !findMd( dev, i ))
	    ret = MD_UNKNOWN_NUMBER;
	}
    if (ret == 0 && i->isUsedBy())
	{
	ret = MD_REMOVE_USED_BY;
	}
    if( ret==0 )
	{
	getStorage()->clearUsedBy(i->getDevs());

	if( i->created() )
	    {
	    if( !removeFromList( &(*i) ))
		ret = MD_REMOVE_CREATE_NOT_FOUND;
	    }
	else
	    {
	    i->setDeleted();
	    i->setDestroySb( destroySb );
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int MdCo::removeVolume( Volume* v )
    {
    int ret = 0;
    y2mil("name:" << v->name());
    ret = removeMd( v->device() );
    return( ret );
    }

void MdCo::activate( bool val, const string& tmpDir )
    {
	if (getenv("LIBSTORAGE_NO_MDRAID") != NULL)
	    return;

    y2mil("old active:" << active << " val:" << val << " tmp:" << tmpDir);
    if( active!=val )
	{
	SystemCmd c;
	if( val )
	    {
	    string mdconf = tmpDir + "/mdadm.conf";
	    c.execute("echo 1 > /sys/module/md_mod/parameters/start_ro");
	    c.execute(MDADMBIN " --examine --scan --config=partitions > " + mdconf);
	    AsciiFile(mdconf).logContent();
	    c.execute(MDADMBIN " --assemble --scan --config=" + mdconf);
	    unlink(mdconf.c_str());
	    }
	else
	    {
	    c.execute(MDADMBIN " --stop --scan");
	    }
	active = val;
	}
    Storage::waitForDevice();
    }

int 
MdCo::doCreate( Volume* v ) 
    {
    y2mil("name:" << v->name());
    Md * m = dynamic_cast<Md *>(v);
    int ret = 0;
    if( m != NULL )
	{
	getStorage()->showInfoCb( m->createText(true), silent );
	ret =  m->checkDevices();
	if( ret==0 )
	    {
	    list<string> devs = m->getDevs();
	    for (auto const &i : devs)
		{
		getStorage()->removeDmTableTo(i);
		getStorage()->unaccessDev(i);
		}
	    }
	if( ret==0 )
	    {
	    string cmd = m->createCmd();
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		{
		ret = MD_CREATE_FAILED;
		setExtError( c );
		}
	    }
	if( ret==0 )
	    {
	    m->setCreated(false);
	    Storage::waitForDevice(m->device());
	    SystemInfo systeminfo;
	    m->updateData(systeminfo);
	    m->setUdevData(systeminfo);
	    EtcMdadm* mdadm = getStorage()->getMdadm();
	    if (mdadm)
		m->updateEntry(mdadm);
	    bool used_as_pv = m->isUsedBy(UB_LVM);
	    y2mil("zeroNew:" << getStorage()->getZeroNewPartitions() << " used_as_pv:" << used_as_pv);
	    if( used_as_pv || getStorage()->getZeroNewPartitions() )
		{
		ret = Storage::zeroDevice(m->device());
		}
	    }
	}
    else
	ret = MD_CREATE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }

int 
MdCo::doRemove( Volume* v )
    {
    y2mil("name:" << v->name());
    Md * m = dynamic_cast<Md *>(v);
    int ret = 0;
    if( m != NULL )
	{
	if( !active )
	    MdPartCo::activate(true, getStorage()->tmpDir());
	getStorage()->showInfoCb( m->removeText(true), silent );
	ret = m->prepareRemove();
	if( ret==0 )
	    {
	    string cmd = MDADMBIN " --stop " + quote(m->device());
	    SystemCmd c( cmd );
	    if( c.retcode()!=0 )
		{
		ret = MD_REMOVE_FAILED;
		setExtError( c );
		}
	    }
	if( ret==0 && m->destroySb() )
	    {
	    SystemCmd c;
	    for (auto const &i : m->getDevs())
		c.execute(MDADMBIN " --zero-superblock " + quote(i));
	    }
	if( ret==0 )
	    {
	    EtcMdadm* mdadm = getStorage()->getMdadm();
	    if (mdadm)
		mdadm->removeEntry(m->getMdUuid());
	    if( !removeFromList( m ) )
		ret = MD_NOT_IN_LIST;
	    }
	}
    else
	ret = MD_REMOVE_INVALID_VOLUME;
    y2mil("ret:" << ret);
    return( ret );
    }

void MdCo::changeDeviceName( const string& old, const string& nw )
    {
    for (auto &i : mdPair())
        i.changeDeviceName(old, nw);
    }


std::ostream& operator<< (std::ostream& s, const MdCo& d )
    {
    s << dynamic_cast<const Container&>(d);
    return( s );
    }


    void
    MdCo::logDifferenceWithVolumes(std::ostream& log, const Container& rhs_c) const
    {
	const MdCo& rhs = dynamic_cast<const MdCo&>(rhs_c);

	logDifference(log, rhs);
	log << endl;

	ConstMdPair pp = mdPair();
	ConstMdPair pc = rhs.mdPair();
	logVolumesDifference(log, pp.begin(), pp.end(), pc.begin(), pc.end());
    }


bool MdCo::equalContent( const Container& rhs ) const
    {
    const MdCo * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const MdCo*>(&rhs);
    if( ret && p )
	{
	ConstMdPair pp = mdPair();
	ConstMdPair pc = p->mdPair();
	ret = ret && storage::equalContent(pp.begin(), pp.end(), pc.begin(), pc.end());
	}
    return( ret );
    }


bool MdCo::isHandledByMdPart(const string& name) const
{
    for (auto const &i : getStorage()->mdpartCoPair(MdPartCo::notDeleted))
        if (i.name() == name)
            return true;
    return false;
}


bool MdCo::active = false;

}
