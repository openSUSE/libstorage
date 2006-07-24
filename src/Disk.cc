/*
  Textdomain    "storage"
*/

#include <iostream>

#include <string>
#include <sstream>
#include <iomanip>

#include <fcntl.h>
#include <sys/mount.h>         /* for BLKGETSIZE */
#include <linux/hdreg.h>       /* for HDIO_GETGEO */

#include "y2storage/Region.h"
#include "y2storage/Partition.h"
#include "y2storage/ProcPart.h"
#include "y2storage/Disk.h"
#include "y2storage/Storage.h"
#include "y2storage/AsciiFile.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"

#define PARTEDCMD "/usr/sbin/parted -s "  // blank at end !!

using namespace std;
using namespace storage;

Disk::Disk( Storage * const s, const string& Name,
            unsigned long long SizeK ) :
    Container(s,Name,staticType())
    {
    init_disk = false;
    logfile_name = Name;
    size_k = SizeK;
    y2debug( "constructed disk %s", dev.c_str() );
    }

Disk::Disk( Storage * const s, const string& Name,
            unsigned num, unsigned long long SizeK, ProcPart& ppart ) :
    Container(s,Name,staticType())
    {
    y2milestone( "constructed disk %s nr %u sizeK:%llu", Name.c_str(), num,
                 SizeK );
    logfile_name = Name + decString(num);
    init_disk = false;
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

Disk::Disk( Storage * const s, const string& fname ) :
    Container(s,"",staticType())
    {
    nm = fname.substr( fname.find_last_of( '/' )+1);
    if( nm.find("disk_")==0 )
	nm.erase( 0, 5 );
    AsciiFile file( fname );
    string line;
    if( searchFile( file, "^Device:", line ) )
	{
	dev = extractNthWord( 1, line );
	}
    mnr = mjr = 0;
    if( searchFile( file, "^Major:", line ) )
	{
	extractNthWord( 1, line ) >> mjr;
	}
    if( searchFile( file, "^Minor:", line ) )
	{
	extractNthWord( 1, line ) >> mnr;
	}
    range = 4;
    if( searchFile( file, "^Range:", line ) )
	{
	extractNthWord( 1, line ) >> range;
	}
    cyl = 1024;
    if( searchFile( file, "^Cylinder:", line ) )
	{
	extractNthWord( 1, line ) >> cyl;
	}
    head = 1024;
    if( searchFile( file, "^Head:", line ) )
	{
	extractNthWord( 1, line ) >> head;
	}
    sector = 32;
    if( searchFile( file, "^Sector:", line ) )
	{
	extractNthWord( 1, line ) >> sector;
	}
    byte_cyl = head * sector * 512;
    if( searchFile( file, "^Label:", line ) )
	{
	label = extractNthWord( 1, line );
	}
    max_primary = 0;
    if( searchFile( file, "^MaxPrimary:", line ) )
	{
	extractNthWord( 1, line ) >> max_primary;
	}
    ext_possible = false;
    if( searchFile( file, "^ExtPossible:", line ) )
	{
	extractNthWord( 1, line ) >> ext_possible;
	}
    max_logical = 0;
    if( searchFile( file, "^MaxLogical:", line ) )
	{
	extractNthWord( 1, line ) >> max_logical;
	}
    ronly = false;
    if( searchFile( file, "^Readonly:", line ) )
	{
	extractNthWord( 1, line ) >> ronly;
	}
    if( FakeDisk() && isdigit( nm[nm.size()-1] ))
	{
	string::size_type p = nm.find_last_not_of( "0123456789" );
	nm.erase( p+1 );
	}
    size_k = 0;
    if( searchFile( file, "^SizeK:", line ) )
	{
	extractNthWord( 1, line ) >> size_k;
	}
    udev_path.clear();
    if( searchFile( file, "^UdevPath:", line ) )
	{
	udev_path = extractNthWord( 1, line );
	}
    udev_id.clear();
    if( searchFile( file, "^UdevId:", line ) )
	{
	udev_id.push_back( extractNthWord( 1, line ));
	}
    int lnr = 0;
    while( searchFile( file, "^Partition:", line, lnr ))
	{
	lnr++;
	Partition *p = new Partition( *this, extractNthWord( 1, line, true ));
	addToList( p );
	}
    y2debug( "constructed disk %s from file %s", dev.c_str(), fname.c_str() );
    }

Disk::~Disk()
    {
    y2debug( "destructed disk %s", dev.c_str() );
    }

void Disk::setUdevData( const string& path, const string& id )
    {
    y2milestone( "disk %s path %s id %s", nm.c_str(), path.c_str(), id.c_str() );
    udev_path = path;
    udev_id.clear();
    udev_id = splitString( id );
    list<string>::iterator i = find_if( udev_id.begin(), udev_id.end(), 
                                        find_begin( "edd-" ) );
    if( i!=udev_id.end() )
	{
	udev_id.push_back( *i );
	udev_id.erase( i );
	}
    y2mil( "id:" << udev_id );
    PartPair pp = partPair();
    for( PartIter p=pp.begin(); p!=pp.end(); ++p )
	{
	p->addUdevData();
	}
    }

void 
Disk::addMpAlias( const string& dev )
    {
    y2mil( "dev:" << dev );
    if( dev != device() &&
        find( mp_alias.begin(), mp_alias.end(), dev )==mp_alias.end() )
	{
	mp_alias.push_back( dev );
	y2mil( "mp_alias:" << mp_alias );
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
    y2milestone( "KB:%lld ret:%ld byte_cyl:%ld", kb, ret, byte_cyl );
    return (ret);
    }

bool Disk::detectGeometry()
    {
    y2milestone( "disk:%s", device().c_str() );
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
	y2milestone( "After HDIO_GETGEO ret %d Head:%u Sector:%u Cylinder:%lu",
		     rcode, head, sector, cyl );
	__uint64_t sect = 0;
	rcode = ioctl( fd, BLKGETSIZE64, &sect);
	y2milestone( "BLKGETSIZE64 Ret:%d Bytes:%llu", rcode,
		     (unsigned long long int) sect );
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
	    y2milestone( "BLKGETSIZE Ret:%d Sect:%lu", rcode, lsect );
	    if( rcode==0 && lsect!=0 )
		{
		cyl = lsect / (unsigned long)(head*sector);
		ret = true;
		}
	    }
	y2milestone( "After getsize Cylinder:%lu", cyl );
	close( fd );
	}
    byte_cyl = head * sector * 512;
    y2milestone( "ret:%d byte_cyl:%lu", ret, byte_cyl );
    return( ret );
    }

