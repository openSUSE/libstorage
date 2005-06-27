/*
  Textdomain    "storage"
*/

#include <sstream>

#include "y2storage/Md.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/Storage.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/Regex.h"
#include "y2storage/Container.h"

using namespace storage;
using namespace std;

Md::Md( const MdCo& d, unsigned PNr, MdType Type, 
        const list<string>& devices ) : Volume( d, PNr, 0 )

    {
    y2milestone( "constructed md %s on container %s", dev.c_str(),
                 cont->name().c_str() );
    if( d.type() != MD )
	y2error( "constructed md with wrong container" );
    init();
    md_type = Type;
    for( list<string>::const_iterator i=devices.begin(); i!=devices.end(); ++i )
	devs.push_back( normalizeDevice( *i ) );
    computeSize();
    }

Md::Md( const MdCo& d, const string& line1, const string& line2 )
    : Volume( d, 0, 0 )
    {
    y2milestone( "constructed md lines1:\"%s\" line2:\"%s\"", line1.c_str(), 
                 line2.c_str() );
    if( d.type() != MD )
	y2error( "constructed md with wrong container" );
    init();
    if( mdStringNum( extractNthWord( 0, line1 ), num ))
	{
	setNameDev();
	getMajorMinor( dev, mjr, mnr );
	}
    SystemCmd c( "mdadm -D " + device() + " | grep 'UUID : '" );
    string::size_type pos;
    if( c.retcode()==0 && c.numLines()>0 )
	{
	md_uuid = *c.getLine(0);
	if( (pos=md_uuid.find( "UUID : " ))!=string::npos )
	    md_uuid.erase( 0, pos+7 );
	md_uuid = extractNthWord( 0, md_uuid );
	}
    string tmp;
    string line = line1;
    if( (pos=line.find( ':' ))!=string::npos )
	line.erase( 0, pos+1 );
    if( (pos=line.find_first_not_of( app_ws ))!=string::npos && pos!=0 )
	line.erase( 0, pos );
    if( (pos=line.find_first_of( app_ws ))!=string::npos )
	{
	tmp = line.substr( 0, pos );
	if( tmp=="(read-only)" || tmp=="inactive" )
	    {
	    setReadonly();
	    y2warning( "readonly or inactive md device %d", nr() );
	    line.erase( 0, pos );
	    }
	}
    if( (pos=line.find_first_not_of( app_ws ))!=string::npos && pos!=0 )
	line.erase( 0, pos );
    if( (pos=line.find_first_of( app_ws ))!=string::npos )
	{
	if( line.substr( 0, pos ).find( "active" )!=string::npos )
	    line.erase( 0, pos );
	}
    if( (pos=line.find_first_not_of( app_ws ))!=string::npos && pos!=0 )
	line.erase( 0, pos );
    tmp = extractNthWord( 0, line );
    md_type = toMdType( tmp );
    if( md_type == RAID_UNK )
	{
	y2warning( "unknown raid type %s", tmp.c_str() );
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
		y2warning( "unknown parity %s", line2.substr( pos ).c_str() );
		break;
	    }
	}
    }

Md::~Md()
    {
    y2milestone( "destructed md %s", dev.c_str() );
    }

void
Md::init()
    {
    destrSb = false;
    md_parity = PAR_NONE;
    chunk = 0;
    md_type = RAID_UNK;
    }

void 
Md::getDevs( list<string>& devices, bool all, bool spares ) const
    { 
    if( !all )
	devices = spares ? spare : devs ; 
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
    y2milestone( "dev:%s spare:%d ret:%d", dev.c_str(), to_spare, ret );
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
    y2milestone( "dev:%s ret:%d", dev.c_str(), ret );
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
    y2milestone( "type:%d min:%u size:%d ret:%d", md_type, nmin, devs.size(),
                 ret );
    return( ret );
    }

void
Md::computeSize()
    {
    unsigned long long sum = 0;
    unsigned long long smallest = 0;
    list<string>::const_iterator i=devs.begin();
    while( i!=devs.end() )
	{
	const Volume* v = getContainer()->getStorage()->getVolume( *i );
	sum += v->sizeK();
	if( smallest==0 )
	    smallest = v->sizeK();
	else
	    smallest = min( smallest, v->sizeK() );
	++i;
	}
    unsigned long long rsize = 0;
    switch( md_type )
	{
	case RAID0:
	    rsize = sum;
	    break;
	case RAID1:
	case MULTIPATH:
	    rsize = smallest;
	    break;
	case RAID5:
	    rsize = devs.size()==0 ? 0 : smallest*(devs.size()-1);
	    break;
	case RAID6:
	    rsize = devs.size()<2 ? 0 : smallest*(devs.size()-2);
	    break;
	case RAID10:
	    rsize = smallest*devs.size()/2;
	    break;
	default:
	    rsize = 0;
	}
    y2milestone( "type:%d smallest:%llu sum:%llu size:%llu", md_type, 
                 smallest, sum, rsize );
    setSize( rsize );
    }

