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


#include <stdio.h>
#include <string>
#include <ostream>
#include <fstream>
#include <iomanip>
#include <boost/algorithm/string.hpp>

#include "storage/SystemCmd.h"
#include "storage/ProcParts.h"
#include "storage/Storage.h"
#include "storage/OutputProcessor.h"
#include "storage/Dasd.h"
#include "storage/StorageDefines.h"
#include "storage/SystemInfo.h"
#include "storage/Dasdview.h"


namespace storage
{
    using namespace std;


    Dasd::Dasd(Storage* s, const string& name, const string& device, unsigned long long SizeK,
	       SystemInfo& systeminfo)
	: Disk(s, name, device, SizeK, systeminfo), fmt(DASDF_NONE)
    {
	y2deb("constructed Dasd " << dev);
    }


    Dasd::Dasd(const Dasd& c)
	: Disk(c), fmt(c.fmt)
    {
	y2deb("copy-constructed Dasd from " << c.dev);
    }


    Dasd::~Dasd()
    {
	y2deb("destructed Dasd " << dev);
    }


    bool
    Dasd::detectPartitionsFdasd(SystemInfo& systeminfo)
    {
    bool ret = true;
	checkFdasdOutput(systeminfo);
    y2mil("ret:" << ret << " partitions:" << vols.size());
    return( ret );
    }


    bool
    Dasd::detectPartitions(SystemInfo& systeminfo)
    {
    bool ret = true;

    detected_label = "dasd";
    setLabelData( "dasd" );

	Dasdview dasdview(device());
	new_geometry = geometry = dasdview.getGeometry();
	fmt = dasdview.getDasdFormat();
	ronly = fmt != DASDF_CDL;

	if( size_k==0 )
	    {
	    size_k = geometry.sizeK();
	    y2mil("New SizeK:" << size_k);
	    }

	switch( fmt )
	    {
	    case DASDF_CDL:
		ret = Dasd::detectPartitionsFdasd(systeminfo);
		break;
	    case DASDF_LDL:
		{
		max_primary = 1;
		unsigned long long s = cylinderToKb(cylinders());
		Partition *p = new Partition(*this, getPartName(1), getPartDevice(1), 1,
					     systeminfo, s, Region(0, cylinders()), PRIMARY);
		const ProcParts& parts = systeminfo.getProcParts();
		if (parts.getSize(p->device(), s))
		    {
		    p->setSize( s );
		    }
		addToList( p );
		ret = true;
		}
		break;
	    default:
		break;
	    }

    y2mil("ret:" << ret << " partitions:" << vols.size() << " detected label:" << label);
    y2mil("geometry:" << geometry << " fmt:" << toString(fmt) << " readonly:" << ronly);
    return ret;
    }


