#include <iostream>

#include <ycp/y2log.h>

#include <string>
#include <sstream>
#include <iomanip>

#include <fcntl.h>
#include <sys/mount.h>         /* for BLKGETSIZE */
#include <linux/hdreg.h>       /* for HDIO_GETGEO */
#include <linux/fs.h>          /* for BLKGETSIZE64 */

#include "y2storage/Region.h"
#include "y2storage/Partition.h"
#include "y2storage/ProcPart.h"
#include "y2storage/Disk.h"
#include "y2storage/Storage.h"
#include "y2storage/AsciiFile.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"

#define PARTEDCMD "/usr/sbin/parted -s "  // blank at end !!

Disk::Disk( Storage * const s, const string& Name,
            unsigned long long SizeK ) :
    Container(s,Name,staticType())
    {
    size_k = SizeK;
    y2milestone( "constructed disk %s", dev.c_str() );
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
    rdonly = false;
    if( searchFile( file, "^Readonly:", line ) )
	{
	extractNthWord( 1, line ) >> rdonly;
	}
    size_k = 0;
    if( searchFile( file, "^SizeK:", line ) )
	{
	extractNthWord( 1, line ) >> size_k;
	}
    int lnr = 0;
    while( searchFile( file, "^Partition:", line, lnr ))
	{
	lnr++;
	Partition *p = new Partition( *this, extractNthWord( 1, line, true ));
	addToList( p );
	}
    y2milestone( "constructed disk %s from file %s", dev.c_str(), fname.c_str() );
    }


Disk::~Disk()
    {
    y2milestone( "destructed disk %s", dev.c_str() );
    }

unsigned long long
Disk::cylinderToKb( unsigned long cylinder )
    {
    return (unsigned long long)byte_cyl * cylinder / 1024;
    }

unsigned long
Disk::kbToCylinder( unsigned long long kb )
    {
    unsigned long long bytes = kb * 1024;
    bytes += byte_cyl - 1;
    unsigned long ret = bytes/byte_cyl;
    y2milestone( "KB:%lld ret:%ld byte_cyl:%ld", kb, ret, byte_cyl );
    return (ret);
    }

unsigned long long
Disk::capacityInKb()
    {
    return (unsigned long long)byte_cyl * cyl / 1024;
    }