void 
Md::addSpareDevice( const string& dev ) 
    { 
    string d = normalizeDevice(dev);
    if( find( spare.begin(), spare.end(), d )!=spare.end() ||
        find( devs.begin(), devs.end(), d )!=devs.end() )
	{
	y2warning( "spare %s already present", dev.c_str() );
	}
    else
	spare.push_back(d);
    }

string Md::createCmd() const
    {
    string cmd = "mdadm --create " + device() + " --run --level=" + pName();
    if( chunk>0 )
	cmd += " --chunk=" + decString(chunk);
    if( md_parity!=PAR_NONE )
	cmd += " --parity=" + ptName();
    cmd += " --raid-devices=" + decString(devs.size());
    if( !spare.empty() )
	cmd += " --spare-devices=" + decString(spare.size());
    for( list<string>::const_iterator i=devs.begin(); i!=devs.end(); ++i )
	cmd += " " + *i;
    for( list<string>::const_iterator i=spare.begin(); i!=spare.end(); ++i )
	cmd += " " + *i;
    y2milestone( "ret:%s", cmd.c_str() );
    return( cmd );
    }

string Md::mdadmLine() const
    {
    string line = "ARRAY " + device() + " level=" + pName();
    line += " UUID=" + md_uuid;
    y2milestone( "line:%s", line.c_str() );
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
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/md0
	txt = sformat( _("Deleting software RAID %1$s"), d.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name e.g. md0
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete software RAID %1$s (%2$s)"), d.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

string Md::createText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/md0
	txt = sformat( _("Creating software RAID %1$s"), d.c_str() );
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
		txt = sformat( _("Create software RAID %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create encrypted software RAID %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. md0
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create software RAID %1$s (%2$s)"),
			   dev.c_str(), sizeString().c_str() );
	    }
	}
    return( txt );
    }

string Md::formatText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/md0
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting software RAID %1$s (%2$s) with %3$s "),
		       d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
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
		txt = sformat( _("Format software RAID %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), 
			       fsTypeString().c_str(), mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/md0 
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted software RAID %1$s (%2$s) for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), 
			       fsTypeString().c_str(), mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/md0
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format software RAID %1$s (%2$s) with %3$s"),
			   d.c_str(), sizeString().c_str(), 
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

MdType
Md::toMdType( const string& val )
    {
    enum MdType ret = MULTIPATH;
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

bool Md::mdStringNum( const string& name, unsigned& num )
    {
    bool ret=false;
    string d = undevDevice(name);
    static Regex md( "^md[0-9]+$" );
    if( md.match( d ))
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


void Md::getInfo( MdInfo& info ) const
    {
    info.nr = num;
    info.type = md_type;
    info.uuid = md_uuid;
    info.chunk = chunk;
    info.parity = md_parity;
    info.dlist.clear();
    list<string>::const_iterator i=devs.begin();
    while( i!=devs.end() )
	{
	if( !info.dlist.empty() )
	    info.dlist += ' ';
	info.dlist += *i;
	++i;
	}
    y2milestone( "dlist:\"%s\"", info.dlist.c_str() );
    }

std::ostream& operator<< (std::ostream& s, const Md& m )
    {
    s << "Md " << *(Volume*)&m
      << " Personality:" << m.pName();
    if( m.chunk>0 )
	s << " Chunk:" << m.chunk;
    if( m.md_parity!=storage::PAR_NONE )
	s << " Parity:" << m.ptName();
    if( !m.md_uuid.empty() )
	s << " MD UUID:" << m.md_uuid;
    if( m.destrSb )
	s << " destroySb";
    s << " Devices:" << m.devs;
    if( !m.spare.empty() )
	s << " Spare:" << m.spare;
    return( s );
    }


bool Md::equalContent( const Md& rhs ) const
    {
    return( Volume::equalContent(rhs) &&
            md_type==rhs.md_type && md_parity==rhs.md_parity && 
	    chunk==rhs.chunk && md_uuid==rhs.md_uuid && destrSb==rhs.destrSb &&
	    devs == rhs.devs && spare==rhs.spare );
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
	b << " Devices:" << devs << "-->" << rhs.devs;
	log += b.str();
	}
    if( spare!=rhs.spare )
	{
	std::ostringstream b;
	b << " Devices:" << spare << "-->" << rhs.spare;
	log += b.str();
	}
    y2milestone( "%s", log.c_str() );
    }

Md& Md::operator= ( const Md& rhs )
    {
    y2milestone( "operator= from %s", rhs.nm.c_str() );
    *((Volume*)this) = rhs;
    md_type = rhs.md_type;
    md_parity = rhs.md_parity;
    chunk = rhs.chunk;
    md_uuid = rhs.md_uuid;
    destrSb = rhs.destrSb;
    devs = rhs.devs;
    spare = rhs.spare;
    return( *this );
    }

Md::Md( const MdCo& d, const Md& rhs ) : Volume(d)
    {
    y2milestone( "constructed md by copy constructor from %s", 
                 rhs.dev.c_str() );
    *this = rhs;
    }


string Md::md_names[] = { "unknown", "raid0", "raid1", "raid5", "raid6", 
                          "raid10", "multipath" };
string Md::par_names[] = { "none", "left-asymmetric", "left-symmetric", 
                           "right-asymmetric", "right-symmetric" };

