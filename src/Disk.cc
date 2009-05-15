/*
  Textdomain    "storage"
*/

#include <fcntl.h>
#include <sys/mount.h>         /* for BLKGETSIZE */
#include <linux/hdreg.h>       /* for HDIO_GETGEO */

#include <string>
#include <ostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <boost/algorithm/string.hpp>

#include "y2storage/Region.h"
#include "y2storage/Partition.h"
#include "y2storage/ProcPart.h"
#include "y2storage/Disk.h"
#include "y2storage/Storage.h"
#include "y2storage/AsciiFile.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/StorageDefines.h"


namespace storage
{
    using namespace std;


Disk::Disk( Storage * const s, const string& Name,
            unsigned long long SizeK ) :
    Container(s,"",staticType())
    {
    init_disk = dmp_slave = iscsi = gpt_enlarge = false;
    nm = Name;
    undevDevice(nm);
    logfile_name = boost::replace_all_copy(nm, "/", "_");
    if( Name.find( "/dev/" )==0 )
	dev = Name;
    else 
	dev = "/dev/" + Name;
    size_k = SizeK;
    y2deb("constructed disk " << dev);
    }

Disk::Disk( Storage * const s, const string& Name,
            unsigned num, unsigned long long SizeK, ProcPart& ppart ) :
    Container(s,Name,staticType())
    {
    y2mil("constructed disk " << Name << " nr " << num << " sizeK:" << SizeK);
    logfile_name = Name + decString(num);
    init_disk = dmp_slave = iscsi = gpt_enlarge = false;
    ronly = true;
    size_k = SizeK;
    head = new_head = 16;
    sector = new_sector = 32;
    cyl = new_cyl = std::max( size_k*2 / head / sector, 1ULL );
    byte_cyl = head * sector * 512;
    unsigned long long sz = size_k;
    Partition *p = new Partition( *this, num, sz, 0, cyl, PRIMARY );
    if( ppart.getSize( p->device(), sz ) && sz>0 )
	{
	p->setSize( sz );
	}
    addToList( p );
    }


Disk::Disk(Storage * const s, const string& fname)
    : Container(s, "", staticType())
{
    init_disk = dmp_slave = iscsi = gpt_enlarge = false;
    nm = fname.substr( fname.find_last_of( '/' )+1);
    if( nm.find("disk_")==0 )
	nm.erase( 0, 5 );

    AsciiFile file(fname);
    const vector<string>& lines = file.lines();
    vector<string>::const_iterator it;

    if ((it = find_if(lines, string_starts_with("Device:"))) != lines.end())
	dev = extractNthWord(1, *it);

    mnr = mjr = 0;
    if ((it = find_if(lines, string_starts_with("Major:"))) != lines.end())
	extractNthWord(1, *it) >> mjr;
    if ((it = find_if(lines, string_starts_with("Minor:"))) != lines.end())
	extractNthWord(1, *it) >> mnr;

    range = 4;
    if ((it = find_if(lines, string_starts_with("Range:"))) != lines.end())
	extractNthWord(1, *it) >> range;

    cyl = 1024;
    if ((it = find_if(lines, string_starts_with("Cylinder:"))) != lines.end())
	extractNthWord(1, *it) >> cyl;
    head = 1024;
    if ((it = find_if(lines, string_starts_with("Head:"))) != lines.end())
	extractNthWord(1, *it) >> head;
    sector = 32;
    if ((it = find_if(lines, string_starts_with("Sector:"))) != lines.end())
	extractNthWord(1, *it) >> sector;
    byte_cyl = head * sector * 512;

    if ((it = find_if(lines, string_starts_with("Label:"))) != lines.end())
	label = extractNthWord(1, *it);

    max_primary = 0;
    if ((it = find_if(lines, string_starts_with("MaxPrimary:"))) != lines.end())
	extractNthWord(1, *it) >> max_primary;
    ext_possible = false;
    if ((it = find_if(lines, string_starts_with("ExtPossible:"))) != lines.end())
	extractNthWord(1, *it) >> ext_possible;
    max_logical = 0;
    if ((it = find_if(lines, string_starts_with("MaxLogical:"))) != lines.end())
	extractNthWord(1, *it) >> max_logical;

    ronly = false;
    if ((it = find_if(lines, string_starts_with("Readonly:"))) != lines.end())
	extractNthWord(1, *it) >> ronly;

    if( FakeDisk() && isdigit( nm[nm.size()-1] ))
    {
	string::size_type p = nm.find_last_not_of( "0123456789" );
	nm.erase( p+1 );
    }

    size_k = 0;
    if ((it = find_if(lines, string_starts_with("SizeK:"))) != lines.end())
	extractNthWord(1, *it) >> size_k;

    udev_path.clear();
    if ((it = find_if(lines, string_starts_with("UdevPath:"))) != lines.end())
	udev_path = extractNthWord(1, *it);
    udev_id.clear();
    if ((it = find_if(lines, string_starts_with("UdevId:"))) != lines.end())
	udev_id.push_back(extractNthWord(1, *it));

    for (it = lines.begin(); it != lines.end(); ++it)
    {
	if (string_starts_with("Partition:")(*it))
	{
	    Partition* p = new Partition(*this, extractNthWord(1, *it, true));
	    addToList( p );
	}
    }

    y2deb("constructed disk " << dev << " from file " << fname);
}


Disk::~Disk()
{
    y2deb("destructed disk " << dev);
}


void
Disk::triggerUdevUpdate() const
{
    ConstPartPair pp = partPair();
    for( ConstPartIter p=pp.begin(); p!=pp.end(); ++p )
    {
	if( !p->deleted() && !p->created() )
	    p->triggerUdevUpdate();
    }
}


void
Disk::setUdevData(const string& path, const list<string>& id)
{
    y2mil("disk:" << nm << " path:" << path << "id:" << id);
    udev_path = path;
    udev_id = id;
    udev_id.remove_if(string_starts_with("edd-"));
    partition(udev_id.begin(), udev_id.end(), string_starts_with("ata-"));
    y2mil("id:" << udev_id);

    PartPair pp = partPair();
    for( PartIter p=pp.begin(); p!=pp.end(); ++p )
	{
	p->addUdevData();
	}
}


unsigned long long
Disk::cylinderToKb( unsigned long cylinder ) const
    {
    return (unsigned long long)byte_cyl * cylinder / 1024;
    }

unsigned long
Disk::kbToCylinder( unsigned long long kb ) const
    {
    unsigned long long bytes = kb * 1024;
    bytes += byte_cyl - 1;
    unsigned long ret = bytes/byte_cyl;
    y2mil("KB:" << kb << " ret:" << ret << " byte_cyl:" << byte_cyl);
    return (ret);
    }

bool Disk::detect( ProcPart& ppart )
    {
    return( detectGeometry() && detectPartitions(ppart) );
    }

bool Disk::detectGeometry()
    {
    y2mil("disk:" << device());
    bool ret = false;
    int fd = open( device().c_str(), O_RDONLY );
    if( fd >= 0 )
	{
	head = 255;
	sector = 63;
	cyl = 16;
	struct hd_geometry geometry;
	int rcode = ioctl( fd, HDIO_GETGEO, &geometry );
	if( rcode==0 )
	    {
	    head = geometry.heads>0?geometry.heads:head;
	    sector = geometry.sectors>0?geometry.sectors:sector;
	    cyl = geometry.cylinders>0?geometry.cylinders:cyl;
	    }
	y2mil("After HDIO_GETGEO ret " << rcode << " Head:" << head << " Sector:" << sector <<
	      " Cylinder:" << cyl);
	__uint64_t sect = 0;
	rcode = ioctl( fd, BLKGETSIZE64, &sect);
	y2mil("BLKGETSIZE64 Ret:" << rcode << " Bytes:" << sect);
	if( rcode==0 && sect!=0 )
	    {
	    sect /= 512;
	    cyl = (unsigned)(sect / (__uint64_t)(head*sector));
	    ret = true;
	    }
	else
	    {
	    unsigned long lsect;
	    rcode = ioctl( fd, BLKGETSIZE, &lsect );
	    y2mil("BLKGETSIZE Ret:" << rcode << " Sect:" << lsect);
	    if( rcode==0 && lsect!=0 )
		{
		cyl = lsect / (unsigned long)(head*sector);
		ret = true;
		}
	    }
	y2mil("After getsize Cylinder:" << cyl);
	close( fd );
	}
    byte_cyl = head * sector * 512;
    y2mil("ret:" << ret << " byte_cyl:" << byte_cyl);
    return( ret );
    }

bool Disk::getSysfsInfo( const string& SysfsDir )
    {
    bool ret = true;
    sysfs_dir = SysfsDir;
    y2mil("sysfs_dir:" << sysfs_dir);
    string SysfsFile = sysfs_dir+"/range";
    if( access( SysfsFile.c_str(), R_OK )==0 )
	{
	ifstream File( SysfsFile.c_str() );
	classic(File);
	File >> range;
	if( range<=1 ) ret = false;
	}
    else
	{
	ret = false;
	}
    SysfsFile = sysfs_dir+"/dev";
    if( access( SysfsFile.c_str(), R_OK )==0 )
	{
	ifstream File( SysfsFile.c_str() );
	classic(File);
	char c;
	File >> mjr;
	File >> c;
	File >> mnr;
	}
    else
	{
	ret = false;
	}
    SysfsFile = sysfs_dir+"/device";
    string lname;
    if (access(SysfsFile.c_str(), R_OK) == 0 && readlink(SysfsFile, lname))
	{
	if( lname.find( "/session" )!=string::npos )
	    iscsi = true;
	y2mil("lname:" << lname);
	}
    y2mil("Ret:" << ret << " Range:" << range << " Major:" << mjr << " Minor:" << mnr <<
	  " iSCSI:" << iscsi);
    return( ret );
    }

void Disk::getGeometry( const string& line, unsigned long& c, unsigned& h,
                        unsigned& s )
    {
    string tmp( line );
    tmp.erase( 0, tmp.find(':')+1 );
    tmp = extractNthWord( 0, tmp );
    list<string> geo = splitString( extractNthWord( 0, tmp ), "," );
    list<string>::const_iterator i = geo.begin();
    unsigned long val = 0;
    bool sect_head_changed = false;
    bool cyl_changed = false;
    if( i!=geo.end() )
	{
	*i >> val;
	if( val>0 )
	    {
	    c = val;
	    cyl_changed = true;
	    }
	}
    ++i;
    val = 0;
    if( i!=geo.end() )
	{
	*i >> val;
	if( val>0 )
	    {
	    h = (unsigned)val;
	    sect_head_changed = true;
	    }
	}
    ++i;
    val = 0;
    if( i!=geo.end() )
	{
	*i >> val;
	if( val>0 )
	    {
	    s = (unsigned)val;
	    sect_head_changed = true;
	    }
	}
    if( !cyl_changed && sect_head_changed )
	{
	c = sizeK()*2/(s*h);
	if( c<=0 )
	    c=1;
	y2mil("new c:" << c);
	}
    y2mil("line:" << line);
    y2mil("c:" << c << " h:" << h << " s:" << s);
    }

bool Disk::detectPartitions( ProcPart& ppart )
    {
    bool ret = true;
    string cmd_line = PARTEDCMD + quote(device()) + " unit cyl print | sort -n";
    string dlabel;
    system_stderr.erase();
    y2mil("executing cmd:" << cmd_line);
    SystemCmd Cmd( cmd_line );
    checkSystemError( cmd_line, Cmd );
    if( Cmd.select( "Partition Table:" )>0 )
	{
	string tmp = Cmd.getLine(0, true);
	y2mil("Label line:" << tmp);
	dlabel = extractNthWord( 2, tmp );
	}
    if( Cmd.select( "BIOS cylinder" )>0 )
	{
	string tmp = Cmd.getLine(0, true);
	getGeometry( tmp, cyl, head, sector );
	new_cyl = cyl;
	new_head = head;
	new_sector = sector;
	y2mil("After parted Head:" << head << " Sector:" << sector << " Cylinder:" << cyl);
	byte_cyl = head * sector * 512;
	y2mil("byte_cyl:" << byte_cyl);
	}
    gpt_enlarge = Cmd.select( "fix the GPT to use all" )>0;
    y2mil("Label:" << dlabel << " gpt_enlarge:" << gpt_enlarge);
    if( dlabel!="loop" )
	{
	setLabelData( dlabel );
	checkPartedOutput( Cmd, ppart );
	if( dlabel.empty() )
	    {
	    Cmd.setCombine();
	    Cmd.execute(FDISKBIN " -l " + quote(device()));
	    if( Cmd.select( "AIX label" )>0 )
		{
		detected_label = "aix";
		}
	    }
	}
    else
	dlabel.erase();
    if( detected_label.empty() )
	detected_label = dlabel;
    if( dlabel.empty() )
	dlabel = defaultLabel(getStorage()->efiBoot(), size_k);
    setLabelData( dlabel );

    if (label == "unsupported")
	{
	string txt = sformat(
	// popup text %1$s is replaced by disk name e.g. /dev/hda
_("The partition table type on disk %1$s cannot be handled by\n"
"this tool.\n"
"\n"
"You can use the partitions on disk %1$s as they are.\n"
"You can format them and assign mount points to them, but you\n"
"cannot add, edit, resize, or remove partitions from that\n"
"disk with this tool."), dev.c_str() );
	y2war( "unsupported disk label on " << dev << " txt:" << txt );

	detected_label = label;
	ronly = true;
	}

    y2mil("ret:" << ret << " partitons:" << vols.size() << " detected label:" << detected_label <<
	  " label:" << label);
    return( ret );
    }


void
Disk::logData(const string& Dir) const
    {
    string fname( Dir + "/disk_" + logfile_name + ".tmp" );
    ofstream file( fname.c_str() );
    classic(file);
    file << "Device: " << dev << endl;
    if( !udev_path.empty() )
	file << "UdevPath: " << udev_path << endl;
    if( !udev_id.empty() )
	file << "UdevId: " << udev_id << endl;
    file << "Major: " << mjr << endl;
    file << "Minor: " << mnr << endl;
    file << "Range: " << range << endl;

    file << "Cylinder: " << cyl << endl;
    file << "Head: " << head << endl;
    file << "Sector: " << sector << endl;

    file << "Label: " << label << endl;
    file << "MaxPrimary: " << max_primary << endl;
    if( ext_possible )
	{
	file << "ExtPossible: " << ext_possible << endl;
	file << "MaxLogical: " << max_logical << endl;
	}
    if( ronly )
	{
	file << "Readonly: " << ronly << endl;
	}
    file << "SizeK: " << size_k << endl;

    ConstPartPair pp = partPair();
    for (ConstPartIter p = pp.begin(); p != pp.end(); ++p)
	{
	file << "Partition: ";
	p->logData(file);
	file << endl;
	}
    file.close();
    getStorage()->handleLogFile( fname );
    }


void
Disk::setLabelData( const string& disklabel )
    {
    y2mil("disklabel:" << disklabel);
    int i=0;
    while( !labels[i].name.empty() && labels[i].name!=disklabel )
	{
	i++;
	}
    if( labels[i].name.empty() )
	{
	y2err("unknown disklabel " << disklabel);
	ext_possible = false;
	max_primary = 0;
	max_logical = 0;
	label = "unsupported";
	}
    else
        {
	ext_possible = labels[i].extended;
	max_primary = min(labels[i].primary,unsigned(range-1));
	max_logical = min(labels[i].logical,unsigned(range-1));
	label = labels[i].name;
	}
    y2mil("name:" << label << " ext:" << ext_possible << " primary:" << max_logical << 
	  " logical:" << max_logical);
    }

unsigned long long
Disk::maxSizeLabelK( const string& label )
    {
    unsigned long long ret = 0;
    int i=0;
    while( !labels[i].name.empty() && labels[i].name!=label )
	{
	i++;
	}
    if( !labels[i].name.empty() )
        {
	ret = labels[i].max_size_k;
	}
    y2mil("label:" << label << " ret:" << ret);
    return( ret );
    }

int
Disk::checkSystemError( const string& cmd_line, const SystemCmd& cmd )
    {
    string tmp = boost::join(cmd.stderr(), "\n");
    if (!tmp.empty())
        {
	y2err("cmd:" << cmd_line);
	y2err("err:" << tmp);
	if( !system_stderr.empty() )
	    {
	    system_stderr += "\n";
	    }
	system_stderr += tmp;
        }
    tmp = boost::join(cmd.stdout(), "\n");
    if (!tmp.empty())
        {
	y2mil("cmd:" << cmd_line);
	y2mil("out:" << tmp);
	if( !system_stderr.empty() )
	    {
	    system_stderr += "\n";
	    }
	system_stderr += tmp;
        }
    int ret = cmd.retcode();
    if( ret!=0 && cmd_line.find( device()+" set" )!=string::npos &&
        tmp.find( "kernel was unable to re-read" )!=string::npos )
	{
	y2mil( "resetting retcode set cmd " << ret << " of:" << cmd_line );
	ret = 0;
	}
    if( ret != 0 )
        {
	if( dmp_slave && tmp.empty() )
	    {
	    y2mil( "resetting retcode " << ret << " of:" << cmd_line );
	    ret = 0;
	    }
	else
	    y2err("retcode:" << cmd.retcode());
        }
    return( ret );
    }

int
Disk::execCheckFailed( const string& cmd_line )
    {
    static SystemCmd cmd;
    return( execCheckFailed( cmd, cmd_line ) );
    }

int Disk::execCheckFailed( SystemCmd& cmd, const string& cmd_line )
    {
    getStorage()->handleHald(true);
    cmd.execute( cmd_line );
    int ret = checkSystemError( cmd_line, cmd );
    if( ret!=0 )
	setExtError( cmd );
    getStorage()->handleHald(false);
    return( ret );
    }



bool
Disk::scanPartedLine( const string& Line, unsigned& nr, unsigned long& start,
                      unsigned long& csize, PartitionType& type, unsigned& id,
		      bool& boot ) const
    {
    unsigned long StartM, EndM;
    string PartitionType, TInfo;

    y2deb("Line:" << Line);
    std::istringstream Data( Line );
    classic(Data);

    nr=0;
    StartM = EndM = 0;
    type = PRIMARY;
    string skip;
    if( label == "msdos" )
	{
	Data >> nr >> StartM >> skip >> EndM >> skip >> skip >> PartitionType;
	}
    else
	{
	Data >> nr >> StartM >> skip >> EndM >> skip >> skip;
	}
    if (Data.fail())
	{
	y2mil( "invalid line:" << Line );
	nr = 0;
	}
    char c;
    TInfo = ",";
    Data.unsetf(ifstream::skipws);
    Data >> c;
    char last_char = ',';
    while( Data.good() && !Data.eof() )
	{
	if( !isspace(c) )
	    {
	    TInfo += c;
	    last_char = c;
	    }
	else
	    {
	    if( last_char != ',' )
		{
		TInfo += ",";
		last_char = ',';
		}
	    }
	Data >> c;
	}
    TInfo += ",";
    if( nr>0 )
	{
	y2mil("Fields Num:" << nr << " Start:" << StartM << " End:" << EndM << " Type:" << type);
	y2mil("TInfo:" << TInfo);
	start = StartM;
	csize = EndM-StartM+1;
	if( start+csize > cylinders() )
	    {
	    csize = cylinders()-start;
	    y2mil("new csize:" << csize);
	    }
	id = Partition::ID_LINUX;
	boot = TInfo.find( ",boot," ) != string::npos;
	string OrigTInfo = TInfo;
	boost::to_lower(TInfo, locale::classic());
	if( ext_possible )
	    {
	    if( PartitionType == "extended" )
		{
		type = EXTENDED;
		id = Partition::ID_EXTENDED;
		}
	    else if( nr>=5 )
		{
		type = LOGICAL;
		}
	    }
	else if( TInfo.find( ",fat" )!=string::npos )
	    {
	    id = Partition::ID_DOS;
	    }
	else if( TInfo.find( ",ntfs," )!=string::npos )
	    {
	    id = Partition::ID_NTFS;
	    }
	else if( TInfo.find( "swap," )!=string::npos )
	    {
	    id = Partition::ID_SWAP;
	    }
	else if( TInfo.find( ",raid," )!=string::npos )
	    {
	    id = Partition::ID_RAID;
	    }
	else if( TInfo.find( ",lvm," )!=string::npos )
	    {
	    id = Partition::ID_LVM;
	    }
	string::size_type pos = TInfo.find( ",type=" );
	if( pos != string::npos )
	    {
	    string val;
	    int tmp_id = 0;
	    if( label != "mac" )
		{
		val = TInfo.substr( pos+6, 2 );
		Data.clear();
		Data.str( val );
		Data >> std::hex >> tmp_id;
		y2deb("val=" << val << " id=" << tmp_id);
		if( tmp_id>0 )
		    {
		    id = tmp_id;
		    }
		}
	    else
		{
		pos = OrigTInfo.find("type=");
		val = OrigTInfo.substr( pos+5 );
		if( (pos=val.find_first_of( ", \t\n" )) != string::npos )
		    {
		    val = val.substr( 0, pos );
		    }
		if( id == Partition::ID_LINUX )
		    {
		    if( val.find( "Apple_HFS" ) != string::npos ||
			val.find( "Apple_Bootstrap" ) != string::npos )
			{
			id = Partition::ID_APPLE_HFS;
			}
		    else if( val.find( "Apple_partition" ) != string::npos ||
			val.find( "Apple_Driver" ) != string::npos ||
			val.find( "Apple_Loader" ) != string::npos ||
			val.find( "Apple_Boot" ) != string::npos ||
			val.find( "Apple_ProDOS" ) != string::npos ||
			val.find( "Apple_FWDriver" ) != string::npos ||
			val.find( "Apple_Patches" ) != string::npos )
			{
			id = Partition::ID_APPLE_OTHER;
			}
		    else if( val.find( "Apple_UFS" ) != string::npos )
			{
			id = Partition::ID_APPLE_UFS;
			}
		    }
		}
	    }
	if( label == "gpt" )
	    {
	    if( TInfo.find( ",boot," ) != string::npos &&
	        TInfo.find( ",fat" ) != string::npos )
		{
		id = Partition::ID_GPT_BOOT;
		}
	    if( TInfo.find( ",hp-service," ) != string::npos )
		{
		id = Partition::ID_GPT_SERVICE;
		}
	    if( TInfo.find( ",msftres," ) != string::npos )
		{
		id = Partition::ID_GPT_MSFTRES;
		}
	    if( TInfo.find( ",hfs+," ) != string::npos || 
		TInfo.find( ",hfs," ) != string::npos )
		{
		id = Partition::ID_APPLE_HFS;
		}
	    }
	y2mil("Fields Num:" << nr << " Id:" << id << " Ptype:" << type << " Start:" << start <<
	      " Size:" << csize);
	}
    return( nr>0 );
    }

bool
Disk::checkPartedOutput( const SystemCmd& Cmd, ProcPart& ppart )
    {
    int cnt;
    string line;
    string tmp;
    unsigned long range_exceed = 0;
    list<Partition *> pl;

    cnt = Cmd.numLines();
    for( int i=0; i<cnt; i++ )
	{
	unsigned pnr;
	unsigned long c_start;
	unsigned long c_size;
	PartitionType type;
	unsigned id;
	bool boot;

	line = Cmd.getLine(i);
	tmp = extractNthWord( 0, line );
	if( tmp.length()>0 && isdigit(tmp[0]) )
	    {
	    if( scanPartedLine( line, pnr, c_start, c_size, type, id, boot ))
		{
		if( pnr<range )
		    {
		    unsigned long long s = cylinderToKb(c_size);
		    Partition *p = new Partition( *this, pnr, s,
						  c_start, c_size, type,
						  id, boot );
		    if( ppart.getSize( p->device(), s ))
			{
			if( s>0 && p->type() != EXTENDED )
			    p->setSize( s );
			}
		    pl.push_back( p );
		    }
		else
		    range_exceed = max( range_exceed, (unsigned long)pnr );
		}
	    }
	}
    y2mil("nm:" << nm);
    if( !dmp_slave && !checkPartedValid( ppart, nm, pl, range_exceed ) )
	{
	string txt = sformat(
	// popup text %1$s is replaced by disk name e.g. /dev/hda
_("The partitioning on disk %1$s is not readable by\n"
"the partitioning tool parted, which is used to change the\n"
"partition table.\n"
"\n"
"You can use the partitions on disk %1$s as they are.\n"
"You can format them and assign mount points to them, but you\n"
"cannot add, edit, resize, or remove partitions from that\n"
"disk with this tool."), dev.c_str() );

	getStorage()->addInfoPopupText( dev, txt );
	ronly = true;
	}
    if( range_exceed>0 )
	{
	string txt = sformat(
	// popup text %1$s is replaced by disk name e.g. /dev/hda
	//            %2$lu and %3$lu are replaced by numbers.
_("Your disk %1$s contains %2$lu partitions. The maximum number\n"
"of partitions that the kernel driver of the disk can handle is %3$lu.\n"
"Partitions numbered above %3$lu cannot be accessed."),
                              (char*)dev.c_str(), range_exceed, range-1 );
	txt += "\n";
	txt += 
	// popup text
_("You have the following options:\n"
"  - Repartition your system so that only the maximal allowed number\n"
"    of partitions is used. To repartition, use your existing operating\n"
"    system.\n"
"  - Use LVM since it can provide an arbitrary and flexible\n"
"    number of block devices partitions. This needs a repartition\n"
"    as well.");
	getStorage()->addInfoPopupText( dev, txt );
	}
    for( list<Partition*>::iterator i=pl.begin(); i!=pl.end(); ++i )
	{
	addToList( *i );
	}
    return( true );
    }


bool
Disk::checkPartedValid(const ProcPart& pp, const string& diskname,
		       list<Partition*>& pl, unsigned long& range_exceed) const
{
    unsigned ext_nr = 0;
    bool ret=true;
    unsigned long long SizeK;
    map<unsigned,unsigned long> proc_l;
    map<unsigned,unsigned long> parted_l;
    for( list<Partition*>::const_iterator i=pl.begin(); i!=pl.end(); i++ )
	{
	if( (*i)->type()==EXTENDED )
	    ext_nr = (*i)->nr();
	else
	    {
	    parted_l[(*i)->nr()] = (*i)->cylSize();
	    }
	}
    string reg = diskname;
    if( !reg.empty() && reg.find( '/' )!=string::npos && 
        isdigit(reg[reg.length()-1]) )
	reg += "p";
    reg = "^" + reg + "[0-9]+" "$";
    list<string> ps = pp.getMatchingEntries(regex_matches(reg));
    y2mil("regex:\"" << reg << "\" ps:" << ps);
    for( list<string>::const_iterator i=ps.begin(); i!=ps.end(); i++ )
	{
	pair<string,unsigned> p = getDiskPartition( *i );
	if( p.second>0 && p.second!=ext_nr &&
	    pp.getSize(*i, SizeK))
	    {
	    proc_l[p.second] = kbToCylinder( SizeK );
	    }
	}
    y2mil("proc  :" << proc_l);
    y2mil("parted:" << parted_l);
    if( proc_l.size()>=parted_l.size() && !parted_l.empty() )
	{
	map<unsigned,unsigned long>::const_iterator i, j;
	for( i=proc_l.begin(); i!=proc_l.end(); i++ )
	    {
	    j=parted_l.find(i->first);
	    if( j!=parted_l.end() )
		{
		ret = ret && (abs((long)i->second-(long)j->second)<=2 ||
		              abs((long)i->second-(long)j->second)<(long)j->second/100);
		}
	    }
	for( i=parted_l.begin(); i!=parted_l.end(); i++ )
	    {
	    j=proc_l.find(i->first);
	    if( j==proc_l.end() )
		ret = false;
	    else
		{
		ret = ret && (abs((long)i->second-(long)j->second)<=2 ||
		              abs((long)i->second-(long)j->second)<(long)j->second/100);
		}
	    }
	}
    else
	{
	ret = parted_l.empty() && proc_l.empty();
	}
    if( !ret || label=="unsupported" )
	{
	range_exceed = 0;
	for( list<Partition*>::iterator i=pl.begin(); i!=pl.end(); i++ )
	    {
	    delete *i;
	    }
	pl.clear();
	unsigned cyl_start = 1;
	for( list<string>::const_iterator i=ps.begin(); i!=ps.end(); i++ )
	    {
	    unsigned long cyl;
	    unsigned long long s;
	    pair<string,unsigned> pr = getDiskPartition( *i );
	    if( pp.getSize( *i, s ))
		{
		cyl = kbToCylinder(s);
		if( pr.second!=0 && pr.second < range )
		    {
		    unsigned id = Partition::ID_LINUX;
		    PartitionType type = PRIMARY;
		    if( ext_possible )
			{
			if( s==1 )
			    {
			    type = EXTENDED;
			    id = Partition::ID_EXTENDED;
			    }
			if( pr.second>max_primary )
			    {
			    type = LOGICAL;
			    }
			}
		    Partition *p =
			new Partition( *this, pr.second, s, cyl_start, cyl,
			               type, id, false );
		    pl.push_back( p );
		    }
		else if( pr.second>0 )
		    range_exceed = max( range_exceed, (unsigned long)pr.second );
		cyl_start += cyl;
		}
	    }
	}
    y2mil("pr.size:" << proc_l.size() << " pa.size:" << parted_l.size());
    y2mil("ret:" << ret);
    return ret;
}


// note: factors are in the context of kilo
#define TB (1024ULL * 1024ULL * 1024ULL)
#define PB (1024ULL * 1024ULL * 1024ULL * 1024ULL)

string
Disk::defaultLabel(bool efiboot, unsigned long long size_k)
{
    string ret = "msdos";
    if (efiboot)
	ret = "gpt";
    else if( Storage::arch()=="ia64" )
	ret = "gpt";
    else if( Storage::arch()=="sparc" )
	ret = "sun";
    else if( Storage::arch()=="ppc" && Storage::isPPCMac() )
	ret = "mac";
    else if( Storage::arch()=="ppc" && Storage::isPPCPegasos() )
	ret = "amiga";
    if( size_k>2*TB )
	ret = "gpt";
    y2mil("efiboot:" << efiboot << " size_k:" << size_k << " ret:" << ret);
    return ret;
}

Disk::label_info Disk::labels[] = {
	{ "msdos", true, 4, 63, 2*TB },
	{ "gpt", false, 128, 0, 16*PB },
	{ "bsd", false, 8, 0, 2*TB },
	{ "sun", false, 8, 0, 2*TB },
	{ "mac", false, 64, 0, 2*TB },
	{ "dasd", false, 3, 0, 2*TB },
	{ "aix", false, 0, 0, 2*TB },
	{ "amiga", false, 63, 0, 2*TB },
	{ "", false, 0, 0, 0 }
    };

#undef TB
#undef PB


string Disk::p_disks [] = { "cciss/", "ida/", "ataraid/", "etherd/", "rd/" };

bool Disk::needP( const string& disk )
    {
    bool need_p = false;
    unsigned i=0;
    while( !need_p && i<lengthof(p_disks) )
	{
	string::size_type p = disk.find(p_disks[i]);
	if( p==0 || (p==5 && disk.find( "/dev/" )==0 ))
	    {
	    need_p = true;
	    }
	i++;
	}
    return( need_p );
    }


string
Disk::partNaming(const string& disk)
{
    // TODO: this is hackish
    if (boost::starts_with(disk, "/dev/mapper/"))
	return "_part";
    else if (needP(disk))
	return "p";
    else
	return "";
}


string Disk::getPartName( const string& disk, unsigned nr )
    {
    return( disk + (Disk::needP(disk)?"p":"") + decString(nr) );
    }

string Disk::getPartName( const string& disk, const string& nr )
    {
    return( disk + "/" + nr );
    }

string Disk::getPartName( unsigned nr ) const
    {
    return( getPartName( dev, nr ) );
    }

pair<string,unsigned> Disk::getDiskPartition( const string& dev )
    {
    static Regex prx( "[0123456789]p[0123456789]+$" );
    static Regex partrx( "[-_]part[0123456789]+$" );
    unsigned nr = 0;
    string disk = dev;
    bool need_p = prx.match( dev );
    bool part = partrx.match( dev );
    string::size_type p = dev.find_last_not_of( "0123456789" );
    if( p != string::npos && p<dev.size() && (!need_p||dev[p]=='p') &&
	(dev.find("/disk/by-")==string::npos||part) )
	{
	dev.substr(p+1) >> nr;
	unsigned pos = p+1;
	if( need_p )
	    pos--;
	else if( part )
	    pos -= 5;
	disk = dev.substr( 0, pos );
	}
    y2mil( "dev:" << dev << " disk:" << disk << " nr:" << nr );
    return( make_pair<string,unsigned>(disk,nr) );
    }


static bool notDeletedPri( const Partition& p )
{
    return !p.deleted() && p.type()==PRIMARY;
}

static bool notDeletedExt( const Partition& p )
{
    return !p.deleted() && p.type()==EXTENDED;
}

static bool notDeletedLog( const Partition& p )
{
    return !p.deleted() && p.type()==LOGICAL;
}


unsigned int Disk::numPrimary() const
{
    return partPair(notDeletedPri).length();
}

bool Disk::hasExtended() const
{
    return ext_possible && !partPair(notDeletedExt).empty();
}

unsigned int Disk::numLogical() const
{
    return partPair(notDeletedLog).length();
}


unsigned
Disk::availablePartNumber(PartitionType type) const
{
    y2mil("begin name:" << name() << " type:" << type);
    unsigned ret = 0;
    ConstPartPair p = partPair( notDeleted );
    if( !ext_possible && type==LOGICAL )
	{
	ret = 0;
	}
    else if( p.empty() )
	{
	if( type==LOGICAL )
	    ret = max_primary + 1;
	else
	    ret = label!="mac" ? 1 : 2;
	}
    else if( type==LOGICAL )
	{
	unsigned mx = max_primary;
	ConstPartIter i=p.begin();
	while( i!=p.end() )
	    {
	    y2mil( "i:" << *i );
	    if( i->nr()>mx )
		mx = i->nr();
	    ++i;
	    }
	ret = mx+1;
	if( !ext_possible || !hasExtended() || ret>max_logical )
	    ret = 0;
	}
    else
	{
	ConstPartIter i=p.begin();
	unsigned start = label!="mac" ? 1 : 2;
	while( i!=p.end() && i->nr()<=start && i->nr()<=max_primary )
	    {
	    if( i->nr()==start )
		start++;
	    if( label=="sun" && start==3 )
		start++;
	    ++i;
	    }
	if( start<=max_primary )
	    ret = start;
	if( type==EXTENDED && (!ext_possible || hasExtended()))
	    ret = 0;
	}

    if( ret >= range )
	ret = 0;

    y2mil("ret:" << ret);
    return ret;
}


static bool notDeletedNotLog( const Partition& p )
    {
    return( !p.deleted() && p.type()!=LOGICAL );
    }

static bool existingNotLog( const Partition& p )
    {
    return( !p.deleted() && !p.created() && p.type()!=LOGICAL );
    }

static bool existingLog( const Partition& p )
    {
    return( !p.deleted() && !p.created() && p.type()==LOGICAL );
    }

static bool notCreatedPrimary( const Partition& p )
    {
    return( !p.created() && p.type()==PRIMARY );
    }


void
Disk::getUnusedSpace(list<Region>& free, bool all, bool logical) const
{
    y2mil("all:" << all << " logical:" << logical);

    free.clear();

    if (all || !logical)
    {
	ConstPartPair p = partPair(notDeletedNotLog);

	unsigned long start = 1;
	unsigned long end = cylinders();

	list<Region> tmp;
	for (ConstPartIter i = p.begin(); i != p.end(); ++i)
	    tmp.push_back(Region(i->cylStart(), i->cylEnd() - i->cylStart() + 1));
	tmp.sort();

	for (list<Region>::const_iterator i = tmp.begin(); i != tmp.end(); ++i)
	{
	    if (i->start() > start)
		free.push_back(Region(start, i->start() - start));
	    start = i->end() + 1;
	}
	if (end > start)
	    free.push_back(Region(start, end - start));
    }

    if (all || logical)
    {
	ConstPartPair ext = partPair(notDeletedExt);
	if (!ext.empty())
	{
	    ConstPartPair p = partPair(notDeletedLog);

	    unsigned long start = ext.begin()->cylStart();
	    unsigned long end = ext.begin()->cylEnd();

	    list<Region> tmp;
	    for (ConstPartIter i = p.begin(); i != p.end(); ++i)
		tmp.push_back(Region(i->cylStart(), i->cylEnd() - i->cylStart() + 1));
	    tmp.sort();

	    for (list<Region>::const_iterator i = tmp.begin(); i != tmp.end(); ++i)
	    {
		if (i->start() > start)
		    free.push_back(Region(start, i->start() - start));
		start = i->end() + 1;
	    }
	    if (end > start)
		free.push_back(Region(start, end - start));
	}
    }

    y2deb("free:" << free);
}


static bool regions_sort_size( const Region& rhs, const Region& lhs )
    {
    return( rhs.len()>lhs.len() );
    }

int Disk::createPartition( unsigned long cylLen, string& device,
			   bool checkRelaxed )
    {
    y2mil("len:" << cylLen << " relaxed:" << checkRelaxed);
    getStorage()->logCo( this );
    int ret = 0;
    list<Region> free;
    getUnusedSpace( free );
    y2mil("free:");
    if( !free.empty() )
	{
	free.sort( regions_sort_size );
	list<Region>::iterator i = free.begin();
	while( i!=free.end() && i->len()>=cylLen )
	    ++i;
	--i;
	if( i->len()>=cylLen )
	    {
	    PartPair ext = partPair(notDeletedExt);
	    PartitionType t = PRIMARY;
	    bool usable = false;
	    do
		{
		t = PRIMARY;
		if( !ext.empty() && ext.begin()->contains( *i ) )
		    t = LOGICAL;
		usable = availablePartNumber(t)>0;
		if( !usable && i!=free.begin() )
		    --i;
		}
	    while( i!=free.begin() && !usable );
	    usable = availablePartNumber(t)>0;
	    if( usable )
		ret = createPartition( t, i->start(), cylLen, device,
				       checkRelaxed );
	    else
		ret = DISK_PARTITION_NO_FREE_NUMBER;
	    }
	else
	    ret = DISK_CREATE_PARTITION_NO_SPACE;
	}
    else
	ret = DISK_CREATE_PARTITION_NO_SPACE;
    getStorage()->logCo( this );
    y2mil("ret:" << ret);
    return( ret );
    }

int Disk::createPartition( PartitionType type, string& device )
    {
    y2mil("type " << type);
    int ret = 0;
    list<Region> free;
    getUnusedSpace( free, type==PTYPE_ANY, type==LOGICAL );
    if( !free.empty() )
	{
	free.sort( regions_sort_size );
	list<Region>::iterator i = free.begin();
	PartPair ext = partPair(notDeletedExt);
	PartitionType t = type;
	bool usable = false;
	do
	    {
	    t = PRIMARY;
	    if( !ext.empty() && ext.begin()->contains( *i ) )
		t = LOGICAL;
	    usable = t==type || type==PTYPE_ANY || (t==PRIMARY&&type==EXTENDED);
	    usable = usable && availablePartNumber(t)>0;
	    if( !usable && i!=free.begin() )
		--i;
	    }
	while( i!=free.begin() && !usable );
	usable = availablePartNumber(t)>0;
	if( usable )
	    ret = createPartition( type==PTYPE_ANY?t:type, i->start(),
	                           i->len(), device, true );
	else
	    ret = DISK_PARTITION_NO_FREE_NUMBER;
	}
    else
	ret = DISK_CREATE_PARTITION_NO_SPACE;
    y2mil("ret:" << ret);
    return( ret );
    }


int
Disk::nextFreePartition(PartitionType type, unsigned& nr, string& device) const
{
    int ret = 0;
    device = "";
    nr = 0;
    unsigned number = availablePartNumber( type );
    if( number==0 )
	{
	ret = DISK_PARTITION_NO_FREE_NUMBER;
	}
    else
	{
	Partition * p = new Partition( *this, number, 0, 0, 1, type );
	device = p->device();
	nr = p->nr();
	delete( p );
	}
    y2mil("ret:" << ret << " nr:" << nr << " device:" << device);
    return ret;
}


int Disk::createPartition( PartitionType type, unsigned long start,
                           unsigned long len, string& device,
			   bool checkRelaxed )
    {
    y2mil("begin type " << type << " at " << start << " len " << len << " relaxed:" << checkRelaxed);
    getStorage()->logCo( this );
    int ret = createChecks( type, start, len, checkRelaxed );
    int number = 0;
    if( ret==0 )
	{
	number = availablePartNumber( type );
	if( number==0 )
	    {
	    ret = DISK_PARTITION_NO_FREE_NUMBER;
	    }
	}
    if( ret==0 )
	{
	if( label=="sun" && start==0 )
	    start=1;
	Partition * p = new Partition( *this, number, cylinderToKb(len), start,
	                               len, type );
	PartPair pp = partPair();
	PartIter i=pp.begin();
	while( i!=pp.end() && !(i->deleted() && i->cylStart()==start) )
	    ++i;
	if( i!=pp.end() )
	    {
	    y2mil( "deleted at same start:" << *i );
	    p->getFsInfo( &(*i) );
	    }
	p->setCreated();
	device = p->device();
	addToList( p );
	}
    getStorage()->logCo( this );
    y2mil("ret:" << ret << " device:" << (ret==0?device:""));
    return( ret );
    }

int Disk::createChecks( PartitionType& type, unsigned long start,
                        unsigned long len, bool checkRelaxed ) const
    {
    y2mil("begin type " << type << " at " << start << " len " << len << " relaxed:" << checkRelaxed);
    unsigned fuzz = checkRelaxed ? 2 : 0;
    int ret = 0;
    Region r( start, len );
    ConstPartPair ext = partPair(notDeletedExt);
    if( type==PTYPE_ANY )
	{
	if( ext.empty() || !ext.begin()->contains( Region(start,1) ))
	    type = PRIMARY;
	else
	    type = LOGICAL;
	}

    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    if( ret==0 && (r.end() > cylinders()+fuzz) )
	{
	y2mil("too large for disk cylinders " << cylinders());
	ret = DISK_PARTITION_EXCEEDS_DISK;
	}
    if( ret==0 && len==0 )
	{
	ret = DISK_PARTITION_ZERO_SIZE;
	}
    if( ret==0 && type==LOGICAL && ext.empty() )
	{
	ret = DISK_CREATE_PARTITION_LOGICAL_NO_EXT;
	}
    if( ret==0 )
	{
	ConstPartPair p = (type != LOGICAL) ? partPair(notDeleted) : partPair(notDeletedLog);
	ConstPartIter i = p.begin();
	while( i!=p.end() && !i->intersectArea( r, fuzz ))
	    {
	    ++i;
	    }
	if( i!=p.end() )
	    {
	    y2war( "overlaps r:" << r << " p:" << i->region() <<
	           " inter:" << i->region().intersect(r) );
	    ret = DISK_PARTITION_OVERLAPS_EXISTING;
	    }
	}
    if( ret==0 && type==LOGICAL && !ext.begin()->contains( r, fuzz ))
	{
	y2war( "outside ext r:" <<  r << " ext:" << ext.begin()->region() <<
	       "inter:" << ext.begin()->region().intersect(r) );
	ret = DISK_PARTITION_LOGICAL_OUTSIDE_EXT;
	}
    if( ret==0 && type==EXTENDED )
	{
	if( !ext_possible || !ext.empty())
	    {
	    ret = ext_possible ? DISK_CREATE_PARTITION_EXT_ONLY_ONCE
	                       : DISK_CREATE_PARTITION_EXT_IMPOSSIBLE;
	    }
	}
    y2mil("ret:" << ret);
    return ret;
    }

int Disk::changePartitionArea( unsigned nr, unsigned long start,
			       unsigned long len, bool checkRelaxed )
    {
    y2mil("begin nr " << nr << " at " << start << " len " << len << " relaxed:" << checkRelaxed);
    int ret = 0;
    Region r( start, len );
    unsigned fuzz = checkRelaxed ? 2 : 0;
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    PartPair p = partPair( notDeleted );
    PartIter part = p.begin();
    while( ret==0 && part!=p.end() && part->nr()!=nr)
	{
	++part;
	}
    if( ret==0 && part==p.end() )
	{
	ret = DISK_PARTITION_NOT_FOUND;
	}
    if( ret==0 && r.end() > cylinders()+fuzz )
	{
	y2mil("too large for disk cylinders " << cylinders());
	ret = DISK_PARTITION_EXCEEDS_DISK;
	}
    if( ret==0 && len==0 )
	{
	ret = DISK_PARTITION_ZERO_SIZE;
	}
    if( ret==0 && part->type()==LOGICAL )
	{
	PartPair ext = partPair(notDeletedExt);
	p = partPair( notDeletedLog );
	PartIter i = p.begin();
	while( i!=p.end() && (i==part||!i->intersectArea( r, fuzz )) )
	    {
	    ++i;
	    }
	if( i!=p.end() )
	    {
	    y2war( "overlaps r:" << r << " p:" << i->region() <<
	           " inter:" << i->region().intersect(r) );
	    ret = DISK_PARTITION_OVERLAPS_EXISTING;
	    }
	if( ret==0 && !ext.begin()->contains( r, fuzz ))
	    {
	    y2war( "outside ext r:" <<  r << " ext:" << ext.begin()->region() <<
		   "inter:" << ext.begin()->region().intersect(r) );
	    ret = DISK_PARTITION_LOGICAL_OUTSIDE_EXT;
	    }
	}
    if( ret==0 && part->type()!=LOGICAL )
	{
	PartIter i = p.begin();
	while( i!=p.end() &&
	       (i==part || i->nr()>max_primary || !i->intersectArea( r, fuzz )))
	    {
	    ++i;
	    }
	if( i!=p.end() )
	    {
	    y2war( "overlaps r:" << r << " p:" << i->region() <<
	           " inter:" << i->region().intersect(r) );
	    ret = DISK_PARTITION_OVERLAPS_EXISTING;
	    }
	}
    if( ret==0 )
	{
	part->changeRegion( start, len, cylinderToKb(len) );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

static bool volume_ptr_sort_nr( Partition*& rhs, Partition*& lhs )
    {
    return( rhs->nr()<lhs->nr() );
    }

int Disk::removePartition( unsigned nr )
    {
    y2mil("begin nr " << nr);
    getStorage()->logCo( this );
    int ret = 0;
    PartPair p = partPair( notDeleted );
    PartIter i = p.begin();
    while( i!=p.end() && i->nr()!=nr)
	{
	++i;
	}
    if( i==p.end() )
	{
	ret = DISK_PARTITION_NOT_FOUND;
	}
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    else if( i->getUsedByType() != UB_NONE )
	{
	ret = DISK_REMOVE_USED_BY;
	}
    if( ret==0 )
	{
	PartitionType t = i->type();
	if( i->created() )
	    {
	    if( !removeFromList( &(*i) ))
		ret = DISK_REMOVE_PARTITION_CREATE_NOT_FOUND;
	    p = partPair( notDeleted );
	    }
	else
	    i->setDeleted();
	if( ret==0 && nr>max_primary )
	    {
	    changeNumbers( p.begin(), p.end(), nr, -1 );
	    }
	else if( ret==0 && nr<=max_primary )
	    {
	    list<Partition*> l;
	    i = p.begin();
	    while( i!=p.end() )
		{
		if( i->created() && i->nr()<=max_primary && i->nr()>nr )
		    {
		    l.push_back( &(*i) );
		    }
		++i;
		}
	    if( !l.empty() )
		{
		l.sort( volume_ptr_sort_nr );
		unsigned old = nr;
		list<Partition*>::iterator vi = l.begin();
		while( vi!=l.end() )
		    {
		    unsigned save = (*vi)->nr();
		    (*vi)->changeNumber( old );
		    old = save;
		    ++vi;
		    }
		}
	    }
	if( t==EXTENDED )
	    {
	    list<Volume*> l;
	    i = p.begin();
	    while( i!=p.end() )
		{
		if( i->nr()>max_primary )
		    {
		    if( i->created() )
			l.push_back( &(*i) );
		    else
			i->setDeleted();
		    }
		++i;
		}
	    list<Volume*>::iterator vi = l.begin();
	    while( ret==0 && vi!=l.end() )
		{
		if( !removeFromList( *vi ))
		    ret = DISK_PARTITION_NOT_FOUND;
		++vi;
		}
	    }
	}
    getStorage()->logCo( this );
    y2mil("ret:" << ret);
    return( ret );
    }

void Disk::changeNumbers( const PartIter& b, const PartIter& e,
                          unsigned start, int incr )
    {
    y2mil("start:" << start << " incr:" << incr);
    PartIter i(b);
    while( i!=e )
	{
	if( i->nr()>start )
	    {
	    i->changeNumber( i->nr()+incr );
	    }
	++i;
	}
    }

int Disk::destroyPartitionTable( const string& new_label )
    {
    y2mil("begin");
    int ret = 0;
    setLabelData( new_label );
    if( max_primary==0 )
	{
	setLabelData( label );
	ret = DISK_DESTROY_TABLE_INVALID_LABEL;
	}
    else
	{
	label = new_label;
	VIter j = vols.begin();
	while( j!=vols.end() )
	    {
	    if( (*j)->created() )
		{
		delete( *j );
		j = vols.erase( j );
		}
	    else
		++j;
	    }
	bool save = getStorage()->getRecursiveRemoval();
	getStorage()->setRecursiveRemoval(true);
	if( getUsedByType() != UB_NONE )
	    {
	    getStorage()->removeUsing( device(), getUsedBy() );
	    }
	ronly = false;
	RVIter i = vols.rbegin();
	while( i!=vols.rend() && !dmp_slave )
	    {
	    if( !(*i)->deleted() )
		getStorage()->removeVolume( (*i)->device() );
	    ++i;
	    }
	getStorage()->setRecursiveRemoval(save);
	setDeleted();
	}
    y2mil("ret:" << ret);
    return( ret );
    }


int
Disk::changePartitionId(unsigned nr, unsigned id)
{
    y2mil("begin nr:" << nr << " id:" << hex << id);
    int ret = 0;
    PartPair p = partPair( notDeleted );
    PartIter i = p.begin();
    while( i!=p.end() && i->nr()!=nr)
	{
	++i;
	}
    if( i==p.end() )
	{
	ret = DISK_PARTITION_NOT_FOUND;
	}
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	ret = i->changeId( id );
	}
    y2mil("ret:" << ret);
    return ret;
}


int Disk::forgetChangePartitionId( unsigned nr )
    {
    y2mil("begin nr:" << nr);
    int ret = 0;
    PartPair p = partPair( notDeleted );
    PartIter i = p.begin();
    while( i!=p.end() && i->nr()!=nr)
	{
	++i;
	}
    if( i==p.end() )
	{
	ret = DISK_PARTITION_NOT_FOUND;
	}
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	i->unChangeId();
	}
    y2mil("ret:" << ret);
    return( ret );
    }


void
Disk::getToCommit(CommitStage stage, list<const Container*>& col, list<const Volume*>& vol) const
    {
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    Container::getToCommit( stage, col, vol );
    if( stage==INCREASE )
	{
	ConstPartPair p = partPair( Partition::toChangeId );
	for( ConstPartIter i=p.begin(); i!=p.end(); ++i )
	    if( find( vol.begin(), vol.end(), &(*i) )==vol.end() )
		vol.push_back( &(*i) );
	}
    if( col.size()!=oco || vol.size()!=ovo )
	y2mil("stage:" << stage << " col:" << col.size() << " vol:" << vol.size());
    }

int Disk::commitChanges( CommitStage stage, Volume* vol )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = Container::commitChanges( stage, vol );
    if( ret==0 && stage==INCREASE )
	{
	Partition * p = dynamic_cast<Partition *>(vol);
	if( p!=NULL )
	    {
	    if( Partition::toChangeId( *p ) )
		ret = doSetType( p );
	    }
	else
	    ret = DISK_SET_TYPE_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Disk::commitChanges( CommitStage stage )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = 0;
    if( stage==DECREASE && deleted() )
	{
	ret = doCreateLabel();
	}
    else
	ret = DISK_COMMIT_NOTHING_TODO;
    y2mil("ret:" << ret);
    return( ret );
    }


void
Disk::getCommitActions(list<commitAction>& l) const
    {
    Container::getCommitActions( l );
    if( deleted() )
	{
	l.remove_if(stage_is(DECREASE));
	l.push_front(commitAction(DECREASE, staticType(), setDiskLabelText(false), this, true));
	}
    }


string Disk::setDiskLabelText( bool doing ) const
    {
    string txt;
    string d = dev;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by disk name (e.g. /dev/hda),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Setting disk label of disk %1$s to %2$s"),
		       d.c_str(), label.c_str() );
        }
    else
        {
        // displayed text before action, %1$s is replaced by disk name (e.g. /dev/hda),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Set disk label of disk %1$s to %2$s"),
		      d.c_str(), label.c_str() );
        }
    return( txt );
    }

