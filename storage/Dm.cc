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


#include <sstream>

#include "storage/Dm.h"
#include "storage/PeContainer.h"
#include "storage/SystemCmd.h"
#include "storage/AppUtil.h"
#include "storage/Regex.h"
#include "storage/Storage.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


    // this ctr is used for volumes of LvmVg and DmPart
    Dm::Dm(const PeContainer& c, const string& name, const string& device, const string& tname)
	: Volume(c, name, device), tname(tname), num_le(0), stripe(1), stripe_size(0),
	  inactiv(true)
    {
	y2mil("constructed Dm tname:" << tname);
    }


    // this ctr is used for volumes of DmCo
    Dm::Dm(const PeContainer& c, const string& name, const string& device, const string& tname,
	   SystemInfo& systeminfo)
	: Volume(c, name, device, systeminfo), tname(tname), num_le(0), stripe(1), stripe_size(0),
	  inactiv(true)
    {
	y2mil("constructed Dm dev:" << dev << " tname:" << tname);

	string dmn = "/dev/mapper/" + tname;
	if (dmn != dev)
	    alt_names.push_back(dmn);

	updateMajorMinor();

    getTableInfo();

    getStorage()->fetchDanglingUsedBy(dev, uby);
    for (list<string>::const_iterator it = alt_names.begin(); it != alt_names.end(); ++it)
	getStorage()->fetchDanglingUsedBy(*it, uby);
    }


    Dm::Dm(const PeContainer& c, const xmlNode* node)
	: Volume(c, node), tname(), num_le(0), stripe(1), stripe_size(0), inactiv(true)
    {
	getChildValue(node, "table_name", tname);

	getChildValue(node, "stripes", stripe);
	getChildValue(node, "stripe_size_k", stripe_size);

	y2deb("constructed Dm " << dev);

	assert(!tname.empty());
	assert(stripe == 1 || stripe_size > 0);
    }


    Dm::Dm(const PeContainer& c, const Dm& v)
	: Volume(c, v), tname(v.tname), num_le(v.num_le), stripe(v.stripe),
	  stripe_size(v.stripe_size), inactiv(v.inactiv), pe_map(v.pe_map)
    {
	y2deb("copy-constructed Dm " << dev);
    }


    Dm::~Dm()
    {
	y2deb("destructed Dm dev " << dev);
    }


    void
    Dm::saveData(xmlNode* node) const
    {
	Volume::saveData(node);

	setChildValue(node, "table_name", tname);

	setChildValue(node, "stripes", stripe);
	if (stripe > 1)
	    setChildValue(node, "stripe_size_k", stripe_size);
    }


unsigned Dm::dmMajor()
    {
    if( dm_major==0 )
    {
	dm_major = getMajorDevices("device-mapper");
	y2mil("dm_major:" << dm_major);
    }
    return( dm_major );
    }