bool Disk::getSysfsInfo( const string& SysfsDir )
    {
    bool ret = true;
    sysfs_dir = SysfsDir;
    y2mil( "sysfs_dir:" << sysfs_dir );
    string SysfsFile = sysfs_dir+"/range";
    if( access( SysfsFile.c_str(), R_OK )==0 )
	{
	ifstream File( SysfsFile.c_str() );
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
	char c;
	File >> mjr;
	File >> c;
	File >> mnr;
	}
    else
	{
	ret = false;
	}
    y2milestone( "Ret:%d Range:%ld Major:%ld Minor:%ld", ret, range, mjr,
                 mnr );
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
    if( i!=geo.end() )
	{
	*i >> val;
	if( val>0 )
	    c = val;
	}
    ++i;
    val = 0;
    if( i!=geo.end() )
	{
	*i >> val;
	if( val>0 )
	    h = (unsigned)val;
	}
    ++i;
    val = 0;
    if( i!=geo.end() )
	{
	*i >> val;
	if( val>0 )
	    s = (unsigned)val;
	}
    y2milestone( "line:%s", line.c_str() );
    y2milestone( "c:%lu h:%u s:%u", c, h, s );
    }

bool Disk::detectPartitions( ProcPart& ppart )
    {
    bool ret = true;
    string cmd_line = PARTEDCMD + device() + " unit cyl print | sort -n";
    string dlabel;
    system_stderr.erase();
    y2milestone( "executing cmd:%s", cmd_line.c_str() );
    SystemCmd Cmd( cmd_line );
    checkSystemError( cmd_line, Cmd );
    if( Cmd.select( "Partition Table:" )>0 )
	{
	string tmp = *Cmd.getLine(0, true);
	y2milestone( "Label line:%s", tmp.c_str() );
	dlabel = extractNthWord( 2, tmp );
	}
    if( Cmd.select( "BIOS cylinder" )>0 )
	{
	string tmp = *Cmd.getLine(0, true);
	getGeometry( tmp, cyl, head, sector );
	new_cyl = cyl;
	new_head = head;
	new_sector = sector;
	y2milestone( "After parted Head:%u Sector:%u Cylinder:%lu",
		     head, sector, cyl );
	byte_cyl = head * sector * 512;
	y2milestone( "byte_cyl:%lu", byte_cyl );
	}
    y2milestone( "Label:%s", dlabel.c_str() );
    if( dlabel!="loop" )
	{
	setLabelData( dlabel );
	checkPartedOutput( Cmd, ppart );
	if( dlabel.empty() )
	    {
	    Cmd.setCombine();
	    Cmd.execute( "/sbin/fdisk -l " + device() );
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
	dlabel = defaultLabel();
    setLabelData( dlabel );

    if (label == "unsupported")
	{
	// popup text %1$s is replaced by disk name e.g. /dev/hda
	string txt = sformat(
_("The partition table type on disk %1$s cannot be handled by\n"
"this tool.\n"
"\n"
"You can use the partitions on disk %1$s as they are.\n"
"You can format them and assign mount points to them, but you\n"
"cannot add, edit, resize, or remove partitions from that\n"
"disk with this tool."), dev.c_str() );
	getStorage()->infoPopupCb( txt );

	detected_label = label;
	ronly = true;
	}

    y2milestone( "ret:%d partitons:%zd detected label:%s label:%s", ret,
                 vols.size(), detected_label.c_str(), label.c_str() );
    return( ret );
    }

void
Disk::logData( const string& Dir )
    {
    string fname( Dir + "/disk_" + logfile_name + ".tmp" );
    ofstream file( fname.c_str() );
    file << "Device: " << dev << endl;
    if( !udev_path.empty() )
	file << "UdevPath: " << udev_path << endl;
    if( !udev_id.empty() )
	file << "UdevId: " << udev_id << endl;
    if( !mp_alias.empty() )
	file << "MpAlias: " << mp_alias << endl;
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

    PartPair pp = partPair();
    for( PartIter p=pp.begin(); p!=pp.end(); ++p )
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
    y2milestone( "disklabel:%s", disklabel.c_str() );
    int i=0;
    while( !labels[i].name.empty() && labels[i].name!=disklabel )
	{
	i++;
	}
    if( labels[i].name.empty() )
	{
	y2error ("unknown disklabel %s", disklabel.c_str());
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
    y2milestone( "name:%s ext:%d primary:%d logical:%d", label.c_str(),
                 ext_possible, max_primary, max_logical );
    }

int
Disk::checkSystemError( const string& cmd_line, const SystemCmd& cmd )
    {
    string tmp = *cmd.getString(SystemCmd::IDX_STDERR);
    if( tmp.length()>0 )
        {
        y2error( "cmd:%s", cmd_line.c_str() );
        y2error( "err:%s", tmp.c_str() );
	if( !system_stderr.empty() )
	    {
	    system_stderr += "\n";
	    }
	system_stderr += tmp;
        }
    tmp = *cmd.getString(SystemCmd::IDX_STDOUT);
    if( tmp.length()>0 )
        {
        y2milestone( "cmd:%s", cmd_line.c_str() );
        y2milestone( "out:%s", tmp.c_str() );
	if( !system_stderr.empty() )
	    {
	    system_stderr += "\n";
	    }
	system_stderr += tmp;
        }
    if( cmd.retcode() != 0 )
        {
        y2error( "retcode:%d", cmd.retcode() );
        }
    return( cmd.retcode() );
    }

int
Disk::execCheckFailed( const string& cmd_line )
    {
    static SystemCmd cmd;
    return( execCheckFailed( cmd, cmd_line ) );
    }

int Disk::execCheckFailed( SystemCmd& cmd, const string& cmd_line )
    {
    cmd.execute( cmd_line );
    int ret = checkSystemError( cmd_line, cmd );
    if( ret!=0 )
	setExtError( cmd );
    return( ret );
    }



bool
Disk::scanPartedLine( const string& Line, unsigned& nr, unsigned long& start,
                      unsigned long& csize, PartitionType& type, unsigned& id, bool& boot )
    {
    unsigned long StartM, EndM;
    string PartitionType, TInfo;

    y2debug( "Line: %s", Line.c_str() );
    std::istringstream Data( Line );

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
    char c;
    TInfo = ",";
    Data.unsetf(ifstream::skipws);
    Data >> c;
    char last_char = ',';
    while( !Data.eof() )
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
    y2milestone( "Fields Num:%d Start:%lu End:%lu Type:%d",
		 nr, StartM, EndM, type );
    y2milestone( "TInfo:%s", TInfo.c_str() );
    if( nr>0 )
      {
      start = StartM;
      csize = EndM-StartM+1;
      if( start+csize > cylinders() )
	  {
	  csize = cylinders()-start;
	  y2milestone( "new csize:%lu", csize );
	  }
      id = Partition::ID_LINUX;
      boot = TInfo.find( ",boot," ) != string::npos;
      string OrigTInfo = TInfo;
      tolower( TInfo );
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
	      y2debug( "val=%s id=%d", val.c_str(), tmp_id );
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
	  if( TInfo.find( ",boot," ) != string::npos )
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
	  }
      y2milestone( "Fields Num:%d Id:%x Ptype:%d Start:%ld Size:%ld",
		   nr, id, type, start, csize );
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
    for( int i=0; i<cnt; i++)
	{
	unsigned pnr;
	unsigned long c_start;
	unsigned long c_size;
	PartitionType type;
	unsigned id;
	bool boot;

	line = *Cmd.getLine(i);
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
    if( !checkPartedValid( ppart, nm, pl, range_exceed ) )
	{
	// popup text %1$s is replaced by disk name e.g. /dev/hda
	string txt = sformat(
_("The partitioning on disk %1$s is not readable by\n"
"the partitioning tool parted, which is used to change the\n"
"partition table.\n"
"\n"
"You can use the partitions on disk %1$s as they are.\n"
"You can format them and assign mount points to them, but you\n"
"cannot add, edit, resize, or remove partitions from that\n"
"disk with this tool."), dev.c_str() );

	getStorage()->infoPopupCb( txt );
	ronly = true;
	}
    if( range_exceed>0 )
	{
	// popup text %1$s is replaced by disk name e.g. /dev/hda
	//            %2$lu and %3$lu are replaced by numbers.
	string txt = sformat(
_("Your disk %1$s contains %2$lu partitions. The maximum number\n"
"of partitions that the kernel driver of the disk can handle is %3$lu.\n"
"Partitions numbered above %3$lu cannot be accessed."),
                              (char*)dev.c_str(), range_exceed, range-1 );
	getStorage()->infoPopupCb( txt );
	}
    for( list<Partition*>::iterator i=pl.begin(); i!=pl.end(); ++i )
	{
	addToList( *i );
	}
    return( true );
    }

bool Disk::checkPartedValid( const ProcPart& pp, const string& diskname,
                             list<Partition*>& pl, unsigned long& range_exceed )
    {
    long ext_nr = 0;
    bool ret=true;
    unsigned long Dummy;
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
    reg += "[0-9]+";
    list<string> ps = pp.getMatchingEntries( reg );
    y2mil( "regex " << reg << " ps " << ps );
    for( list<string>::const_iterator i=ps.begin(); i!=ps.end(); i++ )
	{
	pair<string,long> p = getDiskPartition( *i );
	if( p.second>=0 && p.second!=ext_nr &&
	    pp.getInfo( *i, SizeK, Dummy, Dummy ))
	    {
	    proc_l[unsigned(p.second)] = kbToCylinder( SizeK );
	    }
	}
    bool openbsd = false;
    y2mil( "proc  :" << proc_l );
    y2mil( "parted:" << parted_l );
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
    if( !ret )
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
	    pair<string,long> pr = getDiskPartition( *i );
	    if( pp.getSize( *i, s ))
		{
		cyl = kbToCylinder(s);
		if( pr.second < (long)range )
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
			if( (unsigned)pr.second>max_primary )
			    {
			    type = LOGICAL;
			    }
			}
		    Partition *p =
			new Partition( *this, pr.second, s, cyl_start, cyl,
			               type, id, false );
		    pl.push_back( p );
		    }
		else
		    range_exceed = max( range_exceed, (unsigned long)pr.second );
		cyl_start += cyl;
		}
	    }
	}
    y2milestone("haveBsd:%d pr.size:%zd pa.size:%zd", openbsd,
                 proc_l.size(), parted_l.size() );
    y2milestone( "ret:%d", ret );
    return( ret );
    }