int Disk::doCreateLabel()
    {
    y2mil("label:" << label);
    int ret = 0;
    if( !silent )
	{
	getStorage()->showInfoCb( setDiskLabelText(true) );
	}
    if( !dmp_slave )
	getStorage()->removeDmMapsTo( device() );
    removePresentPartitions();
    system_stderr.erase();
    std::ostringstream cmd_line;
    classic(cmd_line);
    cmd_line << PARTEDCMD << quote(device()) << " mklabel " << label;
    if( execCheckFailed( cmd_line.str() ) )
	{
	ret = DISK_SET_LABEL_PARTED_FAILED;
	}
    else
	{
	setDeleted(false);
	removeFromMemory();
	}
    if( ret==0 )
	{
	if( !dmp_slave )
	    Storage::waitForDevice();
	redetectGeometry();
	}
    gpt_enlarge = false;
    y2mil("ret:" << ret);
    return( ret );
    }

void Disk::removePresentPartitions()
    {
    VolPair p = volPair();
    if( !p.empty() )
	{
	bool save=silent;
	setSilent( true );
	list<VolIterator> l;
	for( VolIterator i=p.begin(); i!=p.end(); ++i )
	    {
	    y2mil( "rem:" << *i );
	    if( !i->created() )
		l.push_front( i );
	    }
	for( list<VolIterator>::const_iterator i=l.begin(); i!=l.end(); ++i )
	    {
	    doRemove( &(**i) );
	    }
	setSilent( save );
	}
    }