void
Dm::getTableInfo()
    {
    SystemCmd c(DMSETUPBIN " table " + quote(tname));
    inactiv = c.retcode()!=0;
    y2mil("table:" << tname << " retcode:" << c.retcode() << " numLines:" << c.numLines() <<
	  " inactive:" << inactiv);
    if( c.numLines()>0 )
	{
	string line = c.getLine(0);
	target = extractNthWord( 2, line );
	if( target=="striped" )
	    extractNthWord( 3, line ) >> stripe;
	}
    pe_map.clear();
    map<string,unsigned long>::iterator mit;
    unsigned long long pesize = pec()->peSize();
    y2mil("pesize:" << pesize );
    for( unsigned i=0; i<c.numLines(); i++ )
	{
	unsigned long le;
	string dev;
	string majmin;
	string line = c.getLine(i);
	if( target=="linear" )
	    {
	    extractNthWord( 1, line ) >> le;
	    le /= 2;
	    le += pesize-1;
	    le /= pesize;
	    majmin = extractNthWord( 3, line );
	    dev = pec()->getDeviceByNumber( majmin );
	    if( !dev.empty() )
		{
		if( (mit=pe_map.find( dev ))==pe_map.end() )
		    pe_map[dev] = le;
		else
		    mit->second += le;
		}
	    else
		y2war("could not find major/minor pair " << majmin);
	    }
	else if (target == "snapshot-origin")
	{
	    // AFAIS snapshot-origins do not really use space.
	}
	else if (target == "snapshot")
	{
	    // AFAIS think snapshots do not really use space.
	}
	else if( target=="striped" )
	    {
	    unsigned str;
	    extractNthWord( 1, line ) >> le;
	    le /= 2;
	    le += pesize-1;
	    le /= pesize;
	    extractNthWord( 4, line ) >> stripe_size;
	    stripe_size /= 2;
	    extractNthWord( 3, line ) >> str;
	    if( str<2 )
		y2war("invalid stripe count " << str);
	    else
		{
		le = (le+str-1)/str;
		for( unsigned j=0; j<str; j++ )
		    {
		    majmin = extractNthWord( 5+j*2, line );
		    dev = pec()->getDeviceByNumber( majmin );
		    if( !dev.empty() )
			{
			if( (mit=pe_map.find( dev ))==pe_map.end() )
			    pe_map[dev] = le;
			else
			    mit->second += le;
			}
		    else
			y2war("could not find major/minor pair " << majmin);
		    }
		}
	    }
	else 
	    {
	    if( find( known_types.begin(), known_types.end(), target ) == 
	        known_types.end() )
		y2war("unknown target type \"" << target << "\"");
	    extractNthWord( 1, line ) >> le;
	    y2mil( "le:" << le );
	    le /= 2;
	    y2mil( "le:" << le );
	    if( pesize>0 )
		{
		le += pesize-1;
		y2mil( "le:" << le );
		le /= pesize;
		y2mil( "le:" << le );
		}
	    list<string> sl = splitString( extractNthWord( 2, line, true ));
	    y2mil( "sl:" << sl );
	    Regex devspec( "^[0123456789]+:[0123456789]+$" );
	    for( list<string>::const_iterator i=sl.begin(); i!=sl.end(); ++i )
		{
		if( devspec.match( *i ))
		    {
		    y2mil( "match \"" << *i << "\"" );
		    dev = pec()->getDeviceByNumber( *i );
		    if( !dev.empty() )
			{
			if( (mit=pe_map.find( dev ))==pe_map.end() )
			    pe_map[dev] = le;
			else
			    mit->second += le;
			}
		    else
			y2war("could not find major/minor pair " << majmin);
		    }
		}
	    }
	}
    }

bool Dm::removeTable()
    {
    return getStorage()->removeDmTable(tname);
    }

unsigned long long
Dm::usingPe( const string& dev ) const
    {
    unsigned long ret = 0;
    map<string,unsigned long>::const_iterator mit = pe_map.find( dev );
    if( mit!=pe_map.end() )
	ret = mit->second;
    return( ret );
    }

bool
Dm::mapsTo( const string& dev ) const
    {
    bool ret = false;
    map<string,unsigned long>::const_iterator mit;
    if( dev.find_first_of( "[.*^$" ) != string::npos )
	{
	Regex r(dev);
	mit = pe_map.begin();
	while( mit!=pe_map.end() && !r.match( mit->first ) && 
	       !pec()->addedPv(mit->first) )
	    ++mit;
	}
    else
	{
	mit = pe_map.find( dev );
	if( mit != pe_map.end() && pec()->addedPv(mit->first) )
	    mit = pe_map.end();
	}
    ret = mit != pe_map.end();
    if( ret )
	{
	y2mil("map:" << pe_map);
	y2mil("table:" << tname << " dev:" << dev << " ret:" << ret);
	}
    return( ret );
    }

bool 
Dm::checkConsistency() const
    {
    bool ret = false;
    unsigned long sum = 0;
    for( map<string,unsigned long>::const_iterator mit=pe_map.begin();
         mit!=pe_map.end(); ++mit )
	 sum += mit->second;
    if( sum != num_le )
	y2war("lv:" << dev << " sum:" << sum << " num:" << num_le);
    else
	ret = true;
    return( ret );
    }

void
Dm::setLe( unsigned long long le )
    {
    num_le = le;
    }

void Dm::init()
    {
    string dmn = "/dev/mapper/" + tname;
    if( dmn != dev )
	alt_names.push_back( dmn );
    //alt_names.push_back( "/dev/"+tname );
    if (!getStorage()->testmode())
	updateMajorMinor();
    }

void Dm::updateMajorMinor()
    {
    getMajorMinor();
    if( majorNr()==Dm::dmMajor() )
	{
	string d = "/dev/dm-" + decString(minorNr());
	if( d!=dev )
	    replaceAltName( "/dev/dm-", d );
	}
    num = mnr;
    }

const PeContainer* Dm::pec() const
    { 
    return(dynamic_cast<const PeContainer* const>(cont));
    }

void Dm::modifyPeSize( unsigned long long old, unsigned long long neww )
    {
    num_le = num_le * old / neww;
    calcSize();
    for( map<string,unsigned long>::iterator mit=pe_map.begin();
         mit!=pe_map.end(); ++mit )
	 mit->second = mit->second * old / neww;
    }
	        
void Dm::calcSize()
    {
    setSize( num_le*pec()->peSize() );
    }