static bool isBsdPart( const Partition& p )
    { return( p.id()==0xa5 || p.id()==0xa6 || p.id()==0xeb ); }

bool Disk::haveBsdPart(const list<Partition*>& pl) const
    {
    bool ret = false;
    list<Partition*>::const_iterator i = pl.begin();
    while( !ret && i!=pl.end() )
	{
	ret = isBsdPart(**i);
	++i;
	}
    return( ret );
    }

string Disk::defaultLabel()
    {
    string ret = "msdos";
    if( Storage::arch()=="ia64" )
	ret = "gpt";
    else if( Storage::arch()=="sparc" )
	ret = "sun";
    else if( Storage::arch()=="ppc" && Storage::isPPCMac() )
	ret = "mac";
    y2milestone( "ret %s", ret.c_str() );
    return( ret );
    }

Disk::label_info Disk::labels[] = {
	{ "msdos", true, 4, 63 },
	{ "gpt", false, 63, 0 },
	{ "bsd", false, 8, 0 },
	{ "sun", false, 8, 0 },
	{ "mac", false, 64, 0 },
	{ "dasd", false, 3, 0 },
	{ "aix", false, 0, 0 },
	{ "", false, 0, 0 }
    };

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

pair<string,long> Disk::getDiskPartition( const string& dev )
    {
    long nr = -1;
    string disk = dev;
    bool need_p = Disk::needP(dev);
    string::size_type p = dev.find_last_not_of( "0123456789" );
    if( p != string::npos && (!need_p||dev[p]=='p') && isdigit(dev[p+1]))
	{
	dev.substr(p+1) >> nr;
	disk = dev.substr( 0, p-(need_p?1:0));
	}
    return( make_pair<string,long>(disk,nr) );
    }