void Disk::removeFromMemory()
    {
    VIter i = vols.begin();
    while( i!=vols.end() )
	{
	y2mil( "rem:" << *i );
	if( !(*i)->created() )
	    {
	    delete *i;
	    i = vols.erase( i );
	    }
	else
	    ++i;
	}
    }

void Disk::redetectGeometry()
    {
    string cmd_line = PARTEDCMD + quote(device()) + " unit cyl print";
    y2mil("executing cmd:" << cmd_line);
    SystemCmd Cmd( cmd_line );
    if( Cmd.select( "BIOS cylinder" )>0 )
	{
	unsigned long c;
	unsigned h;
	unsigned s;
	string tmp = Cmd.getLine(0, true);
	getGeometry( tmp, c, h, s );
	if( c!=0 && c!=cyl )
	    {
	    new_cyl = c;
	    new_head = h;
	    new_sector = s;
	    y2mil("new parted geometry Head:" << new_head << " Sector:" << new_sector <<
		  " Cylinder:" << new_cyl);
	    }
	}
    }

int Disk::doSetType( Volume* v )
    {
    y2mil("doSetType container " << name() << " name " << v->name());
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->setTypeText(true) );
	    }
	if( p->id()!=Partition::ID_LINUX && p->id()!=Partition::ID_SWAP )
	    p->eraseLabel();
	system_stderr.erase();
	std::ostringstream cmd_line;
	classic(cmd_line);
	cmd_line << PARTEDCMD << quote(device()) << " set " << p->nr() << " ";
	string start_cmd = cmd_line.str();
	if( ret==0 )
	    {
	    cmd_line.str( start_cmd );
	    cmd_line.seekp(0, ios_base::end );
	    cmd_line << "lvm " << (p->id()==Partition::ID_LVM ? "on" : "off");
	    if( execCheckFailed( cmd_line.str() ) && !dmp_slave )
		{
		ret = DISK_SET_TYPE_PARTED_FAILED;
		}
	    }
	if( ret==0 && (label!="sun"))
	    {
	    cmd_line.str( start_cmd );
	    cmd_line.seekp(0, ios_base::end );
	    cmd_line << "raid " << (p->id()==Partition::ID_RAID?"on":"off");
	    if( execCheckFailed( cmd_line.str() ) && !dmp_slave )
		{
		ret = DISK_SET_TYPE_PARTED_FAILED;
		}
	    }
	if( ret==0 && (label=="dvh"||label=="mac"))
	    {
	    cmd_line.str( start_cmd );
	    cmd_line.seekp(0, ios_base::end );
	    cmd_line << "swap " << (p->id()==Partition::ID_SWAP?"on":"off");
	    if( execCheckFailed( cmd_line.str() ) && !dmp_slave )
		{
		ret = DISK_SET_TYPE_PARTED_FAILED;
		}
	    }
	if( ret==0 )
	    {
	    cmd_line.str( start_cmd );
	    cmd_line.seekp(0, ios_base::end );
	    cmd_line << "boot " <<
		     ((p->boot()||p->id()==Partition::ID_GPT_BOOT)?"on":"off");
	    if( execCheckFailed( cmd_line.str() ) && !dmp_slave )
		{
		ret = DISK_SET_TYPE_PARTED_FAILED;
		}
	    }
	if( ret==0 && p->id()<=255 && label=="msdos" )
	    {
	    cmd_line.str( start_cmd );
	    cmd_line.seekp(0, ios_base::end );
	    cmd_line << "type " << p->id();
	    if( execCheckFailed( cmd_line.str() ) && !dmp_slave )
		{
		ret = DISK_SET_TYPE_PARTED_FAILED;
		}
	    }
	if( ret==0 )
	    {
	    if( !dmp_slave )
		Storage::waitForDevice(p->device());
	    p->changeIdDone();
	    }
	}
    else
	{
	ret = DISK_SET_TYPE_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

bool
Disk::getPartedValues( Partition *p ) const
    {
    bool ret = false;
    if (getStorage()->testmode())
	{
	ret = true;
	p->setSize( p->sizeK() );
	}
    else
	{
	ProcPart ppart;
	std::ostringstream cmd_line;
	classic(cmd_line);
	cmd_line << PARTEDCMD << quote(device()) << " unit cyl print | grep -w \"^[ \t]*\"" << p->nr();
	SystemCmd cmd( cmd_line.str() );
	unsigned nr, id;
	unsigned long start, csize;
	PartitionType type;
	bool boot;
	if( cmd.numLines()>0 &&
	    scanPartedLine( cmd.getLine(0), nr, start, csize, type, id, boot ))
	    {
	    y2mil("really created at cyl:" << start << " csize:" << csize);
	    p->changeRegion( start, csize, cylinderToKb(csize) );
	    unsigned long long s=0;
	    ret = true;
	    if( !dmp_slave && p->type() != EXTENDED )
		{
		if( !ppart.getSize( p->device(), s ) || s==0 )
		    {
		    y2err("device " << p->device() << " not found in /proc/partitions");
		    ret = false;
		    }
		else
		    p->setSize( s );
		}
	    }
	cmd_line.str("");
	cmd_line << PARTEDCMD << quote(device()) << " unit cyl print";
	cmd.execute( cmd_line.str() );
	}
    return( ret );
    }

bool
Disk::getPartedSectors( const Partition *p, unsigned long long& start,
                        unsigned long long& end ) const
    {
    bool ret = false;
    if (getStorage()->testmode())
	{
	ret = true;
	start = p->cylStart()*new_head*new_sector;
	end = (p->cylEnd()+1)*new_head*new_sector-1;
	}
    else
	{
	std::ostringstream cmd_line;
	classic(cmd_line);
	cmd_line << PARTEDCMD << quote(device()) << " unit s print | grep -w \"^[ \t]*\"" << p->nr();
	SystemCmd cmd( cmd_line.str() );
	if( cmd.numLines()>0 )
	    {
	    string dummy, s1, s2;
	    std::istringstream data( cmd.getLine(0) );
	    classic(data);
	    data >> dummy >> s1 >> s2;
	    y2mil("dummy:\"" << dummy << "\" s1:\"" << s1 << "\" s2:\"" << s2 << "\"");
	    start = end = 0;
	    s1 >> start;
	    s2 >> end;
	    y2mil("start:" << start << " end:" << end);
	    ret = end>0;
	    }
	}
    return( ret );
    }

void Disk::enlargeGpt()
    {
    y2mil( "gpt_enlarge:" << gpt_enlarge );
    if( gpt_enlarge )
	{
	string cmd_line( "yes Fix | " PARTEDBIN );
	cmd_line += " ---pretend-input-tty ";
	cmd_line += quote(device());
	cmd_line += " print ";
	SystemCmd cmd( cmd_line );
	gpt_enlarge = false;
	}
    }

static bool logicalCreated( const Partition& p )
    { return( p.type()==LOGICAL && p.created() ); }


int Disk::doCreate( Volume* v )
    {
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	bool call_blockdev = false;
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->createText(true) );
	    }
	system_stderr.erase();
	y2mil("doCreate container " << name() << " name " << p->name());
	y2mil("doCreate nr:" << p->nr() << " start " << p->cylStart() << " len " << p->cylSize());
	y2mil("doCreate detected_label:" << detected_label << " label:" << label);
	if( detected_label != label )
	    {
	    ret = doCreateLabel();
	    if( ret==0 )
		detected_label = label;
	    }
	if( ret==0 && gpt_enlarge )
	    {
	    enlargeGpt();
	    }
	std::ostringstream cmd_line;
	classic(cmd_line);
	if( ret==0 )
	    {
	    cmd_line << PARTEDCMD << quote(device()) << " unit cyl mkpart ";
	    if( label != "sun" )
		{
		switch( p->type() )
		    {
		    case LOGICAL:
			cmd_line << "logical ";
			break;
		    case PRIMARY:
			cmd_line << "primary ";
			break;
		    case EXTENDED:
			cmd_line << "extended ";
			break;
		    default:
			ret = DISK_CREATE_PARTITION_INVALID_TYPE;
			break;
		    }
		}
	    }
	if( ret==0 && p->type()!=EXTENDED )
	    {
	    if( p->id()==Partition::ID_SWAP )
		{
		cmd_line << "linux-swap ";
		}
	    else if( p->id()==Partition::ID_GPT_BOOT ||
		     p->id()==Partition::ID_DOS16 ||
	             p->id()==Partition::ID_DOS )
	        {
		cmd_line << "fat32 ";
		}
	    else if( p->id()==Partition::ID_APPLE_HFS )
		{
		cmd_line << "hfs ";
		}
	    else
		{
		cmd_line << "ext2 ";
		}
	    }
	if( ret==0 )
	    {
	    unsigned long start = p->cylStart();
	    unsigned long end = p->cylStart()+p->cylSize();
	    PartPair pp = (p->type()!=LOGICAL) ? partPair( existingNotLog )
					       : partPair( existingLog );
	    unsigned long maxc = cylinders();
	    if( p->type()==LOGICAL )
		{
		PartPair ext = partPair(notDeletedExt);
		if( !ext.empty() )
		    maxc = ext.begin()->cylEnd();
		}
	    y2mil("max " << maxc << " end:" << end);
	    y2mil("pp " << *p);
	    for( PartIter i=pp.begin(); i!=pp.end(); ++i )
		{
		y2mil( "i " << *i );
		if( i->cylStart()<maxc && i->cylStart()<end &&
		    i->cylEnd()>p->cylStart() )
		    {
		    maxc=i->cylStart();
		    y2mil( "new maxc " << maxc );
		    }
		}
	    y2mil("max " << maxc);
	    if( new_cyl!=cyl )
		{
		y2mil("parted geometry changed old c:" << cyl << " h:" << head << " s:" << sector);
		y2mil("parted geometry changed new c:" << new_cyl << " h:" << new_head << " s:" << new_sector);
		y2mil("old start:" << start << " end:" << end);
		start = start * new_cyl / cyl;
		end = end * new_cyl / cyl;
		y2mil("new start:" << start << " end:" << end);
		}
	    if( end>maxc && maxc<=cylinders() )
		{
		y2mil("corrected end from " << end << " to max " << maxc);
		end = maxc;
		}
	    if( start==0 && (label == "mac" || label == "amiga") )
		start = 1;
	    cmd_line << start << " ";
	    string save = cmd_line.str();
	    y2mil( "end:" << end << " cylinders:" << cylinders() );
	    if( execCheckFailed( save + decString(end) ) && 
	        end==cylinders() &&
	        execCheckFailed( save + decString(end-1) ) )
		{
		ret = DISK_CREATE_PARTITION_PARTED_FAILED;
		}
	    }
	if( ret==0 )
	    {
	    if( !dmp_slave )
		{
		if( p->type()!=EXTENDED )
		    Storage::waitForDevice(p->device());
		else
		    Storage::waitForDevice();
		if( p->type()==LOGICAL && getStorage()->instsys() )
		    {
		    // kludge to make the extended partition visible in
		    // /proc/partitions otherwise grub refuses to install if root
		    // filesystem is a logical partition
		    PartPair lc = partPair(logicalCreated);
		    call_blockdev = lc.length()<=1;
		    y2mil("logicalCreated:" << lc.length() << " call_blockdev:" << call_blockdev);
		    }
		}
	    p->setCreated( false );
	    if( !getPartedValues( p ))
		ret = DISK_PARTITION_NOT_FOUND;
	    }
	if( ret==0 && p->type()!=EXTENDED )
	    {
	    bool used_as_pv = p->getUsedByType() == UB_LVM;
	    y2mil("zeroNew:" << getStorage()->getZeroNewPartitions() << " used_as_pv:" << used_as_pv);
	    if( used_as_pv || getStorage()->getZeroNewPartitions() )
		{
		ret = Storage::zeroDevice(p->device(), p->sizeK());
		}
	    else if( !dmp_slave && !p->getFormat() )
		{
		bool lsave = false;
		string lbl;
		if( p->needLabel() )
		    {
		    lsave = true;
		    lbl = p->getLabel();
		    }
		p->updateFsData();
		if( lsave )
		    p->setLabel(lbl);
		}
	    }
	if( ret==0 && !dmp_slave && p->id()!=Partition::ID_LINUX )
	    {
	    ret = doSetType( p );
	    }
	if( !dmp_slave && call_blockdev )
	    {
	    SystemCmd c("/sbin/blockdev --rereadpt " + quote(device()));
	    if( p->type()!=EXTENDED )
		Storage::waitForDevice(p->device());
	    }
	}
    else
	{
	ret = DISK_CREATE_PARTITION_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Disk::doRemove( Volume* v )
    {
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->removeText(true) );
	    }
	system_stderr.erase();
	y2mil("doRemove container " << name() << " name " << p->name());
	if( !dmp_slave )
	    {
	    getStorage()->removeDmMapsTo( getPartName(p->OrigNr()) );
	    ret = v->prepareRemove();
	    }
	if( ret==0 && !p->created() )
	    {
	    std::ostringstream cmd_line;
	    classic(cmd_line);
	    cmd_line << PARTEDCMD << quote(device()) << " rm " << p->OrigNr();
	    if( execCheckFailed( cmd_line.str() ) )
		{
		ret = DISK_REMOVE_PARTITION_PARTED_FAILED;
		}
	    }
	if( ret==0 )
	    {
	    if( !removeFromList( p ) )
		ret = DISK_REMOVE_PARTITION_LIST_ERASE;
	    }
	if( ret==0 )
	    {
	    PartPair p = partPair( notCreatedPrimary );
	    if( p.empty() )
		redetectGeometry();
	    }
	if( ret==0 && !dmp_slave )
	    Storage::waitForDevice();
	}
    else
	{
	ret = DISK_REMOVE_PARTITION_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }


int
Disk::freeCylindersAfterPartition(const Partition* p, unsigned long& freeCyls) const
{
    int ret = 0;
    freeCyls = 0;
    unsigned long start = p->cylEnd() + 1;
    unsigned long end = cylinders();
    if (p->type() == LOGICAL && hasExtended())
    {
	ConstPartPair pp = partPair(notDeletedExt);
	end = pp.begin()->cylEnd() + 1;
    }
    ConstPartPair pp = partPair(notDeleted);
    ConstPartIter i = pp.begin();
    while (i != pp.end())
    {
	if( (i->type()==p->type()||
	     (i->type()==EXTENDED&&p->type()==PRIMARY)) &&
	    i->cylStart()>=start && i->cylStart()<end )
	    end = i->cylStart();
	++i;
    }
    if (end > start)
	freeCyls = end-start;
    y2mil("ret:" << ret << " freeCyls:" << freeCyls);
    return ret;
}


int
Disk::resizePartition(Partition* p, unsigned long newCyl)
{
    y2mil("newCyl:" << newCyl << " p:" << *p);
    int ret = 0;
    if( readonly() )
    {
	ret = DISK_CHANGE_READONLY;
    }
    else
    {
	unsigned long long newSize = cylinderToKb(newCyl);
	if( newCyl!=p->cylSize() )
	    ret = p->canResize( newSize );
	if( ret==0 && newCyl<p->cylSize() )
	{
	    if( p->created() )
		p->changeRegion( p->cylStart(), newCyl, newSize );
	    else
		p->setResizedSize( newSize );
	}
	y2mil("newCyl:" << newCyl << " p->cylSize():" << p->cylSize());
	if( ret==0 && newCyl>p->cylSize() )
	{
	    unsigned long free_cyls = 0;
	    ret = freeCylindersAfterPartition(p, free_cyls);
	    if (ret == 0)
	    {
		unsigned long increase = newCyl - p->cylSize();
		if (free_cyls < increase)
		{
		    ret = DISK_RESIZE_NO_SPACE;
		}
		else
		{
		    if( p->created() )
			p->changeRegion( p->cylStart(), newCyl, newSize );
		    else
			p->setResizedSize( newSize );
		}
	    }
	}
    }
    y2mil("ret:" << ret);
    return ret;
}