    bool
    Dasd::checkPartitionsValid(SystemInfo& systeminfo, const list<Partition*>& pl) const
    {
	const ProcParts& parts = systeminfo.getProcParts();
	const Fdasd& fdasd = systeminfo.getFdasd(dev);

	list<string> ps = partitionsKernelKnowns(parts);
	if (pl.size() != ps.size())
	{
	    y2err("number of partitions fdasd and kernel see differs");
	    return false;
	}

	for (list<Partition*>::const_iterator i = pl.begin(); i != pl.end(); ++i)
	{
	    const Partition& p = **i;

	    Fdasd::Entry entry;
	    if (fdasd.getEntry(p.nr(), entry))
	    {
		// maybe too strict but should be ok

		Region head_fdasd = entry.headRegion;
		Region head_kernel = p.detectSysfsBlkRegion() / (geometry.headSize() / 512);

		if (head_fdasd != head_kernel)
		{
		    y2err("region mismatch dev:" << dev << " nr:" << p.nr() << " head_fdasd:" <<
			  head_fdasd << " head_kernel:" << head_kernel);
		    return false;
		}
	    }
	}

	return true;
    }


bool
    Dasd::checkFdasdOutput(SystemInfo& systeminfo)
    {
	const ProcParts& parts = systeminfo.getProcParts();
	const Fdasd& fdasd = systeminfo.getFdasd(dev);

	assert(geometry == fdasd.getGeometry());

    list<Partition *> pl;

	for (Fdasd::const_iterator it = fdasd.getEntries().begin();
	     it != fdasd.getEntries().end(); ++it)
	{
	    if( it->num < range )
		    {
		    unsigned long long s = cylinderToKb(it->cylRegion.len());
		    Partition *p = new Partition(*this, getPartName(it->num), getPartDevice(it->num),
						 it->num, systeminfo, s, it->cylRegion, PRIMARY);
		    if (parts.getSize(p->device(), s))
			{
			p->setSize( s );
			}
		    pl.push_back( p );
		    }
		else
		    y2war("partition nr " << it->num << " outside range " << range);
	}

    y2mil("nm:" << nm);
    unsigned long dummy = 0;
    if (!checkPartedValid(systeminfo, pl, dummy))
	{
	Text txt = sformat(
	// popup text %1$s is replaced by disk name e.g. /dev/hda
_("The partitioning on disk %1$s is not readable by\n"
"the partitioning tool fdasd, which is used to change the\n"
"partition table.\n"
"\n"
"You can use the partitions on disk %1$s as they are.\n"
"You can format them and assign mount points to them, but you\n"
"cannot add, edit, resize, or remove partitions from that\n"
"disk with this tool."), dev.c_str() );

	getStorage()->addInfoPopupText( dev, txt );
	ronly = true;
	}
    for( list<Partition*>::iterator i=pl.begin(); i!=pl.end(); ++i )
	{
	addToList( *i );
	}
    return( true );
    }


int Dasd::doResize( Volume* v ) 
    { 
    return DASD_NOT_POSSIBLE;
    }

int Dasd::resizePartition( Partition* p, unsigned long newCyl )
    { 
    return DASD_NOT_POSSIBLE;
    }

int Dasd::removePartition( unsigned nr )
    {
    y2mil("begin nr:" << nr);
    int ret = Disk::removePartition( nr );
    if( ret==0 )
	{
	PartPair p = partPair(Partition::notDeleted);
	changeNumbers( p.begin(), p.end(), nr, -1 );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Dasd::createPartition( PartitionType type, unsigned long start,
                           unsigned long len, string& device,
			   bool checkRelaxed )
    {
    y2mil("begin type:" << toString(type) << " start:" << start << " len:" << len << " relaxed:"
	  << checkRelaxed);
    int ret = createChecks(type, Region(start, len), checkRelaxed);
    int number = 0;
    if( ret==0 )
	{
	number = availablePartNumber( type );
	if( number==0 )
	    {
	    ret = DISK_PARTITION_NO_FREE_NUMBER;
	    }
	else
	    {
	    PartPair p = partPair(Partition::notDeleted);
	    number = 1;
	    PartIter i = p.begin();
	    while( i!=p.end() && i->cylStart()<start )
		{
		number++;
		++i;
		}
	    y2mil("number:" << number);
	    changeNumbers( p.begin(), p.end(), number-1, 1 );
	    }
	}
    if( ret==0 )
	{
	Partition * p = new Partition(*this, getPartName(number), getPartDevice(number), number,
				      cylinderToKb(len), Region(start, len), type);
	p->setCreated();
	device = p->device();
	addToList( p );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

Text Dasd::fdasdText() const
    {
    Text txt;
    // displayed text during action, %1$s is replaced by disk name (e.g. /dev/dasda),
    txt = sformat( _("Executing fdasd for disk %1$s"), dev.c_str() );
    return( txt );
    }

int Dasd::doFdasd()
    {
    int ret = 0;
    if( !silent ) 
	{
	getStorage()->showInfoCb( fdasdText() );
	}
    string inpname = getStorage()->tmpDir()+"/fdasd_inp";
    ofstream inpfile( inpname.c_str() );
    classic(inpfile);
    PartPair p = partPair(Partition::notDeleted);
    PartIter i = p.begin();
    while( i!=p.end() )
	{
	string start = decString(i->cylStart() * new_geometry.heads);
	string end  = decString((i->cylEnd() + 1) * new_geometry.heads - 1);
	if( i->cylStart()==0 )
	    start = "first";
	if( i->cylEnd()>=cylinders()-1 )
	    end = "last";
	inpfile << "[" << start << "," << end << "]" << endl;
	++i;
	}
    inpfile.close();
    SystemCmd cmd( "cat " + inpname );
    string cmd_line = FDASDBIN " -c " + inpname + " " + quote(device());
    if( execCheckFailed( cmd_line ) )
	{
	ret = DASD_FDASD_FAILED;
	}
    if( ret==0 )
	{
	ProcParts parts;
	p = partPair();
	i = p.begin();
	list<Partition*> rem_list;
	while( i!=p.end() )
	    {
	    if( i->deleted() )
		{
		rem_list.push_back( &(*i) );
		i->prepareRemove();
		}
	    if( i->created() )
		{
		unsigned long long s;
		Storage::waitForDevice(i->device());
		i->setCreated( false );
		if (parts.getSize(i->device(), s))
		    {
		    i->setSize( s );
		    }
		ret = i->zeroIfNeeded(); 
		}
	    ++i;
	    }
	list<Partition*>::const_iterator pr = rem_list.begin();
	while( pr != rem_list.end() )
	    {
	    if( !removeFromList( *pr ) && ret==0 )
		ret = DISK_REMOVE_PARTITION_LIST_ERASE;
	    ++pr;
	    }
	}
    unlink( inpname.c_str() );
    y2mil("ret:" << ret);
    return( ret );
    }


void
Dasd::getCommitActions(list<commitAction>& l) const
    {
    y2mil("begin:" << name() << " init_disk:" << init_disk);
    Disk::getCommitActions( l );
    if( init_disk )
	{
	l.remove_if(stage_is(DECREASE));
	l.push_front(commitAction(DECREASE, staticType(), dasdfmtText(false), this, true));
	}
    }


    Text
    Dasd::dasdfmtTexts(bool doing, const list<string>& devs)
    {
	Text txt;
	if (doing)
	{
	    // displayed text during action, %1$s is replaced by disk name (e.g. dasda)
	    txt = sformat(_("Executing dasdfmt for disk %1$s",
			    "Executing dasdfmt for disks %1$s", devs.size()),
			  boost::join(devs, " ").c_str());
	}
	else
	{
	    // displayed text during action, %1$s is replaced by disk name (e.g. dasda)
	    txt = sformat(_("Execute dasdfmt on disk %1$s",
			    "Execute dasdfmt on disks %1$s", devs.size()),
			  boost::join(devs, " ").c_str());
	}
	return txt;
    }


    Text
    Dasd::dasdfmtText(bool doing) const
    {
	list<string> tmp;
	tmp.push_back(dev);
	return dasdfmtTexts(doing, tmp);
    }


void
Dasd::getToCommit(CommitStage stage, list<const Container*>& col, list<const Volume*>& vol) const
{
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    if( stage==DECREASE ) 
	{
	ConstVolPair p = volPair( stageDecrease );
	if( !p.empty() )
	    vol.push_back( &(*(p.begin())) );
	if( deleted() || init_disk )
	    col.push_back( this );
	}
    else if( stage==INCREASE )
	{
	ConstVolPair p = volPair(stageIncrease);
	if( !p.empty() )
	    vol.push_back( &(*(p.begin())) );
	}
    else
	Disk::getToCommit( stage, col, vol );
    if( col.size()!=oco || vol.size()!=ovo )
	y2mil("stage:" << stage << " col:" << col.size() << " vol:" << vol.size());
}


int Dasd::commitChanges( CommitStage stage )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = 0;
    if( stage==DECREASE && init_disk )
	{
	ret = doDasdfmt();
	}
    if( ret==0 )
	{
	ret = Disk::commitChanges( stage );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

static bool needDasdfmt( const Disk&d )
    { 
    bool ret = d.isDasd() && d.initializeDisk();
    return( ret );
    }

int Dasd::doDasdfmt()
    {
    int ret = 0;
    y2mil("dasd:" << device());
    list<Disk*> dl;
    list<string> devs;
    getStorage()->getDiskList( needDasdfmt, dl );
    if( !dl.empty() )
	{
	for( list<Disk*>::const_iterator i = dl.begin(); i!=dl.end(); ++i )
	    {
	    devs.push_back( undevDevice((*i)->device()) );
	    }
	y2mil("devs:" << devs);
	if( !silent ) 
	    {
	    Text txt = dasdfmtTexts(true, devs);
	    getStorage()->showInfoCb( txt );
	    }
	for( list<string>::iterator i = devs.begin(); i!=devs.end(); ++i )
	    {
	    *i = "-f " + quote(normalizeDevice(*i));
	    }
	string cmd_line = DASDFMTBIN " -Y -P 4 -b 4096 -y -m 1 -d cdl " + boost::join(devs, " ");
	y2mil("cmdline:" << cmd_line);
	CallbackProgressBar cb = getStorage()->getCallbackProgressBarTheOne();
	ProgressBar* progressbar = new DasdfmtProgressBar( cb );
	SystemCmd cmd;
	cmd.setOutputProcessor(progressbar);
	if( execCheckFailed( cmd, cmd_line ) )
	    {
	    ret = DASD_DASDFMT_FAILED;
	    }
	if( ret==0 )
	    {
	    SystemInfo systeminfo;
	    for( list<Disk*>::iterator i = dl.begin(); i!=dl.end(); ++i )
		{
		Dasd * ds = static_cast<Dasd *>(*i);
		ds->detectPartitions(systeminfo);
		ds->resetInitDisk();
		ds->removeFromMemory();
		}
	    }
	delete progressbar;
	}
    return( ret );
    }

int Dasd::initializeDisk( bool value )
    {
    y2mil("value:" << value << " old:" << init_disk);
    int ret = 0;
    if( init_disk != value )
	{
	init_disk = value;
	if( init_disk )
	    {
	    new_geometry.heads = geometry.heads = 15;
	    new_geometry.sectors = geometry.sectors = 12;
	    y2mil("new geometry:" << geometry);
	    size_k = geometry.sizeK();
	    y2mil("new SizeK:" << size_k);
	    ret = destroyPartitionTable( "dasd" );
	    }
	else
	    {
	    PartPair p = partPair();
	    PartIter i = p.begin();
	    list<Partition*> rem_list;
	    while( i!=p.end() )
		{
		if( i->deleted() )
		    {
		    i->setDeleted(false);
		    }
		if( i->created() )
		    {
		    rem_list.push_back( &(*i) );
		    }
		++i;
		}
	    list<Partition*>::const_iterator pr = rem_list.begin();
	    while( pr != rem_list.end() )
		{
		if( !removeFromList( *pr ) && ret==0 )
		    ret = DISK_REMOVE_PARTITION_LIST_ERASE;
		++pr;
		}
	    }
	}
    return( ret );
    }


std::ostream& operator<< (std::ostream& s, const Dasd& d )
    {
    s << dynamic_cast<const Disk&>(d);
    s << " fmt:" << toString(d.fmt);
    return s;
    }


    static const string dasd_format_names[] = {
	"NONE", "LDL", "CDL"
    };

    const vector<string> EnumInfo<Dasd::DasdFormat>::names(dasd_format_names, dasd_format_names +
							   lengthof(dasd_format_names));


}