static bool isExtended( const Partition& p )
    {
    return( Volume::notDeleted(p) && p.type()==EXTENDED );
    }

bool Disk::hasExtended() const
    {
    return( ext_possible && !partPair(isExtended).empty() );
    }

unsigned Disk::availablePartNumber( PartitionType type )
    {
    y2milestone( "begin name:%s type %d", name().c_str(), type );
    unsigned ret = 0;
    PartPair p = partPair( notDeleted );
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
	PartIter i=p.begin();
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
	PartIter i=p.begin();
	unsigned start = 1;
	while( i!=p.end() && i->nr()==start && i->nr()<=max_primary )
	    {
	    ++i;
	    start++;
	    }
	if( start<=max_primary )
	    ret = start;
	if( type==EXTENDED && (!ext_possible || hasExtended()))
	    ret = 0;
	}

    if( ret >= range )
	ret = 0;

    y2milestone( "ret %d", ret );
    return( ret );
    }

static bool notDeletedLog( const Partition& p )
    {
    return( !p.deleted() && p.type()==LOGICAL );
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

void Disk::getUnusedSpace( list<Region>& free, bool all, bool logical )
    {
    y2milestone( "all:%d logical:%d", all, logical );
    free.clear();
    if( all || !logical )
	{
	PartPair p = partPair( notDeletedNotLog );
	unsigned long start = 1;
	for( PartIter i=p.begin(); i!=p.end(); ++i )
	    {
	    if( i->cylStart()>start )
		free.push_back( Region( start, i->cylStart()-start ));
	    start = i->cylEnd()+1;
	    }
	if( cylinders()>start )
	    free.push_back( Region( start, cylinders()-start ));
	}
    if( all || logical )
	{
	PartPair ext = partPair(isExtended);
	if( !ext.empty() )
	    {
	    PartPair p = partPair( notDeletedLog );
	    unsigned long start = ext.begin()->cylStart();
	    for( PartIter i=p.begin(); i!=p.end(); ++i )
		{
		if( i->cylStart()>start )
		    free.push_back( Region( start, i->cylStart()-start ));
		start = i->cylEnd()+1;
		}
	    if( ext.begin()->cylEnd()>start )
		free.push_back( Region( start, ext.begin()->cylEnd()-start ));
	    }
	}
    }

static bool regions_sort_size( const Region& rhs, const Region& lhs )
    {
    return( rhs.len()>lhs.len() );
    }

int Disk::createPartition( unsigned long cylLen, string& device,
			   bool checkRelaxed )
    {
    y2milestone( "len %ld relaxed:%d", cylLen, checkRelaxed );
    int ret = 0;
    list<Region> free;
    getUnusedSpace( free );
    y2milestone( "free:" );
    if( !free.empty() )
	{
	free.sort( regions_sort_size );
	list<Region>::iterator i = free.begin();
	while( i!=free.end() && i->len()>=cylLen )
	    ++i;
	--i;
	if( i->len()>=cylLen )
	    {
	    PartPair ext = partPair(isExtended);
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
    y2milestone( "ret %d", ret );
    return( ret );
    }

int Disk::createPartition( PartitionType type, string& device )
    {
    y2milestone( "type %u", type );
    int ret = 0;
    list<Region> free;
    getUnusedSpace( free, type==PTYPE_ANY, type==LOGICAL );
    if( !free.empty() )
	{
	free.sort( regions_sort_size );
	list<Region>::iterator i = free.begin();
	PartPair ext = partPair(isExtended);
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
    y2milestone( "ret %d", ret );
    return( ret );
    }

int Disk::nextFreePartition( PartitionType type, unsigned& nr, string& device )
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
    return( ret );
    }

int Disk::createPartition( PartitionType type, unsigned long start,
                           unsigned long len, string& device,
			   bool checkRelaxed )
    {
    y2milestone( "begin type %d at %ld len %ld relaxed:%d", type, start, len,
                 checkRelaxed );
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
	Partition * p = new Partition( *this, number, cylinderToKb(len), start,
	                               len, type );
	PartPair pp = partPair();
	PartIter i=pp.begin();
	while( i!=pp.end() && !(i->deleted() && i->cylStart()==start) )
	    ++i;
	if( i!=pp.end() )
	    {
	    y2mil( "deleted at same start:" << *i );
	    p->setFs( i->getFs() );
	    p->setUuid( i->getUuid() );
	    p->initLabel( i->getLabel() );
	    }
	p->setCreated();
	device = p->device();
	addToList( p );
	}
    y2milestone( "ret %d device:%s", ret, ret==0?device.c_str():"" );
    return( ret );
    }

int Disk::createChecks( PartitionType& type, unsigned long start,
                        unsigned long len, bool checkRelaxed )
    {
    y2milestone( "begin type %d at %ld len %ld relaxed:%d", type, start, len,
                 checkRelaxed );
    unsigned fuzz = checkRelaxed ? 2 : 0;
    int ret = 0;
    Region r( start, len );
    PartPair ext = partPair(isExtended);
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
	y2milestone( "too large for disk cylinders %lu", cylinders() );
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
	PartPair p = (type!=LOGICAL) ? partPair( notDeleted )
	                             : partPair( notDeletedLog );
	PartIter i = p.begin();
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
    y2milestone( "ret %d", ret );
    return( ret );
    }

int Disk::changePartitionArea( unsigned nr, unsigned long start,
			       unsigned long len, bool checkRelaxed )
    {
    y2milestone( "begin nr %u at %ld len %ld relaxed:%d", nr, start, len,
                 checkRelaxed );
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
	y2milestone( "too large for disk cylinders %lu", cylinders() );
	ret = DISK_PARTITION_EXCEEDS_DISK;
	}
    if( ret==0 && len==0 )
	{
	ret = DISK_PARTITION_ZERO_SIZE;
	}
    if( ret==0 && part->type()==LOGICAL )
	{
	PartPair ext = partPair(isExtended);
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
    y2milestone( "ret %d", ret );
    return( ret );
    }

static bool volume_ptr_sort_nr( Partition*& rhs, Partition*& lhs )
    {
    return( rhs->nr()<lhs->nr() );
    }

int Disk::removePartition( unsigned nr )
    {
    y2milestone( "begin nr %u", nr );
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
	bool creat = i->created();
	if( creat )
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
		    if( creat )
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
    y2milestone( "ret %d", ret );
    return( ret );
    }

void Disk::changeNumbers( const PartIter& b, const PartIter& e,
                          unsigned start, int incr )
    {
    y2milestone( "start:%u incr:%d", start, incr );
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
    y2milestone( "begin" );
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
	while( i!=vols.rend() )
	    {
	    if( !(*i)->deleted() )
		getStorage()->removeVolume( (*i)->device() );
	    ++i;
	    }
	getStorage()->setRecursiveRemoval(save);
	setDeleted( true );
	}
    y2milestone( "ret %d", ret );
    return( ret );
    }

int Disk::changePartitionId( unsigned nr, unsigned id )
    {
    y2milestone( "begin nr:%u id:%x", nr, id );
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
	i->changeId( id );
	}
    y2milestone( "ret %d", ret );
    return( ret );
    }

