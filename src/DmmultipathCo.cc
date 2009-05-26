/*
  Textdomain    "storage"
*/

#include <ostream>
#include <sstream>
#include <locale>
#include <boost/algorithm/string.hpp>

#include "storage/DmmultipathCo.h"
#include "storage/Dmmultipath.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


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
	string line = c.getLine(0);
	y2mil("mp line:" << line);

	string unit = extractNthWord(0, line);
	y2mil("mp name:" << unit);

	list<string> tmp = splitString(extractNthWord(2, line, true), ",");
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
	    string line = c.getLine(i);
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
    udev_id.remove_if(string_starts_with("dm-"));
    y2mil("id:" << udev_id);

    DmPartCo::setUdevData(udev_id);

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
    PeContainer::addPv(*p);
    if (!deleted())
	getStorage()->setUsedBy(p->device, UB_DMMULTIPATH, name());
    delete p;
    p = new Pv;
}


void
DmmultipathCo::activate(bool val)
{
    y2mil("old active:" << active << " val:" << val);

    if (active != val)
    {
	bool lvm_active = LvmVg::isActive();
	LvmVg::activate(false);

	SystemCmd c;
	if (val)
	{
	    Dm::activate(true);

	    c.execute(MODPROBEBIN " dm-multipath");
	    c.execute(MULTIPATHBIN);
	    sleep(1);
	    c.execute(MULTIPATHDBIN);
	}
	else
	{
	    c.execute(MULTIPATHDBIN " -F");
	    sleep(1);
	    c.execute(MULTIPATHBIN " -F");
	}

	LvmVg::activate(lvm_active);

	active = val;
    }
}


void
DmmultipathCo::getMultipaths(list<string>& l)
{
    l.clear();

    SystemCmd c(MULTIPATHBIN " -d -v 2+ -ll");
    if (c.numLines() > 0)
    {
	string line;
	unsigned i=0;

	if( i<c.numLines() )
	    line = c.getLine(i);
	while( i<c.numLines() )
	{
	    while( i<c.numLines() && (line.empty() || !isalnum(line[0])))
		if( ++i<c.numLines() )
		    line = c.getLine(i);

	    y2mil("mp line:" << line);

	    string unit = extractNthWord(0, line);
	    y2mil("mp name:" << unit);

	    list<string> mp_list;

	    if( ++i<c.numLines() )
		line = c.getLine(i);
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
		    line = c.getLine(i);
	    }
	    y2mil( "mp_list:" << mp_list );
	    if (mp_list.size() >= 1)
	    {
		l.push_back(unit);
	    }
	}

	if (!l.empty())
	    active = true;
    }

    y2mil("detected multipaths " << l);
}


string DmmultipathCo::setDiskLabelText( bool doing ) const
    {
    string txt;
    string d = nm;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by multipath name (e.g. 3600508b400105f590000900000300000),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Setting disk label of multipath disk %1$s to %2$s"),
		       d.c_str(), labelName().c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by multipath name (e.g. 3600508b400105f590000900000300000),
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


std::ostream& operator<<(std::ostream& s, const DmmultipathCo& d)
{
    s << *((DmPartCo*)&d);
    s << " Vendor:" << d.vendor
      << " Model:" << d.model;
    return s;
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


void
DmmultipathCo::logData(const string& Dir) const
{
}


bool DmmultipathCo::active = false;

}
