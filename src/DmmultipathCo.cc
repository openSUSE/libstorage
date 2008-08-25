/*
  Textdomain    "storage"
*/

#include <iostream>
#include <sstream>
#include <locale>
#include <boost/algorithm/string.hpp>

#include "y2storage/DmmultipathCo.h"
#include "y2storage/Dmmultipath.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"
#include "y2storage/StorageDefines.h"

using namespace std;
using namespace storage;


DmmultipathCo::DmmultipathCo( Storage * const s, const string& Name, ProcPart& ppart ) :
    DmPartCo(s, "/dev/mapper/"+Name, staticType(), ppart )
{
    DmPartCo::init( ppart );
    getMultipathData( Name );
    y2deb("constructing dmmultipath co " << Name);
}


DmmultipathCo::~DmmultipathCo()
{
    y2deb("destructed multipath co " << dev);
}


void
DmmultipathCo::getMultipathData(const string& name)
{
    y2mil("name:" << name);
    SystemCmd c(MULTIPATHBIN " -d -v 2+ -ll " + quote(name));

    if (c.numLines() > 0)
    {
	string line = *c.getLine(0);
	y2mil("line:" << line);
	list<string> tmp = splitString(extractNthWord(1, line, true), ",");
	if (tmp.size() >= 2)
	{
	    list<string>::const_iterator it = tmp.begin();
	    vendor = boost::trim_copy(*it++, locale::classic());
	    model = boost::trim_copy(*it++, locale::classic());
	}
	else
	{
	    vendor = model = "";
	}
	y2mil("vendor:" << vendor << " model:" << model);

	list<string> devs;
	for (unsigned int i = 1; i < c.numLines(); i++)
	{
	    string line = *c.getLine(i);
	    if (boost::starts_with(line, " \\_"))
	    {
		y2mil("mp element:" << line);
		string dev = getStorage()->deviceByNumber(extractNthWord(3,line));
		if (find(devs.begin(), devs.end(), dev) == devs.end())
		    devs.push_back(dev);
	    }
	}

	y2mil("devs:" << devs);

	Pv *pv = new Pv;
	for (list<string>::const_iterator it = devs.begin(); it != devs.end(); it++)
	{
	    pv->device = *it;
	    addPv(pv);
	}
	delete pv;
    }
}


void
DmmultipathCo::setUdevData(const list<string>& id)
{
    y2mil("disk:" << nm << " id:" << id);
    udev_id = id;
    udev_id.erase( remove_if(udev_id.begin(), udev_id.end(),
                             find_begin("dm-uuid-mpath")));
    udev_id.sort();
    y2mil( "id:" << udev_id );

    DmmultipathPair pp = dmmultipathPair();
    for( DmmultipathIter p=pp.begin(); p!=pp.end(); ++p )
	{
	p->addUdevData();
	}
}


void
DmmultipathCo::newP( DmPart*& dm, unsigned num, Partition* p )
    {
    y2mil( "num:" << num );
    dm = new Dmmultipath( *this, num, p );
    }


void
DmmultipathCo::addPv(Pv*& p)
{
    PeContainer::addPv(p);
    if (!deleted())
	getStorage()->setUsedBy(p->device, UB_DMMULTIPATH, name());
    p = new Pv;
}


void
DmmultipathCo::getMultipaths( list<string>& l )
{
    l.clear();

    string line;
    unsigned i=0;
    SystemCmd c(MULTIPATHBIN " -d -v 2+ -ll");
    if( i<c.numLines() )
	line = *c.getLine(i);
    while( i<c.numLines() )
    {
	while( i<c.numLines() && (line.empty() || !isalnum(line[0])))
	    if( ++i<c.numLines() )
		line = *c.getLine(i);

	y2mil("mp unit:" << line);
	string unit = extractNthWord(0, line);
	string::size_type pos = unit.rfind("dm-");
	if (pos != string::npos)
	    unit.erase(pos);
	y2mil("mp name:" << unit);

	list<string> mp_list;

	if( ++i<c.numLines() )
	    line = *c.getLine(i);
	while( i<c.numLines() && (line.empty() || !isalnum(line[0])))
	{
	    if (boost::starts_with(line, " \\_"))
	    {
		y2mil( "mp element:" << line );
		string dev = extractNthWord(3,line);
		if( find( mp_list.begin(), mp_list.end(), dev )== mp_list.end() )
		    mp_list.push_back(dev);
	    }
	    if( ++i<c.numLines() )
		line = *c.getLine(i);
	}
	y2mil( "mp_list:" << mp_list );
	if (mp_list.size() >= 1)
	{
	    l.push_back(unit);
	}
    }

    y2mil("ret:" << l);
}


string DmmultipathCo::setDiskLabelText( bool doing ) const
    {
    string txt;
    string d = nm;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by multipath name (e.g. pdc_igeeeadj),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Setting disk label of multipath disk %1$s to %2$s"),
		       d.c_str(), labelName().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by multipath name (e.g. pdc_igeeeadj),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Set disk label of multipath disk %1$s to %2$s"),
		      d.c_str(), labelName().c_str() );
        }
    return( txt );
    }


void DmmultipathCo::getInfo( DmmultipathCoInfo& tinfo ) const
    {
    DmPartCo::getInfo( info );
    tinfo.p = info;
    tinfo.vendor = vendor;
    tinfo.model = model;
    }

namespace storage
{

std::ostream& operator<<(std::ostream& s, const DmmultipathCo& d)
{
    s << *((DmPartCo*)&d);
    s << " Vendor:" << d.vendor
      << " Model:" << d.model;
    return s;
}

}

string DmmultipathCo::getDiffString( const Container& d ) const
{
    string log = DmPartCo::getDiffString(d);
    const DmmultipathCo * p = dynamic_cast<const DmmultipathCo*>(&d);
    if (p)
    {
	if (vendor != p->vendor)
	    log += " vendor:" + vendor + "-->" + p->vendor;
	if (model != p->model)
	    log += " model:" + model + "-->" + p->model;
    }
    return log;
}

bool DmmultipathCo::equalContent( const Container& rhs ) const
{
    bool ret = Container::equalContent(rhs);
    if (ret)
    {
	const DmmultipathCo *p = dynamic_cast<const DmmultipathCo*>(&rhs);
	ret = p && DmPartCo::equalContent(*p) &&
	    vendor == p->vendor && model == p->model;
    }
    return ret;
}

DmmultipathCo::DmmultipathCo( const DmmultipathCo& rhs ) : DmPartCo(rhs)
{
    vendor = rhs.vendor;
    model = rhs.model;
}

void DmmultipathCo::logData( const string& Dir ) {}
