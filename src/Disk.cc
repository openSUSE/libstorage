#include <iostream> 

#include <ycp/y2log.h>

#include <string>
#include <sstream>
#include <iomanip>

#include <fcntl.h>
#include <sys/mount.h>         /* for BLKGETSIZE */
#include <linux/hdreg.h>       /* for HDIO_GETGEO */
#include <linux/fs.h>          /* for BLKGETSIZE64 */

#include "y2storage/Partition.h"
#include "y2storage/ProcPart.h"
#include "y2storage/Disk.h"
#include "y2storage/Storage.h"
#include "y2storage/AsciiFile.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"

#define PARTEDCMD "/usr/sbin/parted -s "  // blank at end !!

Disk::Disk( const Storage * const s, const string& Name, 
            unsigned long long SizeK ) : 
    Container(s,Name,StaticType())
    {
    size_k = SizeK;
    y2milestone( "constructed disk %s", dev.c_str() );
    }

Disk::Disk( const Storage * const s, const string& fname ) :
    Container(s,"",StaticType())
    {
    name = fname.substr( fname.find_last_of( '/' )+1);
    if( name.find("disk_")==0 )
	name.erase( 0, 5 );
    AsciiFile file( fname );
    string line;
    if( SearchFile( file, "^Device:", line ) )
	{
	dev = ExtractNthWord( 1, line );
	}
    minor = major = 0;
    if( SearchFile( file, "^Major:", line ) )
	{
	ExtractNthWord( 1, line ) >> major;
	}
    if( SearchFile( file, "^Minor:", line ) )
	{
	ExtractNthWord( 1, line ) >> minor;
	}
    range = 4;
    if( SearchFile( file, "^Range:", line ) )
	{
	ExtractNthWord( 1, line ) >> range;
	}
    cylinders = 1024;
    if( SearchFile( file, "^Cylinder:", line ) )
	{
	ExtractNthWord( 1, line ) >> cylinders;
	}
    heads = 1024;
    if( SearchFile( file, "^Head:", line ) )
	{
	ExtractNthWord( 1, line ) >> heads;
	}
    sectors = 32;
    if( SearchFile( file, "^Sector:", line ) )
	{
	ExtractNthWord( 1, line ) >> sectors;
	}
    if( SearchFile( file, "^Label:", line ) )
	{
	label = ExtractNthWord( 1, line );
	}
    max_primary = 0;
    if( SearchFile( file, "^MaxPrimary:", line ) )
	{
	ExtractNthWord( 1, line ) >> max_primary;
	}
    ext_possible = false;
    max_logical = 0;
    if( SearchFile( file, "^ExtPossible:", line ) )
	{
	ExtractNthWord( 1, line ) >> ext_possible;
	}
    if( SearchFile( file, "^MaxLogical:", line ) )
	{
	ExtractNthWord( 1, line ) >> max_logical;
	}
    readonly = false;
    if( SearchFile( file, "^Readonly:", line ) )
	{
	ExtractNthWord( 1, line ) >> readonly;
	}
    size_k = 0;
    if( SearchFile( file, "^SizeK:", line ) )
	{
	ExtractNthWord( 1, line ) >> size_k;
	}
    int lnr = 0;
    while( SearchFile( file, "^Partition:", line, lnr ))
	{
	lnr++;
	Partition *p = new Partition( *this, ExtractNthWord( 1, line, true ));
	AddToList( p );
	}
    y2milestone( "constructed disk %s from file %s", dev.c_str(), fname.c_str() );
    }


Disk::~Disk()
    {
    y2milestone( "destructed disk %s", dev.c_str() );
    }

unsigned long long
Disk::CylinderToKb( unsigned long cylinder )
    {
    return (unsigned long long)byte_cyl * cylinder / 1024;
    }

unsigned long
Disk::KbToCylinder( unsigned long long kb )
    {
    unsigned long long bytes = kb * 1024;
    bytes += byte_cyl - 1;
    return (bytes/byte_cyl);
    }