bool Disk::detectGeometry()
    {
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
	y2milestone( "BLKGETSIZE64 Ret:%d Bytes:%llu", rcode, sect );
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
    string SysfsFile = SysfsDir+"/range";
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
    SysfsFile = SysfsDir+"/dev";
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

bool Disk::detectPartitions()
    {
    bool ret = true;
    string cmd_line = PARTEDCMD + device() + " print | sort -n";
    string dlabel;
    system_stderr.erase();
    y2milestone( "executing cmd:%s", cmd_line.c_str() );
    SystemCmd Cmd( cmd_line );
    checkSystemError( cmd_line, Cmd );
    if( Cmd.select( "Disk label type:" )>0 )
	{
	string tmp = *Cmd.getLine(0, true);
	y2milestone( "Label line:%s", tmp.c_str() );
	dlabel = extractNthWord( 3, tmp );
	}
    y2milestone( "Label:%s", dlabel.c_str() );
    setLabelData( dlabel );
    checkPartedOutput( Cmd );
    if( dlabel.size()==0 )
	{
	Cmd.setCombine();
	Cmd.execute( "/sbin/fdisk -l " + device() );
	if( Cmd.select( "AIX label" )>0 )
	    {
	    detected_label = "aix";
	    }
	}
    detected_label = dlabel;
    if( dlabel.size()==0 )
	dlabel = defaultLabel();
    setLabelData( dlabel );
    y2milestone( "ret:%d partitons:%zd detected label:%s label:%s", ret,
                 vols.size(), detected_label.c_str(), label.c_str() );
    return( ret );
    }

void
Disk::logData( const string& Dir )
    {
    string fname( Dir + "/disk_" + name() );
    ofstream file( fname.c_str() );
    file << "Device: " << dev << endl;
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

    if( rdonly )
	{
	file << "Readonly: " << rdonly << endl;
	}
    file << "SizeK: " << size_k << endl;


    PartPair pp = partPair();
    for( PartIter p=pp.begin(); p!=pp.end(); ++p )
	{
	file << "Partition: ";
	p->logData(file);
	file << endl;
	}
    }

void
Disk::setLabelData( const string& disklabel )
    {
    y2milestone( "disklabel:%s", disklabel.c_str() );
    int i=0;
    while( labels[i].name.size()>0 && labels[i].name!=disklabel )
	{
	i++;
	}
    if( labels[i].name.size()==0 )
	i=0;
    ext_possible = labels[i].extended;
    max_primary = labels[i].primary;
    max_logical = labels[i].logical;
    label = labels[i].name;
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
	if( system_stderr.size()>0 )
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
	if( system_stderr.size()>0 )
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
    int ret = 0;
    if( !sto->test() )
	{
	cmd.execute( cmd_line );
	ret = checkSystemError( cmd_line, cmd );
	}
    else
	{
	y2milestone( "TESTMODE exec cmdline:\"%s\"", cmd_line.c_str() );
	}
    return( ret );
    }

bool
Disk::scanPartedLine( const string& Line, unsigned& nr, unsigned long& start,
                      unsigned long& csize, PartitionType& type,
		      string& parted_start, unsigned& id, bool& boot )
    {
    double StartM, EndM;
    string PartitionType, TInfo;

    y2debug( "Line: %s", Line.c_str() );
    std::istringstream Data( Line );

    nr=0;
    StartM = EndM = 0.0;
    type = PRIMARY;
    if( label == "msdos" )
	{
	Data >> nr >> StartM >> EndM >> PartitionType;
	}
    else
	{
	Data >> nr >> StartM >> EndM;
	}
    std::ostringstream Buf;
    Buf << std::setprecision(3) << std::setiosflags(std::ios_base::fixed)
	<< StartM;
    parted_start = Buf.str().c_str();
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
    y2milestone( "Fields Num:%d Start:%5.2f End:%5.2f Type:%d",
		 nr, StartM, EndM, type );
    int Add = cylinderToKb(1)*2/5;
    if( nr>0 )
      {
      start = kbToCylinder( (unsigned long long)(StartM*1024)+Add );
      csize = kbToCylinder( (unsigned long long)(EndM*1024)+Add ) - start;
      id = Partition::ID_LINUX;
      boot = TInfo.find( ",boot," ) != string::npos;
      string OrigTInfo = TInfo;
      for( string::iterator i=TInfo.begin(); i!=TInfo.end(); i++ )
	  {
	  *i = tolower(*i);
	  }
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
		  if( val.find( "Apple_partition" ) != string::npos ||
		      val.find( "Apple_Driver" ) != string::npos ||
		      val.find( "Apple_Loader" ) != string::npos ||
		      val.find( "Apple_Boot" ) != string::npos ||
		      val.find( "Apple_ProDOS" ) != string::npos ||
		      val.find( "Apple_FWDriver" ) != string::npos ||
		      val.find( "Apple_Patches" ) != string::npos )
		      {
		      id = Partition::ID_APPLE_OTHER;
		      }
		  else if( val.find( "Apple_HFS" ) != string::npos )
		      {
		      id = Partition::ID_APPLE_HFS;
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
	  }
      y2milestone( "Fields Num:%d Id:%x Ptype:%d Start:%ld Size:%ld",
		   nr, id, type, start, csize );
      }
   return( nr>0 );
   }

bool
Disk::checkPartedOutput( const SystemCmd& Cmd )
    {
    int cnt;
    string line;
    string tmp;
    ProcPart ppart;
    unsigned long range_exceed = 0;
    list<Partition *> pl;

    cnt = Cmd.numLines();
    for( int i=0; i<cnt; i++)
	{
	unsigned pnr;
	unsigned long c_start;
	unsigned long c_size;
	PartitionType type;
	string p_start;
	unsigned id;
	bool boot;

	line = *Cmd.getLine(i);
	tmp = extractNthWord( 0, line );
	if( tmp.length()>0 && isdigit(tmp[0]) )
	    {
	    if( scanPartedLine( line, pnr, c_start, c_size, type, p_start, id,
	                        boot ))
		{
		if( pnr<range )
		    {
		    unsigned long long s = cylinderToKb(c_size);
		    Partition *p = new Partition( *this, pnr, s,
						  c_start, c_size, type,
						  p_start, id, boot );
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
    list<string> ps = ppart.getMatchingEntries( nm + "p?[0-9]+" );
    if( !checkPartedValid( ppart, ps, pl ) )
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
	    if( ppart.getSize( *i, s ))
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
			               type,
				       decString(cylinderToKb(cyl_start-1)),
				       id, false );
		    pl.push_back( p );
		    }
		else
		    range_exceed = max( range_exceed, (unsigned long)pr.second );
		cyl_start += cyl;
		}
	    }
	// popup text %1$s is replaced by disk name e.g. /dev/hda
	string txt = sformat(
_("The partitioning on your disk %1$s is not readable by\n"
"the partitioning tool \"parted\" that YaST2 uses to change the\n"
"partition table.\n"
"\n"
"You may use the partitions on disk %1$s as they are.\n"
"You may format them and assign mount points to them, but you\n"
"cannot add, edit, resize, or remove partitions from that\n"
"disk with YaST2."), dev.c_str() );

	// TODO: handle callback into ycp code for error popup
	y2milestone( "parted invalid:%s", txt.c_str() );
	rdonly = true;
	}
    if( range_exceed>0 )
	{
	// popup text %1$s is replaced by disk name e.g. /dev/hda
	//            %2$lu and %3$lu are replaced by numbers.
	string txt = sformat(
_("Your disk %1$s contains %2$lu partitions. The maximal number\n"
"of partitions to handle by the kernel driver of disk is %3$lu.\n"
"You will not be able to access partitions numbered above %3$lu."),
                              (char*)dev.c_str(), range_exceed, range-1 );
	// TODO: handle callback into ycp code for error popup
	y2milestone( "range_exceed:%s", txt.c_str() );
	}
    for( list<Partition*>::iterator i=pl.begin(); i!=pl.end(); ++i )
	{
	addToList( *i );
	}
    return( true );
    }