void Dm::setPeMap( const std::map<string,unsigned long>& m )
{
    pe_map = m;

    // remove usused entries
    // TODO: do not generate pe_maps with unused entries in the first place
    for(std::map<string,unsigned long>::iterator i = pe_map.begin(); i != pe_map.end(); ) {
	if (i->second == 0)
	    pe_map.erase(i++);
	else
	    ++i;
    }

    y2mil("device:" << device() << " pe_map:" << pe_map);
}


void Dm::changeDeviceName( const string& old, const string& nw )
    {
    map<string,unsigned long>::const_iterator mit = pe_map.find( old );
    if( mit != pe_map.end() )
	{
        pe_map[nw] = mit->second;
	pe_map.erase(mit->first);
	}
    }

Text Dm::removeText( bool doing ) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/mapper/system
	txt = sformat( _("Deleting device mapper volume %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete device mapper volume %1$s (%2$s)"), dev.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

Text Dm::formatText( bool doing ) const
    {
    Text txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/mapper/system
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting device mapper volume %1$s (%2$s) with %3$s "),
		       dev.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format device mapper volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted device mapper volume %1$s (%2$s) for %4$s with %3$s"),
			       dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/mapper/system
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format device mapper volume %1$s (%2$s) with %3$s"),
			   dev.c_str(), sizeString().c_str(), 
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

void Dm::activate( bool val )
    {
	if (getenv("LIBSTORAGE_NO_DM") != NULL)
	    return;

    y2mil("old active:" << active << " val:" << val);
    if( active!=val )
	{
	SystemCmd c;
	if( val )
	    {
	    c.execute(DMSETUPBIN " version");
	    if( c.retcode()!=0 )
		{
		    c.execute(MODPROBEBIN " dm-mod");
		    c.execute(MODPROBEBIN " dm-snapshot");
		c.execute(DMSETUPBIN " version");
		}
	    }
	else
	    {
	    c.execute(DMSETUPBIN " info -c -o name,subsystem");
	    SystemCmd rm;
	    for( unsigned i=0; i<c.numLines(); i++ )
		{
		if( extractNthWord(1, c.getLine(i)) != "CRYPT" )
		    {
		    string cmd = DMSETUPBIN " remove ";
		    cmd += extractNthWord(0, c.getLine(i));
		    rm.execute( cmd  );
		    }
		}
	    }
	active = val;
	}
    }

string Dm::devToTable( const string& dev )
    {
    string ret = boost::replace_all_copy(undevDevice(dev), "/", "|");
    if( dev!=ret )
	y2mil("dev:" << dev << " ret:" << ret);
    return( ret );
    }


string Dm::dmDeviceName( unsigned long num )
    {
    return( "/dev/dm-" + decString(num));
    }


    string
    Dm::sysfsPath() const
    {
	return SYSFSDIR "/" + procName();
    }


void Dm::getInfo( DmInfo& tinfo ) const
    {
    Volume::getInfo(info.v);
    info.nr = num;
    info.table = tname;
    info.target = target;
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Dm &p )
    {
    s << p.shortPrintedName() << " ";
    s << dynamic_cast<const Volume&>(p);
    if( p.num_le>0 )
	s << " LE:" << p.num_le;
    s << " Table:" << p.tname;
    if( !p.target.empty() )
	s << " Target:" << p.target;
    if( p.inactiv )
      {
      s << " inactive";
      }
    if( p.stripe>1 )
      {
      s << " Stripes:" << p.stripe;
      if( p.stripe_size>0 )
	s << " StripeSize:" << p.stripe_size;
      }
    if( !p.pe_map.empty() )
      s << " pe_map:" << p.pe_map;
    return( s );
    }
    

bool Dm::equalContent( const Dm& rhs ) const
    {
    return( Volume::equalContent(rhs) &&
            num_le==rhs.num_le && stripe==rhs.stripe && inactiv==rhs.inactiv &&
            stripe_size==rhs.stripe_size && pe_map==rhs.pe_map );
    }


    void
    Dm::logDifference(std::ostream& log, const Dm& rhs) const
    {
	Volume::logDifference(log, rhs);

	logDiff(log, "num_le", num_le, rhs.num_le);
	logDiff(log, "stripes", stripe, rhs.stripe);
	logDiff(log, "stripe_size", stripe_size, rhs.stripe_size);

	logDiff(log, "pe_map", pe_map, rhs.pe_map);
    }


bool Dm::active = false;
unsigned Dm::dm_major = 0;

    static const char* elem[] = { "crypt" };
    const list<string> Dm::known_types(elem, elem + lengthof(elem));

}