unsigned long long
Disk::CapacityInKb()
    {
    return (unsigned long long)byte_cyl * cylinders / 1024;
    }

bool Disk::DetectGeometry()
    {
    bool ret = false;
    int fd = open( Device().c_str(), O_RDONLY );
    if( fd >= 0 )
	{
	heads = 255;
	sectors = 63;
	cylinders = 16;
	struct hd_geometry geometry;
	int rcode = ioctl( fd, HDIO_GETGEO, &geometry );
	if( rcode==0 )
	    {
	    heads = geometry.heads>0?geometry.heads:heads;
	    sectors = geometry.sectors>0?geometry.sectors:sectors;
	    cylinders = geometry.cylinders>0?geometry.cylinders:cylinders;
	    }
	y2milestone( "After HDIO_GETGEO ret %d Head:%u Sector:%u Cylinder:%lu",
		     rcode, heads, sectors, cylinders );
	__uint64_t sect = 0;
	rcode = ioctl( fd, BLKGETSIZE64, &sect);
	y2milestone( "BLKGETSIZE64 Ret:%d Bytes:%llu", rcode, sect );
	if( rcode==0 && sect!=0 )
	    {
	    sect /= 512;
	    cylinders = (unsigned)(sect / (__uint64_t)(heads*sectors));
	    ret = true;
	    }
	else
	    {
	    unsigned long lsect;
	    rcode = ioctl( fd, BLKGETSIZE, &lsect );
	    y2milestone( "BLKGETSIZE Ret:%d Sect:%lu", rcode, lsect );
	    if( rcode==0 && lsect!=0 )
		{
		cylinders = lsect / (unsigned long)(heads*sectors);
		ret = true;
		}
	    }
	y2milestone( "After getsize Cylinder:%lu", cylinders );
	close( fd );
	}
    byte_cyl = heads * sectors * 512;
    y2milestone( "ret:%d byte_cyl:%lu", ret, byte_cyl );
    return( ret );
    }

bool Disk::GetSysfsInfo( const string& SysfsDir )
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
	File >> major;
	File >> c;
	File >> minor;
	}
    else
	{
	ret = false;
	}
    y2milestone( "Ret:%d Range:%ld Major:%ld Minor:%ld", ret, range, major, 
                 minor );
    return( ret );
    }

bool Disk::DetectPartitions()
    {
    bool ret = true;
    string cmd_line = PARTEDCMD + Device() + " print | sort -n";
    string dlabel;
    system_stderr.erase();
    y2milestone( "executing cmd:%s", cmd_line.c_str() );
    SystemCmd Cmd( cmd_line, true );
    CheckSystemError( cmd_line, Cmd );
    if( Cmd.Select( "Disk label type:" )>0 )
	{
	string tmp = *Cmd.GetLine(0, true);
	y2milestone( "Label line:%s", tmp.c_str() );
	dlabel = ExtractNthWord( 3, tmp );
	}
    y2milestone( "Label:%s", dlabel.c_str() );
    SetLabelData( dlabel );
    CheckPartedOutput( Cmd );
    if( dlabel.size()==0 )
	{
	Cmd.SetCombine();
	Cmd.Execute( "/sbin/fdisk -l " + Device() );
	if( Cmd.Select( "AIX label" )>0 )
	    {
	    dlabel = "aix";
	    }
	}
    if( dlabel.size()==0 )
	dlabel = DefaultLabel();
    SetLabelData( dlabel );
    y2milestone( "ret:%d partitons:%d label:%s", ret, vols.size(), 
                 dlabel.c_str() );
    return( ret );
    }