int Disk::resizeVolume( Volume* v, unsigned long long newSize )
    {
    int ret = 0;
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    else
	{
	Partition * p = dynamic_cast<Partition *>(v);
	unsigned new_cyl_cnt = kbToCylinder(newSize);
	newSize = cylinderToKb(new_cyl_cnt);
	if( p!=NULL )
	    {
	    ret = resizePartition( p, new_cyl_cnt );
	    }
	else
	    {
	    ret = DISK_CHECK_RESIZE_INVALID_VOLUME;
	    }
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Disk::removeVolume( Volume* v )
    {
    return( removePartition( v->nr() ));
    }

bool Disk::isLogical( unsigned nr ) const
    {
    bool ret = ext_possible && nr>max_primary;
    y2mil("nr:" << nr << " ret:" << ret);
    return( ret );
    }

int Disk::doResize( Volume* v )
    {
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	bool remount = false;
	bool needExtend = !p->needShrink();
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->resizeText(true) );
	    }
	if( !dmp_slave && p->isMounted() )
	    {
	    ret = p->umount();
	    if( ret==0 )
		remount = true;
	    }
	if( ret==0 && !dmp_slave && !needExtend && 
	    p->getFs()!=HFS && p->getFs()!=HFSPLUS && p->getFs()!=VFAT && 
	    p->getFs()!=FSNONE )
	    ret = p->resizeFs();
	if( ret==0 )
	    {
	    system_stderr.erase();
	    y2mil("doResize container " << name() << " name " << p->name());
	    std::ostringstream cmd_line;
	    classic(cmd_line);
	    unsigned long long start_sect, end_sect;
	    getPartedSectors( p, start_sect, end_sect );
	    end_sect = start_sect + p->sizeK()*2 - 1;
	    y2mil("end_sect " << end_sect);
	    const Partition * after = getPartitionAfter( p );
	    unsigned long max_end = sizeK()*2-1;
	    if( after!=NULL )
		{
		unsigned long long start_after, end_after;
		getPartedSectors( after, start_after, end_after );
		max_end = start_after-1;
		if( p->type() == LOGICAL )
		    max_end--;
		}
	    else if( p->type()==LOGICAL )
		{
		PartPair ext = partPair(notDeletedExt);
		if( !ext.empty() )
		    {
		    unsigned long long start_ext, end_ext;
		    getPartedSectors( &(*ext.begin()), start_ext, end_ext );
		    max_end = end_ext;
		    }
		}
	    y2mil( "max_end:" << max_end << " end_sect:" << end_sect );
	    if( max_end<end_sect ||
		max_end-end_sect < byte_cyl/512*2 )
		{
		end_sect = max_end;
		y2mil( "new end_sect:" << end_sect );
		}
	    cmd_line << "YAST_IS_RUNNING=1 " << PARTEDCMD << quote(device())
	             << " unit s resize " << p->nr() << " "
	             << start_sect << " " << end_sect;
	    if( execCheckFailed( cmd_line.str() ) )
		{
		ret = DISK_RESIZE_PARTITION_PARTED_FAILED;
		}
	    if( ret==0 && !dmp_slave )
		Storage::waitForDevice(p->device());
	    if( !getPartedValues( p ))
		{
		if( ret==0 )
		    ret = DISK_PARTITION_NOT_FOUND;
		}
	    y2mil("after resize size:" << p->sizeK() << " resize:" << (p->needShrink()||p->needExtend()));
	    }
	if( needExtend && !dmp_slave && 
	    p->getFs()!=HFS && p->getFs()!=HFSPLUS && p->getFs()!=VFAT && 
	    p->getFs()!=FSNONE )
	    ret = p->resizeFs();
	if( ret==0 && remount )
	    ret = p->mount();
	}
    else
	{
	ret = DISK_RESIZE_PARTITION_INVALID_VOLUME;
	}
    y2mil("ret:" << ret);
    return( ret );
    }