bool Disk::checkPartedValid( const ProcPart& pp, const list<string>& ps,
                             const list<Partition*>& pl )
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
    if( proc_l.size()==parted_l.size() ||
        ((openbsd=haveBsdPart()) && proc_l.size()>parted_l.size()) )
	{
	map<unsigned,unsigned long>::const_iterator i, j;
	for( i=proc_l.begin(); i!=proc_l.end(); i++ )
	    {
	    j=parted_l.find(i->first);
	    if( j==parted_l.end() )
		ret = ret && openbsd;
	    else
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
	ret = false;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

static bool isBsdPart( const Partition& p )
    { return( p.id()==0xa5 || p.id()==0xa6 || p.id()==0xeb ); }

bool Disk::haveBsdPart() const
    {
    return( !partPair(isBsdPart).empty() );
    }

string Disk::defaultLabel()
    {
    string ret = "msdos";
    if( Storage::arch()=="ia64" )
	ret = "gpt";
    else if( Storage::arch()=="sparc" )
	ret = "sun";
    y2milestone( "ret %s", ret.c_str() );
    return( ret );
    }

Disk::label_info Disk::labels[] = {
	{ "msdos", true, 4, 63 },
	{ "gpt", false, 63, 0 },
	{ "bsd", false, 8, 0 },
	{ "sun", false, 8, 0 },
	{ "max", false, 16, 0 },
	{ "aix", false, 0, 0 },
	{ "", false, 0, 0 }
    };

string Disk::p_disks [] = { "cciss/", "ida/", "ataraid/", "etherd/", "rd/" };

bool Disk::needP( const string& disk )
    {
    bool need_p = false;
    unsigned i=0;
    while( !need_p && i<sizeof(p_disks)/sizeof(string) )
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

bool Disk::hasExtended()
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
	ret = type==LOGICAL ? (max_primary+1) : 1;
	}
    else if( type==LOGICAL )
	{
	ret = (--p.end())->nr()+1;
	if( ret<=max_primary )
	    ret = max_primary+1;
	if( !ext_possible || !hasExtended() )
	    ret = 0;
	if( ret>max_logical )
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

int Disk::createPartition( PartitionType type, unsigned long start,
                           unsigned long len, string& device,
			   bool checkRelaxed )
    {
    y2milestone( "begin type %d at %ld len %ld relxed:%d", type, start, len, checkRelaxed );
    unsigned fuzz = checkRelaxed ? 2 : 0;
    int ret = 0;
    Region r( start, len );
    PartPair ext = partPair(isExtended);
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    if( ret==0 && (r.end() > cylinders()+fuzz) )
	{
	y2milestone( "too large for disk cylinders %lu", cylinders() );
	ret = DISK_CREATE_PARTITION_EXCEEDS_DISK;
	}
    if( ret==0 && len==0 )
	{
	ret = DISK_CREATE_PARTITION_ZERO_SIZE;
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
	    std::ostringstream b;
	    b << "r:" << r << " p:" << i->region() << " inter:" << i->region().intersect(r);
	    y2warning( "overlaps %s", b.str().c_str() );
	    ret = DISK_CREATE_PARTITION_OVERLAPS_EXISTING;
	    }
	}
    if( ret==0 && type==LOGICAL && !ext.begin()->isAreaInside( r, fuzz ))
	{
	std::ostringstream b;
	b << "r:" << r << " ext:" << ext.begin()->region() << "inter:" << ext.begin()->region().intersect(r);
	y2warning( "outside ext %s", b.str().c_str() );
	ret = DISK_CREATE_PARTITION_LOGICAL_OUTSIDE_EXT;
	}
    if( ret==0 && type==EXTENDED )
	{
	if( !ext_possible || !ext.empty())
	    {
	    ret = ext_possible ? DISK_CREATE_PARTITION_EXT_ONLY_ONCE
	                       : DISK_CREATE_PARTITION_EXT_IMPOSSIBLE;
	    }
	}
    int number = 0;
    if( ret==0 )
	{
	number = availablePartNumber( type );
	if( number==0 )
	    {
	    ret = DISK_CREATE_PARTITION_NO_FREE_NUMBER;
	    }
	}
    if( ret==0 )
	{
	Partition * p = new Partition( *this, number, cylinderToKb(len), start,
	                               len, type,
				       decString(cylinderToKb(start-1)) );
	p->setCreated();
	device = p->device();
	addToList( p );
	}
    y2milestone( "ret %d", ret );
    return( ret );
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
	ret = DISK_REMOVE_PARTITION_NOT_FOUND;
	}
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    if( ret==0 )
	{
	i->setDeleted();
	if( nr>max_primary )
	    {
	    i = p.begin();
	    while( i!=p.end() )
		{
		if( i->nr()>nr )
		    {
		    i->changeNumber( i->nr()-1 );
		    }
		++i;
		}
	    }
	else if( i->type()==EXTENDED )
	    {
	    i = p.begin();
	    while( i!=p.end() )
		{
		if( i->nr()>max_primary )
		    {
		    i->setDeleted();
		    }
		++i;
		}
	    }
	}
    y2milestone( "ret %d", ret );
    return( ret );
    }

int Disk::destroyPartitionTable( const string& new_label )
    {
    y2milestone( "begin" );
    int ret = 0;
    setLabelData( new_label );
    if( readonly() )
	{
	ret = DISK_CHANGE_READONLY;
	}
    else if( max_primary==0 )
	{
	setLabelData( label );
	ret = DISK_DESTROY_TABLE_INVALID_LABEL;
	}
    else
	{
	setDeleted( true );
	label = new_label;
	VIter i = vols.begin();
	while( i!=vols.end() )
	    {
	    if( (*i)->created() )
		{
		i = vols.erase( i );
		}
	    else
		{
		(*i)->setDeleted( true );
		++i;
		}
	    }
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
	ret = DISK_CHANGE_PARTITION_ID_NOT_FOUND;
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

int Disk::commitChanges( CommitStage stage )
    {
    y2milestone( "name %s stage %d", name().c_str(), stage );
    int ret = 0;
    if( ret==0 )
	{
	switch( stage )
	    {
	    case DECREASE:
		if( deleted() )
		    {
		    ret = doCreateLabel();
		    }
		if( ret==0 )
		    ret = Container::commitChanges( stage );
		break;
	    case INCREASE:
		ret = Container::commitChanges( stage );
		if( ret==0 )
		    {
		    PartPair p = partPair( Partition::toChangeId );
		    PartIter i = p.begin();
		    while( ret==0 && i!=p.end() )
			{
			ret = doSetType( &(*i) );
			++i;
			}
		    }
		break;
	    default:
		ret = Container::commitChanges( stage );
		break;
	    }
	}
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
		i=l.erase( i );
	    else
		++i;
	    }
	l.push_front( new commitAction( DECREASE, staticType(), 
				        setDiskLabelText(false), true ));
	}
    }

string Disk::setDiskLabelText( bool doing ) const
    {
    string txt;
    if( doing )
        {
        // displayed text during action, %1$s is replaced by disk name (e.g. /dev/hda),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Initializing disk label of disk %1$s to %2$s"),
		      dev.c_str(), label.c_str() );
        }
    else
        {
	string d = dev.substr( 5 );
        // displayed text before action, %1$s is replaced by disk name (e.g. hda),
	// %2$s is replaced by label name (e.g. msdos)
        txt = sformat( _("Initialize disk label of disk %1$s to %2$s"),
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
    VolPair p = volPair();
    if( !p.empty() )
	{
	setSilent( true );
	list<VolIterator> l;
	for( VolIterator i=p.begin(); i!=p.end(); ++i )
	    if( !i->created() )
		l.push_front( i );
	for( list<VolIterator>::const_iterator i=l.begin(); i!=l.end(); ++i )
	    {
	    doRemove( &(**i) );
	    }
	setSilent( false );
	}
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
	VIter i = vols.begin();
	while( i!=vols.end() )
	    {
	    if( !(*i)->created() )
		{
		i = vols.erase( i );
		}
	    else
		++i;
	    }
	}
    y2milestone( "ret:%d", ret );
    return( ret );
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
	if( ret==0 && (label=="gpt"||label=="dvh"||label=="mac"))
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
	if( ret==0 )
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
	    p->changeIdDone();
	}
    else
	{
	ret = DISK_SET_TYPE_INVALID_VOLUME;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Disk::doCreate( Volume* v )
    {
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->createText(true) );
	    }
	system_stderr.erase();
	y2milestone( "doCreate container %s name %s", name().c_str(),
		     p->name().c_str() );
	y2milestone( "doCreate nr:%d start %ld len %ld", p->nr(),
	             p->cylStart(), p->cylSize() );
	if( detected_label != label )
	    {
	    ret = doCreateLabel();
	    }
	std::ostringstream cmd_line;
	if( ret==0 )
	    {
	    cmd_line << PARTEDCMD << device() << " mkpart ";
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
	    cmd_line << std::setprecision(3)
		     << std::setiosflags(std::ios_base::fixed)
		     << (double)(p->cylStart()-1)*cylinderToKb(1)/1024
		     << " ";
	    cmd_line << std::setprecision(3)
		     << std::setiosflags(std::ios_base::fixed)
		     << (double)(p->cylStart()+p->cylSize()-1)*cylinderToKb(1)/1024
		     << " ";
	    if( execCheckFailed( cmd_line.str() ) )
		{
		ret = DISK_CREATE_PARTITION_PARTED_FAILED;
		}
	    }
	if( ret==0 )
	    {
	    p->setCreated( false );
	    std::ostringstream cmd_line;
	    cmd_line << PARTEDCMD << device() << " print | grep -w ^" << p->nr();
	    SystemCmd cmd( cmd_line.str() );
	    unsigned nr, id;
	    unsigned long start, csize;
	    PartitionType type;
	    string pstart;
	    bool boot;
	    if( cmd.numLines()>0 &&
	        scanPartedLine( *cmd.getLine(0), nr, start, csize, type,
		                pstart, id, boot ))
		{
		y2milestone( "really created at cyl:%ld csize:%ld, pstart:%s",
		             start, csize, pstart.c_str() );
		p->changePartedStart( pstart );
		p->changeRegion( start, csize, cylinderToKb(csize) );
		}
	    else
		{
		ret = DISK_CREATE_PARTITION_NOT_FOUND;
		}
	    }
	if( ret==0 && p->id()!=Partition::ID_LINUX )
	    {
	    ret = doSetType( p );
	    }
	}
    else
	{
	ret = DISK_CREATE_PARTITION_INVALID_VOLUME;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int Disk::doResize( Volume* v )
    {
    Partition * p = dynamic_cast<Partition *>(v);
    int ret = 0;
    if( p != NULL )
	{
	if( !silent )
	    {
	    getStorage()->showInfoCb( p->resizeText(true) );
	    }
	system_stderr.erase();
	y2milestone( "doResize container %s name %s", name().c_str(),
		     p->name().c_str() );
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
	    VIter pi = vols.begin();
	    while( pi!=vols.end() && *pi != p )
		++pi;
	    if( pi!=vols.end() )
		{
		vols.erase( pi );
		}
	    else
		{
		ret = DISK_REMOVE_PARTITION_LIST_ERASE;
		}
	    }
	}
    else
	{
	ret = DISK_RESIZE_PARTITION_INVALID_VOLUME;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