void
Disk::LogData( const string& Dir )
    {
    string fname( Dir + "/disk_" + Name() );
    ofstream file( fname.c_str() );
    file << "Device: " << dev << endl;
    file << "Major: " << major << endl;
    file << "Minor: " << minor << endl;
    file << "Range: " << range << endl;

    file << "Cylinder: " << cylinders << endl;
    file << "Head: " << heads << endl;
    file << "Sector: " << sectors << endl;

    file << "Label: " << label << endl;
    file << "MaxPrimary: " << max_primary << endl;
    if( ext_possible )
	{
	file << "ExtPossible: " << ext_possible << endl;
	file << "MaxLogical: " << max_logical << endl;
	}

    if( readonly )
	{
	file << "Readonly: " << readonly << endl;
	}
    file << "SizeK: " << size_k << endl;

    for( ConstPlainIterator i=begin(); i!=end(); i++ )
	{
	const Partition *p = static_cast<const Partition *>(*i);
	file << "Partition: ";
	p->LogData(file);
	file << endl;
	}
    }

void
Disk::SetLabelData( const string& disklabel )
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

void
Disk::CheckSystemError( const string& cmd_line, const SystemCmd& cmd )
    {
    string tmp = *cmd.GetString(SystemCmd::IDX_STDERR);
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
    tmp = *cmd.GetString(SystemCmd::IDX_STDOUT);
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
    if( cmd.Retcode() != 0 )
        {
        y2error( "retcode:%d", cmd.Retcode() );
        }
    }

bool
Disk::ScanPartedLine( const string& Line, unsigned& nr, unsigned long& start, 
                      unsigned long& csize, Partition::PType& type, 
		      string& parted_start, unsigned& id, bool& boot )
    {
    double StartM, EndM;
    string PartitionType, TInfo;

    y2debug( "Line: %s", Line.c_str() );
    std::istringstream Data( Line );

    nr=0;
    StartM = EndM = 0.0;
    type = Partition::PRIMARY;
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
    int Add = CylinderToKb(1)*2/5;
    if( nr>0 )
      {
      start = KbToCylinder( (unsigned long long)(StartM*1024)+Add );
      csize = KbToCylinder( (unsigned long long)(EndM*1024)+Add ) - start;
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
	      type = Partition::EXTENDED;
	      id = 0x0f;
	      }
	  else if( nr>=5 )
	      {
	      type = Partition::LOGICAL;
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
	  id = Partition::ID_MD;
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
		      id = 0x101;
		      }
		  else if( val.find( "Apple_HFS" ) != string::npos )
		      {
		      id = 0x102;
		      }
		  }
	      }
	  }
      if( label == "gpt" )
	  {
	  if( TInfo.find( ",boot," ) != string::npos )
	      {
	      id = 0x103;
	      }
	  if( TInfo.find( ",hp-service," ) != string::npos )
	      {
	      id = 0x104;
	      }
	  }
      y2milestone( "Fields Num:%d Id:%x Ptype:%d Start:%ld Size:%ld",
		   nr, id, type, start, csize );
      }
   return( nr>0 );
   }