const Partition* Disk::getPartitionAfter(const Partition* p) const
    {
    const Partition* ret = NULL;
    y2mil( "p:" << *p );
    ConstPartPair pp = partPair((p->type() == LOGICAL) ? notDeleted : notDeletedLog);
    for (ConstPartIter pi = pp.begin(); pi != pp.end(); ++pi)
	{
	if( !pi->created() &&
	    pi->cylStart()>p->cylStart() &&
	    (ret==NULL || ret->cylStart()>pi->cylStart()) )
	    ret = &(*pi);
	}
    if( ret==NULL )
	y2mil( "ret:NULL" );
    else
	y2mil( "ret:" << *ret );
    return ret;
    }

unsigned Disk::numPartitions() const
    {
    return(partPair( notDeleted ).length());
    }

void Disk::getInfo( DiskInfo& tinfo ) const
    {
    info.sizeK = sizeK();
    info.cyl = cylinders();
    info.heads = heads();
    info.sectors = sectors();
    info.cylSizeB = cylSizeB();
    info.disklabel = labelName();
    info.maxLogical = maxLogical();
    info.maxPrimary = maxPrimary();
    info.initDisk = init_disk;
    info.iscsi = iscsi;
    info.udevPath = udev_path;
    info.udevId = boost::join(udev_id, " ");
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Disk& d )
    {
    s << *((Container*)&d);
    s << " Cyl:" << d.cyl
      << " Head:" << d.head
      << " Sect:" << d.sector
      << " Node <" << d.mjr << ":" << d.mnr << ">"
      << " Range:" << d.range
      << " SizeM:" << d.size_k/1024
      << " Label:" << d.label;
    if( d.detected_label!=d.label )
	s << " DetectedLabel:" << d.detected_label;
    s << " SysfsDir:" << d.sysfs_dir;
    if( !d.udev_path.empty() )
	s << " UdevPath:" << d.udev_path;
    if( !d.udev_id.empty() )
	s << " UdevId:" << d.udev_id;
    s << " MaxPrimary:" << d.max_primary;
    if( d.ext_possible )
	s << " ExtPossible MaxLogical:" << d.max_logical;
    if( d.init_disk )
	s << " InitDisk";
    if( d.iscsi )
	s << " iSCSI";
    if( d.dmp_slave )
	s << " DmpSlave";
    if( d.gpt_enlarge )
	s << " GptEnlarge";
    return( s );
    }


