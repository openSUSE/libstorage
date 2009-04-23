/*
  Textdomain    "storage"
*/

#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

#include "y2storage/Md.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/Storage.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/Regex.h"
#include "y2storage/Container.h"
#include "y2storage/StorageDefines.h"


namespace storage
{
    using namespace std;


Md::Md( const MdCo& d, unsigned PNr, MdType Type, const list<string>& devices )
    : Volume( d, PNr, 0 )
    {
    y2deb("constructed md " << dev << " on container " << cont->name());
    if( d.type() != MD )
	y2err("constructed md with wrong container");
    init();
    md_type = Type;
    for( list<string>::const_iterator i=devices.begin(); i!=devices.end(); ++i )
	devs.push_back( normalizeDevice( *i ) );
    computeSize();
    }


Md::Md( const MdCo& d, const string& line1, const string& line2 )
    : Volume( d, 0, 0 )
    {
    y2mil("constructed md line1:\"" << line1 << "\" line2:\"" << line2 << "\"");
    if( d.type() != MD )
	y2err("constructed md with wrong container");
    init();
    if( mdStringNum( extractNthWord( 0, line1 ), num ))
	{
	nm.clear();
	setNameDev();
	getMajorMinor( dev, mjr, mnr );
	getContainer()->getStorage()->fetchDanglingUsedBy(dev, uby);
	}
    SystemCmd c(MDADMBIN " --detail " + quote(device()));
    c.select( "UUID : " );
    string::size_type pos;
    if( c.retcode()==0 && c.numLines(true)>0 )
	{
	md_uuid = c.getLine(0,true);
	if( (pos=md_uuid.find( "UUID : " ))!=string::npos )
	    md_uuid.erase( 0, pos+7 );
	md_uuid = extractNthWord( 0, md_uuid );
	}
    c.select( "Version : " );
    if( c.retcode()==0 && c.numLines(true)>0 )
	sb_ver = extractNthWord( 2, c.getLine(0,true) );
    if (c.retcode()==0 && c.numLines(true)>0 )
    {
	y2mil( "line:\"" << c.getLine(0,true) << "\"" );
	y2mil( "sb_ver:\"" << sb_ver << "\"" );
	y2mil( "word0:\"" << extractNthWord( 0, c.getLine(0,true)) << "\"" );
	y2mil( "word1:\"" << extractNthWord( 1, c.getLine(0,true)) << "\"" );
	y2mil( "word2:\"" << extractNthWord( 2, c.getLine(0,true)) << "\"" );
    }
    string tmp;
    string line = line1;
    if( (pos=line.find( ':' ))!=string::npos )
	line.erase( 0, pos+1 );
    boost::trim_left(line, locale::classic());
    if( (pos=line.find_first_of( app_ws ))!=string::npos )
    {
	if (line.substr(0, pos) == "active")
	    line.erase(0, pos);
    }
    boost::trim_left(line, locale::classic());
    if( (pos=line.find_first_of( app_ws ))!=string::npos )
	{
	tmp = line.substr( 0, pos );
	if( tmp=="(read-only)" || tmp=="(auto-read-only)" || tmp=="inactive" )
	    {
	    setReadonly();
	    y2war("readonly or inactive md device " << nr());
	    line.erase( 0, pos );
	    boost::trim_left(line, locale::classic());
	    }
	}
    boost::trim_left(line, locale::classic());
    if( (pos=line.find_first_of( app_ws ))!=string::npos )
	{
	if( line.substr( 0, pos ).find( "active" )!=string::npos )
	    line.erase( 0, pos );
	}
    boost::trim_left(line, locale::classic());
    tmp = extractNthWord( 0, line );
    md_type = toMdType( tmp );
    if( md_type == RAID_UNK )
	{
	y2war("unknown raid type " << tmp);
	}
    if( (pos=line.find_first_of( app_ws ))!=string::npos )
	line.erase( 0, pos );
    if( (pos=line.find_first_not_of( app_ws ))!=string::npos && pos!=0 )
	line.erase( 0, pos );
    while( (pos=line.find_first_not_of( app_ws ))==0 )
	{
	tmp = extractNthWord( 0, line );
	string::size_type bracket = tmp.find( '[' );
	if( bracket!=string::npos )
	    devs.push_back( normalizeDevice(tmp.substr( 0, bracket )));
	else
	    {
	    normalizeDevice(tmp);
	    devs.push_back( tmp );
	    }
	line.erase( 0, tmp.length() );
	if( (pos=line.find_first_not_of( app_ws ))!=string::npos && pos!=0 )
	    line.erase( 0, pos );
	}
    unsigned long long longnum;
    extractNthWord( 0, line2 ) >> longnum;
    setSize( longnum );
    chunk = 0;
    pos = line2.find( "chunk" );
    if( pos != string::npos )
	{
	pos = line2.find_last_not_of( app_ws, pos-1 );
	pos = line2.find_last_of( app_ws, pos );
	line2.substr( pos+1 ) >> chunk;
	}
    md_parity = PAR_NONE;
    pos = line2.find( "algorithm" );
    if( pos != string::npos )
	{
	unsigned alg = 999;
	pos = line2.find_first_of( app_ws, pos );
	pos = line2.find_first_not_of( app_ws, pos );
	line2.substr( pos ) >> alg;
	switch( alg )
	    {
	    case 0:
		md_parity = LEFT_ASYMMETRIC;
		break;
	    case 1:
		md_parity = RIGHT_ASYMMETRIC;
		break;
	    case 2:
		md_parity = LEFT_SYMMETRIC;
		break;
	    case 3:
		md_parity = RIGHT_SYMMETRIC;
		break;
	    default:
		y2war("unknown parity " << line2.substr(pos));
		break;
	    }
	}

    for (list<string>::iterator it = devs.begin(); it != devs.end(); ++it)
	getContainer()->getStorage()->setUsedBy(*it, UB_MD, dev.substr(5));
    }


Md::~Md()
    {
    y2deb("destructed md " << dev);
    }

void
Md::init()
    {
    destrSb = false;
    md_parity = PAR_NONE;
    chunk = 0;
    sb_ver = "01.00.00";
    md_type = RAID_UNK;
    }

void
Md::getDevs( list<string>& devices, bool all, bool spares ) const
    {
    if( !all )
	devices = spares ? spare : devs;
    else
	{
	devices = devs;
	devices.insert( devices.end(), spare.begin(), spare.end() );
	}
    }

int
Md::addDevice( const string& dev, bool to_spare )
    {
    int ret = 0;
    string d = normalizeDevice( dev );
    if( find( devs.begin(), devs.end(), dev )!=devs.end() ||
        find( spare.begin(), spare.end(), dev )!=spare.end() )
	{
	ret = MD_ADD_DUPLICATE;
	}
    if( ret==0 )
	{
	if( to_spare )
	    {
	    spare.push_back(d);
	    }
	else
	    {
	    devs.push_back(d);
	    computeSize();
	    }
	}
    y2mil("dev:" << dev << " spare:" << to_spare << " ret:" << ret);
    return( ret );
    }

int
Md::removeDevice( const string& dev )
    {
    int ret = 0;
    string d = normalizeDevice( dev );
    list<string>::iterator i;
    if( (i=find( devs.begin(), devs.end(), dev ))!=devs.end() )
	{
	devs.erase(i);
	computeSize();
	}
    else if( (i=find( spare.begin(), spare.end(), dev ))!=spare.end() )
	spare.erase(i);
    else
	ret = MD_REMOVE_NONEXISTENT;
    y2mil("dev:" << dev << " ret:" << ret);
    return( ret );
    }

int
Md::checkDevices()
    {
    unsigned nmin = 2;
    switch( md_type )
	{
	case RAID5:
	    nmin = 3;
	    break;
	case RAID6:
	    nmin = 4;
	    break;
	default:
	    break;
	}
    int ret = devs.size()<nmin ? MD_TOO_FEW_DEVICES : 0;
    y2mil("type:" << md_type << " min:" << nmin << " size:" << devs.size() <<
	  " ret:" << ret);
    return( ret );
    }

void
Md::getState(MdStateInfo& info) const
{
    SystemCmd c(MDADMBIN " --detail " + quote(device()));

    c.select("State : ");
    if( c.retcode()==0 && c.numLines(true)>0 )
    {
	string state = c.getLine(0,true);
	string::size_type pos;
	if( (pos=state.find( "State : " ))!=string::npos )
	    state.erase( 0, pos+8 );

	typedef boost::tokenizer<boost::char_separator<char>> char_tokenizer;
	char_tokenizer toker(state, boost::char_separator<char>(","));

	info.active = false;
	info.degraded = false;
	for (char_tokenizer::const_iterator it = toker.begin(); it != toker.end(); it++)
	{
	    string s = boost::trim_copy(*it, locale::classic());
	    if (s == "active")
		info.active = true;
	    else if (s == "degraded")
		info.degraded = true;
	}
    }
}

void
Md::computeSize()
{
    unsigned long long size_k;
    getContainer()->getStorage()->computeMdSize(md_type, devs, size_k);
    setSize(size_k);
}

void
Md::addSpareDevice( const string& dev )
    {
    string d = normalizeDevice(dev);
    if( find( spare.begin(), spare.end(), d )!=spare.end() ||
        find( devs.begin(), devs.end(), d )!=devs.end() )
	{
	y2war("spare " << dev << " already present");
	}
    else
	spare.push_back(d);
    }

void Md::changeDeviceName( const string& old, const string& nw )
    {
    list<string>::iterator i = find( devs.begin(), devs.end(), old );
    if( i!=devs.end() )
	*i = nw;
    i = find( spare.begin(), spare.end(), old );
    if( i!=spare.end() )
	*i = nw;
    }


string
Md::createCmd() const
{
    string cmd = "ls -l --full-time " + quote(devs) + " " + quote(spare) + "; ";
    cmd += MODPROBEBIN " " + pName() + "; " MDADMBIN " --create " + quote(device()) +
	" --run --level=" + pName() + " -e 1.0";
    if (pName() == "raid1" || pName() == "raid5" || pName() == "raid6" ||
        pName() == "raid10")
	cmd += " -b internal";
    if (chunk > 0)
	cmd += " --chunk=" + decString(chunk);
    if (md_parity != PAR_NONE)
	cmd += " --parity=" + ptName();
    cmd += " --raid-devices=" + decString(devs.size());
    if (!spare.empty())
	cmd += " --spare-devices=" + decString(spare.size());
    cmd += " " + quote(devs) + " " + quote(spare);
    y2mil("ret:" << cmd);
    return cmd;
}


string Md::mdadmLine() const
    {
    string line = "ARRAY " + device() + " level=" + pName();
    line += " UUID=" + md_uuid;
    y2mil("line:" << line);
    return( line );
    }

void Md::raidtabLines( list<string>& lines ) const
    {
    lines.clear();
    lines.push_back( "raiddev " + device() );
    string tmp = "   raid-level            ";
    switch( md_type )
	{
	case RAID1:
	    tmp += "1";
	    break;
	case RAID5:
	    tmp += "5";
	    break;
	case RAID6:
	    tmp += "6";
	    break;
	case RAID10:
	    tmp += "10";
	    break;
	case MULTIPATH:
	    tmp += "multipath";
	    break;
	default:
	    tmp += "0";
	    break;
	}
    lines.push_back( tmp );
    lines.push_back( "   nr-raid-disks         " + decString(devs.size()));
    lines.push_back( "   nr-spare-disks        " + decString(spare.size()));
    lines.push_back( "   persistent-superblock 1" );
    if( md_parity!=PAR_NONE )
	lines.push_back( "   parity-algorithm      " + ptName());
    if( chunk>0 )
	lines.push_back( "   chunk-size            " + decString(chunk));
    unsigned cnt = 0;
    for( list<string>::const_iterator i=devs.begin(); i!=devs.end(); ++i )
	{
	lines.push_back( "   device                " + *i);
	lines.push_back( "   raid-disk             " + decString(cnt++));
	}
    cnt = 0;
    for( list<string>::const_iterator i=spare.begin(); i!=spare.end(); ++i )
	{
	lines.push_back( "   device                " + *i);
	lines.push_back( "   spare-disk            " + decString(cnt++));
	}
    }


string Md::removeText( bool doing ) const
{
    string txt;
    if( doing )
    {
	// displayed text during action, %1$s is replaced by device name e.g. /dev/md0
	txt = sformat(_("Deleting software RAID %1$s"), dev.c_str());
    }
    else
    {
	// displayed text before action, %1$s is replaced by device name e.g. md0
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat(_("Delete software RAID %1$s (%2$s)"), dev.c_str(),
		      sizeString().c_str());
    }
    return txt;
}


string Md::createText( bool doing ) const
{
    string txt;
    if( doing )
    {
	// displayed text during action, %1$s is replaced by device name e.g. /dev/md0
	// %2$s is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
	txt = sformat(_("Creating software RAID %1$s from %2$s"), dev.c_str(), 
		      boost::join(devs, " ").c_str());
    }
    else
    {
	if( !mp.empty() )
	{
	    if( encryption==ENC_NONE )
	    {
		// displayed text before action, %1$s is replaced by device name e.g. md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		// %5$s is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
		txt = sformat(_("Create software RAID %1$s (%2$s) from %5$s for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str(), boost::join(devs, " ").c_str());
	    }
	    else
	    {
		// displayed text before action, %1$s is replaced by device name e.g. md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		// %5$s is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
		txt = sformat(_("Create encrypted software RAID %1$s (%2$s) from %5$s for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str(), boost::join(devs, " ").c_str());
	    }
	}
	else
	{
	    // displayed text before action, %1$s is replaced by device name e.g. md0
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by one or more devices (e.g /dev/sda1 /dev/sda2)
	    txt = sformat(_("Create software RAID %1$s (%2$s) from %3$s"), dev.c_str(),
			  sizeString().c_str(), boost::join(devs, " ").c_str());
	}
    }
    return txt;
}


string Md::formatText( bool doing ) const
{
    string txt;
    if( doing )
    {
	// displayed text during action, %1$s is replaced by device name e.g. /dev/md0
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat(_("Formatting software RAID %1$s (%2$s) with %3$s "),
		      dev.c_str(), sizeString().c_str(), fsTypeString().c_str());
    }
    else
    {
	if( !mp.empty() )
	{
	    if( encryption==ENC_NONE )
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat(_("Format software RAID %1$s (%2$s) for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str());
	    }
	    else
	    {
		// displayed text before action, %1$s is replaced by device name e.g. /dev/md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat(_("Format encrypted software RAID %1$s (%2$s) for %4$s with %3$s"),
			      dev.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			      mp.c_str());
	    }
	}
	else
	{
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/md0
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat(_("Format software RAID %1$s (%2$s) with %3$s"),
			  dev.c_str(), sizeString().c_str(), fsTypeString().c_str());
	}
    }
    return txt;
}


MdType
Md::toMdType( const string& val )
    {
    MdType ret = MULTIPATH;
    while( ret!=RAID_UNK && val!=md_names[ret] )
	{
	ret = MdType(ret-1);
	}
    return( ret );
    }

MdParity
Md::toMdParity( const string& val )
    {
    enum MdParity ret = RIGHT_SYMMETRIC;
    while( ret!=PAR_NONE && val!=par_names[ret] )
	{
	ret = MdParity(ret-1);
	}
    return( ret );
    }

bool Md::matchRegex( const string& dev )
    {
    static Regex md( "^md[0123456789]+$" );
    return( md.match(dev));
    }

bool Md::mdStringNum( const string& name, unsigned& num )
    {
    bool ret=false;
    string d = undevDevice(name);
    if( matchRegex( d ))
	{
	d.substr( 2 )>>num;
	ret = true;
	}
    return( ret );
    }

void Md::setPersonality( MdType val )
    {
    md_type=val;
    computeSize();
    }

unsigned Md::mdMajor()
    {
    if( md_major==0 )
	getMdMajor();
    return( md_major );
    }

void Md::getMdMajor()
    {
    md_major = getMajorDevices( "md" );
    y2mil("md_major:" << md_major);
    }


void Md::getInfo( MdInfo& tinfo ) const
    {
    ((Volume*)this)->getInfo( info.v );
    info.nr = num;
    info.type = md_type;
    info.uuid = md_uuid;
    info.sb_ver = sb_ver;
    info.chunk = chunk;
    info.parity = md_parity;
    info.devices.clear();
    list<string>::const_iterator i=devs.begin();
    while( i!=devs.end() )
	{
	if( !info.devices.empty() )
	    info.devices += ' ';
	info.devices += *i;
	++i;
	}
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Md& m )
    {
    s << "Md " << *(Volume*)&m
      << " Personality:" << m.pName();
    if( m.chunk>0 )
	s << " Chunk:" << m.chunk;
    if( m.md_parity!=storage::PAR_NONE )
	s << " Parity:" << m.ptName();
    if( !m.sb_ver.empty() )
	s << " SbVer:" << m.sb_ver;
    if( !m.md_uuid.empty() )
	s << " MD UUID:" << m.md_uuid;
    if( m.destrSb )
	s << " destroySb";
    s << " Devices:" << m.devs;
    if( !m.spare.empty() )
	s << " Spares:" << m.spare;
    return( s );
    }


bool Md::equalContent( const Md& rhs ) const
    {
    return( Volume::equalContent(rhs) &&
            md_type==rhs.md_type && md_parity==rhs.md_parity &&
	    chunk==rhs.chunk && md_uuid==rhs.md_uuid && sb_ver==rhs.sb_ver &&
	    destrSb==rhs.destrSb && devs == rhs.devs && spare==rhs.spare );
    }

void Md::logDifference( const Md& rhs ) const
    {
    string log = Volume::logDifference( rhs );
    if( md_type!=rhs.md_type )
	log += " Personality:" + md_names[md_type] + "-->" +
	       md_names[rhs.md_type];
    if( md_parity!=rhs.md_parity )
	log += " Parity:" + par_names[md_parity] + "-->" +
	       par_names[rhs.md_parity];
    if( chunk!=rhs.chunk )
	log += " Chunk:" + decString(chunk) + "-->" + decString(rhs.chunk);
    if( sb_ver!=rhs.sb_ver )
	log += " SbVer:" + sb_ver + "-->" + rhs.sb_ver;
    if( md_uuid!=rhs.md_uuid )
	log += " MD-UUID:" + md_uuid + "-->" + rhs.md_uuid;
    if( destrSb!=rhs.destrSb )
	{
	if( rhs.destrSb )
	    log += " -->destrSb";
	else
	    log += " destrSb-->";
	}
    if( devs!=rhs.devs )
	{
	std::ostringstream b;
	classic(b);
	b << " Devices:" << devs << "-->" << rhs.devs;
	log += b.str();
	}
    if( spare!=rhs.spare )
	{
	std::ostringstream b;
	classic(b);
	b << " Spares:" << spare << "-->" << rhs.spare;
	log += b.str();
	}
    y2mil(log);
    }

Md& Md::operator= ( const Md& rhs )
    {
    y2deb("operator= from " << rhs.nm);
    *((Volume*)this) = rhs;
    md_type = rhs.md_type;
    md_parity = rhs.md_parity;
    chunk = rhs.chunk;
    md_uuid = rhs.md_uuid;
    sb_ver = rhs.sb_ver;
    destrSb = rhs.destrSb;
    devs = rhs.devs;
    spare = rhs.spare;
    return( *this );
    }

Md::Md( const MdCo& d, const Md& rhs ) : Volume(d)
    {
    y2deb("constructed md by copy constructor from " << rhs.dev);
    *this = rhs;
    }

string Md::md_names[] = { "unknown", "raid0", "raid1", "raid5", "raid6",
                          "raid10", "multipath" };
string Md::par_names[] = { "none", "left-asymmetric", "left-symmetric",
                           "right-asymmetric", "right-symmetric" };
unsigned Md::md_major = 0;

}