bool
Disk::CheckPartedOutput( const SystemCmd& Cmd )
    {
    int cnt;
    string line;
    string tmp;
    ProcPart ppart;
    unsigned long range_exceed = 0;
    list<Partition *> pl;

    cnt = Cmd.NumLines();
    for( int i=0; i<cnt; i++)
	{
	unsigned pnr;
	unsigned long c_start;
	unsigned long c_size;
	Partition::PType type;
	string p_start;
	unsigned id;
	bool boot;

	line = *Cmd.GetLine(i);
	tmp = ExtractNthWord( 0, line );
	if( tmp.length()>0 && isdigit(tmp[0]) )
	    {
	    if( ScanPartedLine( line, pnr, c_start, c_size, type, p_start, id, 
	                        boot ))
		{
		if( pnr<range )
		    {
		    unsigned long major;
		    unsigned long minor;
		    unsigned long long s = CylinderToKb(c_size);
		    Partition *p = new Partition( *this, pnr, 
		                                  CylinderToKb(c_size),
						  c_start, c_size, type, 
						  p_start, id, boot );
		    if( ppart.GetInfo( p->Device(), s, major, minor ))
			{
			if( s>0 && p->Type() != Partition::EXTENDED )
			    p->SetSize( s );
			p->SetMajorMinor( major, minor );
			}
		    pl.push_back( p );
		    }
		else 
		    range_exceed = max( range_exceed, (unsigned long)pnr );
		}
	    }
	}
    list<string> ps = ppart.GetMatchingEntries( name + "p?[0-9]+" );
    if( !CheckPartedValid( ppart, ps, pl ) )
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
	    unsigned long major, minor;
	    pair<string,long> pr = GetDiskPartition( *i );
	    if( ppart.GetInfo( *i, s, major, minor ))
		{
		cyl = KbToCylinder(s);
		if( pr.second < (long)range )
		    {
		    unsigned id = Partition::ID_LINUX;
		    Partition::PType type = Partition::PRIMARY;
		    if( ext_possible )
			{
			if( s==1 )
			    {
			    type = Partition::EXTENDED;
			    id = Partition::ID_EXTENDED;
			    }
			if( pr.second>max_primary )
			    {
			    type = Partition::LOGICAL;
			    }
			}
		    Partition *p = 
			new Partition( *this, pr.second, s, cyl_start, cyl,
			               type, 
				       dec_string(CylinderToKb(cyl_start)),
				       id, false );
		    p->SetMajorMinor( major, minor );
		    pl.push_back( p );
		    }
		else 
		    range_exceed = max( range_exceed, (unsigned long)pr.second );
		cyl_start += cyl;
		}
	    }
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
	readonly = true;
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
    for( list<Partition*>::iterator i=pl.begin(); i!=pl.end(); i++ )
	{
	AddToList( *i );
	}
    return( true );
    }

bool Disk::CheckPartedValid( const ProcPart& pp, const list<string>& ps,
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
	if( (*i)->Type()==Partition::EXTENDED )
	    ext_nr = (*i)->Nr();
	else
	    {
	    parted_l[(*i)->Nr()] = (*i)->CylSize();
	    }
	}
    for( list<string>::const_iterator i=ps.begin(); i!=ps.end(); i++ )
	{
	pair<string,long> p = GetDiskPartition( *i );
	if( p.second>=0 && p.second!=ext_nr && 
	    pp.GetInfo( *i, SizeK, Dummy, Dummy ))
	    {
	    proc_l[unsigned(p.second)] = KbToCylinder( SizeK );
	    }
	}
    bool openbsd = false;
    if( proc_l.size()==parted_l.size() || 
        ((openbsd=HaveBsdPart()) && proc_l.size()>parted_l.size()) )
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

bool Disk::HaveBsdPart() const
    {
    bool ret=false;
    for( ConstPlainIterator i=begin(); !ret&&i!=end(); i++ )
	{
	const Partition *p = static_cast<const Partition *>(*i);
	if( p->Id()==0xa5 || p->Id()==0xa6 )
	    ret=true;
	}
    return(ret);
    }


string Disk::DefaultLabel()
    {
    string ret = "msdos";
    if( Storage::Arch()=="ia64" )
	ret = "gpt";
    else if( Storage::Arch()=="sparc" )
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

bool Disk::NeedP( const string& disk )
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

string Disk::GetPartName( const string& disk, unsigned nr )
    {
    return( disk + (Disk::NeedP(disk)?"p":"") + dec_string(nr) );
    }

string Disk::GetPartName( const string& disk, const string& nr )
    {
    return( disk + "/" + nr );
    }

pair<string,long> Disk::GetDiskPartition( const string& dev )
    {
    long nr = -1;
    string disk = dev;
    bool need_p = Disk::NeedP(dev);
    string::size_type p = dev.find_last_not_of( "0123456789" );
    if( p != string::npos && (!need_p||dev[p]=='p') && isdigit(dev[p+1]))
	{
	dev.substr(p+1) >> nr;
	disk = dev.substr( 0, p-(need_p?1:0));
	}
    return( make_pair<string,long>(disk,nr) );
    }