int Disk::forgetChangePartitionId( unsigned nr )
    {
    y2milestone( "begin nr:%u", nr );
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
    y2milestone( "ret %d", ret );
    return( ret );
    }

int Disk::getToCommit( CommitStage stage, list<Container*>& col,
                       list<Volume*>& vol )
    {
    int ret = 0;
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    Container::getToCommit( stage, col, vol );
    if( stage==INCREASE )
	{
	PartPair p = partPair( Partition::toChangeId );
	for( PartIter i=p.begin(); i!=p.end(); ++i )
	    if( find( vol.begin(), vol.end(), &(*i) )==vol.end() )
		vol.push_back( &(*i) );
	}
    if( col.size()!=oco || vol.size()!=ovo )
	y2milestone( "ret:%d col:%zd vol:%zd", ret, col.size(), vol.size());
    return( ret );
    }

int Disk::commitChanges( CommitStage stage, Volume* vol )
    {
    y2milestone( "name %s stage %d", name().c_str(), stage );
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Disk::commitChanges( CommitStage stage )
    {
    y2milestone( "name %s stage %d", name().c_str(), stage );
    int ret = 0;
    if( stage==DECREASE && deleted() )
	{
	ret = doCreateLabel();
	}
    else
	ret = DISK_COMMIT_NOTHING_TODO;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void Disk::getCommitActions( list<commitAction*>& l ) const
    {
    Container::getCommitActions( l );
    if( deleted() )
	{
	list<commitAction*>::iterator i = l.begin();
	while( i!=l.end() )
	    {
	    if( (*i)->stage==DECREASE )
		{
		delete( *i );
		i=l.erase( i );
		}
	    else
		++i;
	    }
	l.push_front( new commitAction( DECREASE, staticType(),
				        setDiskLabelText(false), true, true ));
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
    y2milestone( "label:%s", label.c_str() );
    int ret = 0;
    if( !silent )
	{
	getStorage()->showInfoCb( setDiskLabelText(true) );
	}
    getStorage()->removeDmMapsTo( device() );
    removePresentPartitions();
    system_stderr.erase();
    std::ostringstream cmd_line;
    cmd_line << PARTEDCMD << device() << " mklabel " << label;
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
	getStorage()->waitForDevice();
	redetectGeometry();
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void Disk::removePresentPartitions()
    {
    VolPair p = volPair();
    if( !p.empty() )
	{
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
	setSilent( false );
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
	    i = vols.erase( i );
	    }
	else
	    ++i;
	}
    }

void Disk::redetectGeometry()
    {
    string cmd_line = PARTEDCMD + device() + " unit cyl print";
    y2milestone( "executing cmd:%s", cmd_line.c_str() );
    SystemCmd Cmd( cmd_line );
    if( Cmd.select( "BIOS cylinder" )>0 )
	{
	unsigned long c;
	unsigned h;
	unsigned s;
	string tmp = *Cmd.getLine(0, true);
	getGeometry( tmp, c, h, s );
	if( c!=0 && c!=cyl )
	    {
	    new_cyl = c;
	    new_head = h;
	    new_sector = s;
	    y2milestone( "new parted geometry Head:%u Sector:%u Cylinder:%lu",
			 new_head, new_sector, new_cyl );
	    }
	}
    }

int Disk::doSetType( Volume* v )
    {
    y2milestone( "doSetType container %s name %s", name().c_str(),
		 v->name().c_str() );
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->setTypeText(true) );
	    }
	system_stderr.erase();
	std::ostringstream cmd_line;
	cmd_line << PARTEDCMD << device() << " set " << p->nr() << " ";
	string start_cmd = cmd_line.str();
	if( ret==0 )
	    {
	    cmd_line.str( start_cmd );
	    cmd_line.seekp(0, ios_base::end );
	    cmd_line << "lvm " << (p->id()==Partition::ID_LVM ? "on" : "off");
	    if( execCheckFailed( cmd_line.str() ) )
		{
		ret = DISK_SET_TYPE_PARTED_FAILED;
		}
	    }
	if( ret==0 )
	    {
	    cmd_line.str( start_cmd );
	    cmd_line.seekp(0, ios_base::end );
	    cmd_line << "raid " << (p->id()==Partition::ID_RAID?"on":"off");
	    if( execCheckFailed( cmd_line.str() ) )
		{
		ret = DISK_SET_TYPE_PARTED_FAILED;
		}
	    }
	if( ret==0 && (label=="dvh"||label=="mac"))
	    {
	    cmd_line.str( start_cmd );
	    cmd_line.seekp(0, ios_base::end );
	    cmd_line << "swap " << (p->id()==Partition::ID_SWAP?"on":"off");
	    if( execCheckFailed( cmd_line.str() ) )
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
	    if( execCheckFailed( cmd_line.str() ) )
		{
		ret = DISK_SET_TYPE_PARTED_FAILED;
		}
	    }
	if( ret==0 && p->id()<=255 && label=="msdos" )
	    {
	    cmd_line.str( start_cmd );
	    cmd_line.seekp(0, ios_base::end );
	    cmd_line << "type " << p->id();
	    if( execCheckFailed( cmd_line.str() ) )
		{
		ret = DISK_SET_TYPE_PARTED_FAILED;
		}
	    }
	if( ret==0 )
	    {
	    getStorage()->waitForDevice( p->device() );
	    p->changeIdDone();
	}
	}
    else
	{
	ret = DISK_SET_TYPE_INVALID_VOLUME;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

bool
Disk::getPartedValues( Partition *p )
    {
    bool ret = false;
    if( getStorage()->test() )
	{
	ret = true;
	p->setSize( p->sizeK() );
	}
    else
	{
	ProcPart ppart;
	std::ostringstream cmd_line;
	cmd_line << PARTEDCMD << device() << " unit cyl print | grep -w \"^[ \t]*\"" << p->nr();
	SystemCmd cmd( cmd_line.str() );
	unsigned nr, id;
	unsigned long start, csize;
	PartitionType type;
	bool boot;
	if( cmd.numLines()>0 &&
	    scanPartedLine( *cmd.getLine(0), nr, start, csize, type,
			    id, boot ))
	    {
	    y2milestone( "really created at cyl:%ld csize:%ld", start, csize );
	    p->changeRegion( start, csize, cylinderToKb(csize) );
	    unsigned long long s=0;
	    ret = true;
	    if( p->type() != EXTENDED )
		{
		if( !ppart.getSize( p->device(), s ) || s==0 )
		    {
		    y2error( "device %s not found in /proc/partitions",
		             p->device().c_str() );
		    ret = false;
		    }
		else
		    p->setSize( s );
		}
	    }
	cmd_line.str("");
	cmd_line << PARTEDCMD << device() << " unit cyl print";
	cmd.execute( cmd_line.str() );
	}
    return( ret );
    }

bool
Disk::getPartedSectors( const Partition *p, unsigned long long& start,
                        unsigned long long& end )
    {
    bool ret = false;
    if( getStorage()->test() )
	{
	ret = true;
	start = p->cylStart()*new_head*new_sector;
	end = (p->cylEnd()+1)*new_head*new_sector-1;
	}
    else
	{
	std::ostringstream cmd_line;
	cmd_line << PARTEDCMD << device() << " unit s print | grep -w ^" << p->nr();
	SystemCmd cmd( cmd_line.str() );
	if( cmd.numLines()>0 )
	    {
	    string dummy, s1, s2;
	    std::istringstream data( *cmd.getLine(0) );
	    data >> dummy >> s1 >> s2;
	    y2milestone( "dummy:\"%s\" s1:\"%s\" s2:\"%s\"", dummy.c_str(),
	                 s1.c_str(), s2.c_str() );
	    start = end = 0;
	    s1 >> start;
	    s2 >> end;
	    y2milestone( "start:%llu end:%llu", start, end );
	    ret = end>0;
	    }
	}
    return( ret );
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
	y2milestone( "doCreate container %s name %s", name().c_str(),
		     p->name().c_str() );
	y2milestone( "doCreate nr:%d start %ld len %ld", p->nr(),
	             p->cylStart(), p->cylSize() );
	y2milestone( "doCreate detected_label:%s label:%s",
	             detected_label.c_str(), label.c_str() );
	if( detected_label != label )
	    {
	    ret = doCreateLabel();
	    if( ret==0 )
		detected_label = label;
	    }
	std::ostringstream cmd_line;
	if( ret==0 )
	    {
	    cmd_line << PARTEDCMD << device() << " unit cyl mkpart ";
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
	    unsigned long maxc = cylinders()-1;
	    if( p->type()==LOGICAL )
		{
		PartPair ext = partPair(isExtended);
		if( !ext.empty() )
		    maxc = ext.begin()->cylEnd();
		}
	    y2milestone( "max %lu end:%lu", maxc, end );
	    y2mil( "pp " << *p );
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
	    y2milestone( "max %lu", maxc );
	    if( new_cyl!=cyl )
		{
		y2milestone( "parted geometry changed old c:%lu h:%u s:%u",
		             cyl, head, sector );
		y2milestone( "parted geometry changed new c:%lu h:%u s:%u",
		             new_cyl, new_head, new_sector );
		y2milestone( "old start:%lu end:%lu", start, end );
		start = start * new_cyl / cyl;
		end = end * new_cyl / cyl;
		y2milestone( "new start:%lu end:%lu", start, end );
		}
	    if( end>maxc && maxc<=cylinders()-1 )
		{
		y2milestone( "corrected end from %lu to max %lu", end, maxc );
		end = maxc;
		}
	    cmd_line << start << " " << end;
	    if( execCheckFailed( cmd_line.str() ) )
		{
		ret = DISK_CREATE_PARTITION_PARTED_FAILED;
		}
	    }
	if( ret==0 )
	    {
	    if( p->type()!=EXTENDED )
		getStorage()->waitForDevice( p->device() );
	    else
		getStorage()->waitForDevice();
	    if( p->type()==LOGICAL && getStorage()->instsys() )
		{
		// kludge to make the extended partition visible in
		// /proc/partitions otherwise grub refuses to install if root
		// filesystem is a logical partition
		PartPair lc = partPair(logicalCreated);
		call_blockdev = lc.length()<=1;
		y2milestone( "logicalCreated:%d call_blockdev:%d",
		             lc.length(), call_blockdev );
		}
	    p->setCreated( false );
	    if( !getPartedValues( p ))
		ret = DISK_PARTITION_NOT_FOUND;
	    }
#if 0
	if( ret==0 && p->type()!=EXTENDED )
	    {
	    getStorage()->checkDeviceExclusive( p->device(), 3 );
	    }
#endif
	if( ret==0 )
	    {
	    bool used_as_pv = p->getUsedByType()==UB_EVMS ||
	                      p->getUsedByType()==UB_LVM;
	    y2milestone( "zeroNew:%d used_as_pv:%d", 
	                 getStorage()->getZeroNewPartitions(), used_as_pv );
	    if( used_as_pv || getStorage()->getZeroNewPartitions() )
		{
		string cmd;
		SystemCmd c;
		cmd = "dd if=/dev/zero of=" + p->device() + " bs=1k count=200";
		c.execute( cmd );
		cmd = "dd if=/dev/zero of=" + p->device() +
		      " seek=" + decString(p->sizeK()-10) +
		      " bs=1k count=10";
		c.execute( cmd );
		}
	    else if( !p->getFormat() )
		{
		p->updateFsData();
		}
	    }
	if( ret==0 && p->id()!=Partition::ID_LINUX )
	    {
	    ret = doSetType( p );
	    }
	if( call_blockdev )
	    {
	    SystemCmd c( "/sbin/blockdev --rereadpt " + device() );
	    if( p->type()!=EXTENDED )
		getStorage()->waitForDevice( p->device() );
	    }
	}
    else
	{
	ret = DISK_CREATE_PARTITION_INVALID_VOLUME;
	}
    y2milestone( "ret:%d", ret );
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
	y2milestone( "doRemove container %s name %s", name().c_str(),
		     p->name().c_str() );
	getStorage()->removeDmMapsTo( getPartName(p->OrigNr()) );
	ret = v->prepareRemove();
	if( ret==0 && !p->created() )
	    {
	    std::ostringstream cmd_line;
	    cmd_line << PARTEDCMD << device() << " rm " << p->OrigNr();
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
	if( ret==0 )
	    getStorage()->waitForDevice();
	}
    else
	{
	ret = DISK_REMOVE_PARTITION_INVALID_VOLUME;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Disk::resizePartition( Partition* p, unsigned long newCyl )
    {
    y2mil( "newCyl:" << newCyl << " p:" << *p );
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
	y2mil( "newCyl:" << newCyl << " p->cylSize():" << p->cylSize() );
	if( ret==0 && newCyl>p->cylSize() )
	    {
	    unsigned long increase = newCyl - p->cylSize();
	    PartPair pp = partPair( isExtended );
	    unsigned long start = p->cylEnd()+1;
	    unsigned long end = cylinders();
	    if( p->type()==LOGICAL && !pp.empty() )
		end = pp.begin()->cylEnd()+1;
	    pp = partPair( notDeleted );
	    PartIter i = pp.begin();
	    while( i != pp.end() )
		{
		if( i->type()==p->type() && i->cylStart()>=start &&
		    i->cylStart()<end )
		    end = i->cylStart();
		++i;
		}
	    unsigned long free = 0;
	    if( end>start )
		free = end-start;
	    y2milestone( "free cylinders after %lu SizeK:%llu Extend:%lu",
			 free, cylinderToKb(free), increase );
	    if( cylinderToKb(free) < increase )
		ret = DISK_RESIZE_NO_SPACE;
	    else
		{
		if( p->created() )
		    p->changeRegion( p->cylStart(), newCyl, newSize );
		else
		    p->setResizedSize( newSize );
		}
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Disk::removeVolume( Volume* v )
    {
    return( removePartition( v->nr() ));
    }

bool Disk::isLogical( unsigned nr ) const
    {
    bool ret = ext_possible && nr>max_primary;
    y2milestone( "nr:%u ret:%d", nr, ret );
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
	if( p->isMounted() )
	    {
	    ret = p->umount();
	    if( ret==0 )
		remount = true;
	    }
	if( ret==0 && !needExtend && p->getFs()!=VFAT && p->getFs()!=FSNONE )
	    ret = p->resizeFs();
	if( ret==0 )
	    {
	    system_stderr.erase();
	    y2milestone( "doResize container %s name %s", name().c_str(),
			 p->name().c_str() );
	    std::ostringstream cmd_line;
	    unsigned long long start_sect, end_sect;
	    getPartedSectors( p, start_sect, end_sect );
	    end_sect = start_sect + p->sizeK()*2 - 1;
	    y2milestone( "end_sect %llu", end_sect );
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
		PartPair ext = partPair(isExtended);
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
	    cmd_line << "YAST_IS_RUNNING=1 " << PARTEDCMD << device()
	             << " unit s resize " << p->nr() << " "
	             << start_sect << " " << end_sect;
	    if( execCheckFailed( cmd_line.str() ) )
		{
		ret = DISK_RESIZE_PARTITION_PARTED_FAILED;
		}
	    if( ret==0 )
		getStorage()->waitForDevice( p->device() );
	    if( !getPartedValues( p ))
		{
		if( ret==0 )
		    ret = DISK_PARTITION_NOT_FOUND;
		}
	    y2milestone( "after resize size:%llu resize:%d", p->sizeK(),
	                 p->needShrink()||p->needExtend() );
	    }
	if( needExtend && p->getFs()!=VFAT && p->getFs()!=FSNONE )
	    ret = p->resizeFs();
	if( ret==0 && remount )
	    ret = p->mount();
	}
    else
	{
	ret = DISK_RESIZE_PARTITION_INVALID_VOLUME;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

const Partition * Disk::getPartitionAfter( const Partition * p )
    {
    const Partition * ret = NULL;
    y2mil( "p:" << *p );
    PartPair pp = partPair( (p->type()==LOGICAL)?notDeleted:notDeletedLog );
    for( PartIter pi=pp.begin(); pi!=pp.end(); ++pi )
	{
	if( !pi->created() &&
	    pi->cylStart()>p->cylStart() &&
	    (ret==NULL || ret->cylStart()>p->cylStart()) )
	    ret = &(*pi);
	}
    if( ret==NULL )
	y2milestone( "ret:NULL" );
    else
	y2mil( "ret:" << *ret );
    return( ret );
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
    info.udevPath = udev_path;
    info.udevId = mergeString( udev_id );
    tinfo = info;
    }

namespace storage
{

std::ostream& operator<< (std::ostream& s, const Disk& d )
    {
    s << *((Container*)&d);
    s << " Cyl:" << d.cyl
      << " Head:" << d.head
      << " Sect:" << d.sector
      << " Node <" << d.mjr
      << ":" << d.mnr << ">"
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
    if( !d.mp_alias.empty() )
	s << " MpAlias:" << d.mp_alias;
    s << " MaxPrimary:" << d.max_primary;
    if( d.ext_possible )
	s << " ExtPossible MaxLogical:" << d.max_logical;
    if( d.init_disk )
	s << " InitDisk";
    return( s );
    }

}

void Disk::logDifference( const Disk& d ) const
    {
    string log = Container::logDifference( d );
    if( cyl!=d.cyl )
	log += " Cyl:" + decString(cyl) + "-->" + decString(d.cyl);
    if( head!=d.head )
	log += " Head:" + decString(head) + "-->" + decString(d.head);
    if( sector!=d.sector )
	log += " Sect:" + decString(sector) + "-->" + decString(d.sector);
    if( mjr!=d.mjr )
	log += " Mjr:" + decString(mjr) + "-->" + decString(d.mjr);
    if( mnr!=d.mnr )
	log += " Mnr:" + decString(mnr) + "-->" + decString(d.mnr);
    if( range!=d.range )
	log += " Range:" + decString(range) + "-->" + decString(d.range);
    if( size_k!=d.size_k )
	log += " SizeK:" + decString(size_k) + "-->" + decString(d.size_k);
    if( label!=d.label )
	log += " Label:" + label + "-->" + d.label;
    if( sysfs_dir!=d.sysfs_dir )
	log += " SysfsDir:" + sysfs_dir + "-->" + d.sysfs_dir;
    if( max_primary!=d.max_primary )
	log += " MaxPrimary:" + decString(max_primary) + "-->" + decString(d.max_primary);
    if( ext_possible!=d.ext_possible )
	{
	if( d.ext_possible )
	    log += " -->ExtPossible";
	else
	    log += " ExtPossible-->";
	}
    if( max_logical!=d.max_logical )
	log += " MaxLogical:" + decString(max_logical) + "-->" + decString(d.max_logical);
    if( init_disk!=d.init_disk )
	{
	if( d.init_disk )
	    log += " -->InitDisk";
	else
	    log += " InitDisk-->";
	}
    y2milestone( "%s", log.c_str() );
    ConstPartPair p=partPair();
    ConstPartIter i=p.begin();
    while( i!=p.end() )
	{
	ConstPartPair pc=d.partPair();
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
    p=d.partPair();
    i=p.begin();
    while( i!=p.end() )
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

bool Disk::equalContent( const Disk& rhs ) const
    {
    bool ret = Container::equalContent(rhs) &&
	       cyl==rhs.cyl && head==rhs.head && sector==rhs.sector &&
	       mjr==rhs.mjr && mnr==rhs.mnr && range==rhs.range &&
	       size_k==rhs.size_k && max_primary==rhs.max_primary &&
	       ext_possible==rhs.ext_possible && max_logical==rhs.max_logical &&
	       init_disk==rhs.init_disk && label==rhs.label &&
	       sysfs_dir==rhs.sysfs_dir;
    if( ret )
	{
	ConstPartPair p = partPair();
	ConstPartPair pc = rhs.partPair();
	ConstPartIter i = p.begin();
	ConstPartIter j = pc.begin();
	while( ret && i!=p.end() && j!=pc.end() )
	    {
	    ret = ret && i->equalContent( *j );
	    ++i;
	    ++j;
	    }
	ret == ret && i==p.end() && j==pc.end();
	}
    return( ret );
    }

Disk& Disk::operator= ( const Disk& rhs )
    {
    y2debug( "operator= from %s", rhs.nm.c_str() );
    cyl = rhs.cyl;
    head = rhs.head;
    sector = rhs.sector;
    new_cyl = rhs.new_cyl;
    new_head = rhs.new_head;
    new_sector = rhs.new_sector;
    label = rhs.label;
    detected_label = rhs.detected_label;
    mjr = rhs.mjr;
    mnr = rhs.mnr;
    range = rhs.range;
    size_k = rhs.size_k;
    byte_cyl = rhs.byte_cyl;
    max_primary = rhs.max_primary;
    ext_possible = rhs.ext_possible;
    max_logical = rhs.max_logical;
    init_disk = rhs.init_disk;
    udev_path = rhs.udev_path;
    udev_id = rhs.udev_id;
    mp_alias = rhs.mp_alias;
    logfile_name = rhs.logfile_name;
    sysfs_dir = rhs.sysfs_dir;
    return( *this );
    }

Disk::Disk( const Disk& rhs ) : Container(rhs)
    {
    y2debug( "constructed disk by copy constructor from %s", rhs.nm.c_str() );
    *this = rhs;
    ConstPartPair p = rhs.partPair();
    for( ConstPartIter i = p.begin(); i!=p.end(); ++i )
	{
	Partition * p = new Partition( *this, *i );
	vols.push_back( p );
	}
    }