void Disk::logDifference( const Container& d ) const
    {
    string log = getDiffString( d );
    const Disk * p = dynamic_cast<const Disk*>(&d);
    if( p != NULL )
	{
	if( cyl!=p->cyl )
	    log += " Cyl:" + decString(cyl) + "-->" + decString(p->cyl);
	if( head!=p->head )
	    log += " Head:" + decString(head) + "-->" + decString(p->head);
	if( sector!=p->sector )
	    log += " Sect:" + decString(sector) + "-->" + decString(p->sector);
	if( mjr!=p->mjr )
	    log += " Mjr:" + decString(mjr) + "-->" + decString(p->mjr);
	if( mnr!=p->mnr )
	    log += " Mnr:" + decString(mnr) + "-->" + decString(p->mnr);
	if( range!=p->range )
	    log += " Range:" + decString(range) + "-->" + decString(p->range);
	if( size_k!=p->size_k )
	    log += " SizeK:" + decString(size_k) + "-->" + decString(p->size_k);
	if( label!=p->label )
	    log += " Label:" + label + "-->" + p->label;
	if( sysfs_dir!=p->sysfs_dir )
	    log += " SysfsDir:" + sysfs_dir + "-->" + p->sysfs_dir;
	if( max_primary!=p->max_primary )
	    log += " MaxPrimary:" + decString(max_primary) + "-->" + decString(p->max_primary);
	if( ext_possible!=p->ext_possible )
	    {
	    if( p->ext_possible )
		log += " -->ExtPossible";
	    else
		log += " ExtPossible-->";
	    }
	if( max_logical!=p->max_logical )
	    log += " MaxLogical:" + decString(max_logical) + "-->" + decString(p->max_logical);
	if( init_disk!=p->init_disk )
	    {
	    if( p->init_disk )
		log += " -->InitDisk";
	    else
		log += " InitDisk-->";
	    }
	if( iscsi!=p->iscsi )
	    {
	    if( p->iscsi )
		log += " -->iSCSI";
	    else
		log += " iSCSI-->";
	    }
	y2mil(log);
	ConstPartPair pp=partPair();
	ConstPartIter i=pp.begin();
	while( i!=pp.end() )
	    {
	    ConstPartPair pc=p->partPair();
	    ConstPartIter j = pc.begin();
	    while( j!=pc.end() &&
		   (i->device()!=j->device() || i->created()!=j->created()) )
		++j;
	    if( j!=pc.end() )
		{
		if( !i->equalContent( *j ) )
		    i->logDifference( *j );
		}
	    else
		y2mil( "  -->" << *i );
	    ++i;
	    }
	pp=p->partPair();
	i=pp.begin();
	while( i!=pp.end() )
	    {
	    ConstPartPair pc=partPair();
	    ConstPartIter j = pc.begin();
	    while( j!=pc.end() &&
		   (i->device()!=j->device() || i->created()!=j->created()) )
		++j;
	    if( j==pc.end() )
		y2mil( "  <--" << *i );
	    ++i;
	    }
	}
    else
	y2mil(log);
    }

