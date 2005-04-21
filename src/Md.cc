#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/Md.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Regex.h"
#include "y2storage/Container.h"

Md::Md( const Container& d, unsigned PNr, MdType Type, 
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
    }

Md::Md( const Container& d, const string& line1, const string& line2 )
    : Volume( d, 0, 0 )
    {
    y2milestone( "constructed md lines1:\"%s\" line2:\"%s\"", line1.c_str(), 
                 line2.c_str() );
    if( d.type() != MD )
	y2error( "constructed md with wrong container" );
    init();
    string tmp = extractNthWord( 0, line1 );
    if( tmp.find( "md" )==0 )
	tmp.erase( 0, 2 );
    tmp >> num;
    setNameDev();
    getMajorMinor( dev, mjr, mnr );
    string line = line1;
    string::size_type pos;
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
    md_parity = PAR_NONE;
    chunk = 0;
    md_type = RAID_UNK;
    }

void 
Md::getDevs( list<string>& devices, bool all, bool spares ) const
    { 
    if( !all )
	devices = spares ? devs : spare; 
    else
	{
	devices = devs;
	devices.insert( devices.end(), spare.begin(), spare.end() );
	}
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
    if( spare.size()>0 )
	cmd += " --spare-devices=" + decString(spare.size());
    for( list<string>::const_iterator i=devs.begin(); i!=devs.end(); ++i )
	cmd += " " + *i;
    for( list<string>::const_iterator i=spare.begin(); i!=spare.end(); ++i )
	cmd += " " + *i;
    y2milestone( "ret:%s", cmd.c_str() );
    return( cmd );
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
	    tmp += "miltipath";
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
	txt = sformat( _("Deleting Software raid %1$s"), d.c_str() );
	}
    else
	{
	d.erase( 0, 5 );
	// displayed text before action, %1$s is replaced by device name e.g. md0
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete Software raid %1$s %2$s"), d.c_str(),
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
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	txt = sformat( _("Creating Software raid %1$s"), d.c_str() );
	}
    else
	{
	d.erase( 0, 5 );
	if( mp.size()>0 )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create Software raid %1$s %2$s for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. md0
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create crypted Software raid %1$s %2$s for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. md0
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create Software raid %1$s %2$s"),
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
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	// %2$s is replaced by size (e.g. 623.5 MB)
	// %3$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting Software raid %1$s %2$s with %3$s "),
		       d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
	}
    else
	{
	d.erase( 0, 5 );
	if( mp.size()>0 )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format Software raid %1$s %2$s for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name e.g. md0 
		// %2$s is replaced by size (e.g. 623.5 MB)
		// %3$s is replaced by file system type (e.g. reiserfs)
		// %4$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format crypted Software raid %1$s %2$s for %4$s with %3$s"),
			       d.c_str(), sizeString().c_str(), fsTypeString().c_str(),
			       mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name e.g. /dev/system/var
	    // %2$s is replaced by size (e.g. 623.5 MB)
	    // %3$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format Software raid %1$s %2$s $s with %3$s"),
			   d.c_str(), sizeString().c_str(), fsTypeString().c_str() );
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

Md::MdParity
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
    string d(name);
    if( d.find( "/dev/" )==0 )
	d.erase( 0, 5 );
    static Regex md( "^md[0-9]+$" );
    if( md.match( d ))
	{
	d.substr( 2 )>>num;
	ret = true;
	}
    return( ret );
    }

string Md::md_names[] = { "unknown", "raid0", "raid1", "raid5", "raid6", 
                          "raid10", "multipath" };
string Md::par_names[] = { "none", "left-asymmetric", "left-symmetric", 
                           "right-asymmetric", "right-symmetric" };