bool Disk::equalContent( const Container& rhs ) const
    {
    const Disk * p = NULL;
    bool ret = Container::equalContent(rhs);
    if( ret )
	p = dynamic_cast<const Disk*>(&rhs);
    if( ret && p )
	ret = cyl==p->cyl && head==p->head && sector==p->sector &&
	      mjr==p->mjr && mnr==p->mnr && range==p->range &&
	      size_k==p->size_k && max_primary==p->max_primary &&
	      ext_possible==p->ext_possible && max_logical==p->max_logical &&
	      init_disk==p->init_disk && label==p->label && 
	      iscsi==p->iscsi && sysfs_dir==p->sysfs_dir && 
	      dmp_slave==p->dmp_slave && gpt_enlarge==p->gpt_enlarge;
    if( ret && p )
	{
	ConstPartPair pp = partPair();
	ConstPartPair pc = p->partPair();
	ConstPartIter i = pp.begin();
	ConstPartIter j = pc.begin();
	while( ret && i!=pp.end() && j!=pc.end() )
	    {
	    ret = ret && i->equalContent( *j );
	    ++i;
	    ++j;
	    }
	ret = ret && i==pp.end() && j==pc.end();
	}
    return( ret );
    }

Disk& Disk::operator= ( const Disk& rhs )
    {
    y2deb("operator= from " << rhs.nm);
    cyl = rhs.cyl;
    head = rhs.head;
    sector = rhs.sector;
    new_cyl = rhs.new_cyl;
    new_head = rhs.new_head;
    new_sector = rhs.new_sector;
    label = rhs.label;
    detected_label = rhs.detected_label;
    range = rhs.range;
    byte_cyl = rhs.byte_cyl;
    max_primary = rhs.max_primary;
    ext_possible = rhs.ext_possible;
    max_logical = rhs.max_logical;
    init_disk = rhs.init_disk;
    iscsi = rhs.iscsi;
    udev_path = rhs.udev_path;
    udev_id = rhs.udev_id;
    logfile_name = rhs.logfile_name;
    sysfs_dir = rhs.sysfs_dir;
    dmp_slave = rhs.dmp_slave;
    gpt_enlarge = rhs.gpt_enlarge;
    return( *this );
    }

Disk::Disk( const Disk& rhs ) : Container(rhs)
    {
    y2deb("constructed disk by copy constructor from " << rhs.nm);
    *this = rhs;
    ConstPartPair p = rhs.partPair();
    for( ConstPartIter i = p.begin(); i!=p.end(); ++i )
	{
	Partition * p = new Partition( *this, *i );
	vols.push_back( p );
	}
    }

}
