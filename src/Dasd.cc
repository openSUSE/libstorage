/*
  Textdomain    "storage"
*/

#include <stdio.h>
#include <string>
#include <ostream>
#include <fstream>
#include <iomanip>
#include <boost/algorithm/string.hpp>

#include "y2storage/SystemCmd.h"
#include "y2storage/ProcPart.h"
#include "y2storage/Storage.h"
#include "y2storage/OutputProcessor.h"
#include "y2storage/Dasd.h"
#include "y2storage/StorageDefines.h"


namespace storage
{
    using namespace std;


Dasd::Dasd( Storage * const s, const string& Name,
            unsigned long long SizeK ) : 
    Disk(s,Name,SizeK)
    {
    fmt = DASDF_NONE;
    y2deb("constructed dasd " << dev);
    }

Dasd::~Dasd()
    {
    y2deb("destructed dasd " << dev);
    }

bool Dasd::detectPartitionsFdasd( ProcPart& ppart )
    {
    bool ret = true;
    string cmd_line = FDASDBIN " -p " + quote(device());
    system_stderr.erase();
    y2mil("executing cmd:" << cmd_line);
    SystemCmd Cmd( cmd_line );
    y2mil("retcode:" << Cmd.retcode());
    if( Cmd.retcode() == 0 )
	checkFdasdOutput( Cmd, ppart );
    y2mil("ret:" << ret << " partitons:" << vols.size());
    return( ret );
    }

bool Dasd::detectPartitions( ProcPart& ppart )
    {
    bool ret = true;
    string cmd_line = DASDVIEWBIN " -x " + quote(device());
    system_stderr.erase();
    detected_label = "dasd";
    setLabelData( "dasd" );
    y2mil("executing cmd:" << cmd_line);
    SystemCmd Cmd( cmd_line );
    y2mil("retcode:" << Cmd.retcode());
    if( Cmd.retcode() == 0 )
	{
	if( Cmd.select( "^format" )>0 )
	    {
	    string tmp = Cmd.getLine(0, true);
	    y2mil("Format line:" << tmp);
	    tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	    tmp = boost::to_lower_copy(extractNthWord(4, tmp), locale::classic());
	    if( tmp == "cdl" )
		fmt = DASDF_CDL;
	    else if( tmp == "ldl" )
		fmt = DASDF_LDL;
	    }
	getGeometry( Cmd, cyl, head, sector );
	new_cyl = cyl;
	new_head = head;
	new_sector = sector;
	y2mil("After dasdview head:" << head << " sector:" << sector << " cylinder:" << cyl << " sizeK:" << size_k);
	byte_cyl = head * sector * 512;
	y2mil("byte_cyl:" << byte_cyl);
	if( size_k==0 )
	    {
	    size_k = (head*sector*cyl)/2;
	    y2mil("New SizeK:" << size_k);
	    }
	y2mil("fmt:" << fmt);
	switch( fmt )
	    {
	    case DASDF_CDL:
		ret = Dasd::detectPartitionsFdasd(ppart);
		break;
	    case DASDF_LDL:
		{
		max_primary = 1;
		unsigned long long s = cylinderToKb(cyl);
		Partition *p = new Partition( *this, 1, s, 0, cyl,
					      PRIMARY, Partition::ID_LINUX,
					      false );
		if( ppart.getSize( p->device(), s ))
		    {
		    p->setSize( s );
		    }
		addToList( p );
		ret = true;
		}
		break;
	    default:
		break;
	    }
	}
    else
	{
	new_sector = sector = 96;
	y2mil("new sector:" << sector);
	}
    byte_cyl = head * sector * 512;
    y2mil("byte_cyl:" << byte_cyl);
    y2mil("ret:" << ret << " partitons:" << vols.size() << " detected label:" << label);
    ronly = fmt!=DASDF_CDL;
    y2mil("fmt:" << fmt << " readonly:" << ronly);
    return( ret );
    }

bool
Dasd::scanFdasdLine( const string& Line, unsigned& nr, unsigned long& start,
                     unsigned long& csize )
    {
    unsigned long StartM, EndM;
    string PartitionType, TInfo;

    y2deb("Line:" << Line);
    std::istringstream Data( Line );
    classic(Data);

    nr=0;
    StartM = EndM = 0;
    string devname;
    Data >> devname >> StartM >> EndM;
    devname.erase(0,device().size());
    devname >> nr;
    y2mil("Fields Num:" << nr << " Start:" << StartM << " End:" << EndM);
    if( nr>0 )
      {
      start = StartM/head;
      csize = EndM/head-start+1;
      if( start+csize > cylinders() )
	  {
	  csize = cylinders()-start;
	  y2mil("new csize:" << csize);
	  }
      y2mil("Fields Num:" << nr << " Start:" << start << " Size:" << csize);
      }
   return( nr>0 );
   }

bool
Dasd::checkFdasdOutput( SystemCmd& cmd, ProcPart& ppart )
    {
    int cnt;
    string line;
    string tmp;
    list<Partition *> pl;
    Regex part( "^"+device()+"[0123456789]+$" );

    cmd.select( device() );
    cnt = cmd.numLines();
    for( int i=0; i<cnt; i++)
	{
	unsigned pnr;
	unsigned long c_start;
	unsigned long c_size;

	line = cmd.getLine(i);
	tmp = extractNthWord( 0, line );
	if( part.match(tmp) )
	    {
	    if( scanFdasdLine( line, pnr, c_start, c_size ))
		{
		if( pnr<range )
		    {
		    unsigned long long s = cylinderToKb(c_size);
		    Partition *p = new Partition( *this, pnr, s,
						  c_start, c_size, PRIMARY,
						  Partition::ID_LINUX, false );
		    if( ppart.getSize( p->device(), s ))
			{
			p->setSize( s );
			}
		    pl.push_back( p );
		    }
		else
		    y2war("partition nr " << pnr << " outside range " << range);
		}
	    }
	}
    y2mil("nm:" << nm);
    string reg = nm;
    if( !reg.empty() && reg.find( '/' )!=string::npos &&
	isdigit(reg[reg.length()-1]) )
	reg += "p";
    reg = "^" + reg + "[0-9]+" "$";
    list<string> ps = ppart.getMatchingEntries(regex_matches(reg));
    y2mil("regex:\"" << reg << "\" ps:" << ps);
    unsigned long dummy = 0;
    if( !checkPartedValid( ppart, nm, pl, dummy ) )
	{
	string txt = sformat(
	// popup text %1$s is replaced by disk name e.g. /dev/hda
_("The partitioning on disk %1$s is not readable by\n"
"the partitioning tool fdasd, which is used to change the\n"
"partition table.\n"
"\n"
"You can use the partitions on disk %1$s as they are.\n"
"You can format them and assign mount points to them, but you\n"
"cannot add, edit, resize, or remove partitions from that\n"
"disk with this tool."), dev.c_str() );

	getStorage()->addInfoPopupText( dev, txt );
	ronly = true;
	}
    for( list<Partition*>::iterator i=pl.begin(); i!=pl.end(); ++i )
	{
	addToList( *i );
	}
    return( true );
    }

void Dasd::getGeometry( SystemCmd& cmd, unsigned long& c,
                        unsigned& h, unsigned& s )
    {
    string tmp;
    unsigned long val;
    if( cmd.select( "cylinders" )>0 )
	{
	val = 0;
	tmp = cmd.getLine(0, true);
	y2mil("Cylinder line:" << tmp);
	tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	tmp = extractNthWord( 3, tmp );
	tmp >> val;
	y2mil("val:" << val);
	c=val;
	}
    if( cmd.select( "tracks per" )>0 )
	{
	val = 0;
	tmp = cmd.getLine(0, true);
	y2mil("Tracks line:" << tmp);
	tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	tmp = extractNthWord( 3, tmp );
	tmp >> val;
	y2mil("val:" << val);
	h=val;
	}
    if( cmd.select( "blocks per" )>0 )
	{
	val = 0;
	tmp = cmd.getLine(0, true);
	y2mil("Blocks line:" << tmp);
	tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	tmp = extractNthWord( 3, tmp );
	tmp >> val;
	y2mil("val:" << val);
	s=val;
	}
    if( cmd.select( "blocksize" )>0 )
	{
	val = 0;
	tmp = cmd.getLine(0, true);
	y2mil("Bytes line:" << tmp);
	tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	tmp = extractNthWord( 3, tmp );
	tmp >> val;
	y2mil("val:" << val);
	s*=val/512;
	}
    y2mil("c:" << c << " h:" << h << " s:" << s);
    }

bool Dasd::detectGeometry()
    {
    Disk::detectGeometry();
    sector *= 8;
    byte_cyl *= 8;
    cyl /= 8;
    y2mil("cyl:" << cyl << " sector:" << sector << " byte_cyl:" << byte_cyl);
    return( true );
    }

int Dasd::doResize( Volume* v ) 
    { 
    return DASD_NOT_POSSIBLE;
    }

int Dasd::resizePartition( Partition* p, unsigned long newCyl )
    { 
    return DASD_NOT_POSSIBLE;
    }

int Dasd::removePartition( unsigned nr )
    {
    y2mil("begin nr:" << nr);
    int ret = Disk::removePartition( nr );
    if( ret==0 )
	{
	PartPair p = partPair( notDeleted );
	changeNumbers( p.begin(), p.end(), nr, -1 );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

int Dasd::createPartition( PartitionType type, unsigned long start,
                           unsigned long len, string& device,
			   bool checkRelaxed )
    {
    y2mil("begin type:" << type << " start:" << start << " len:" << len << " relaxed:" << checkRelaxed);
    int ret = createChecks( type, start, len, checkRelaxed );
    int number = 0;
    if( ret==0 )
	{
	number = availablePartNumber( type );
	if( number==0 )
	    {
	    ret = DISK_PARTITION_NO_FREE_NUMBER;
	    }
	else
	    {
	    PartPair p = partPair( notDeleted );
	    number = 1;
	    PartIter i = p.begin();
	    while( i!=p.end() && i->cylStart()<start )
		{
		number++;
		++i;
		}
	    y2mil("number:" << number);
	    changeNumbers( p.begin(), p.end(), number-1, 1 );
	    }
	}
    if( ret==0 )
	{
	Partition * p = new Partition( *this, number, cylinderToKb(len), start,
				       len, type );
	p->setCreated();
	device = p->device();
	addToList( p );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

string Dasd::fdasdText() const
    {
    string txt;
    // displayed text during action, %1$s is replaced by disk name (e.g. /dev/dasda),
    txt = sformat( _("Executing fdasd for disk %1$s..."), dev.c_str() );
    return( txt );
    }

int Dasd::doFdasd()
    {
    int ret = 0;
    if( !silent ) 
	{
	getStorage()->showInfoCb( fdasdText() );
	}
    string inpname = getStorage()->tmpDir()+"/fdasd_inp";
    ofstream inpfile( inpname.c_str() );
    classic(inpfile);
    PartPair p = partPair( notDeleted );
    PartIter i = p.begin();
    while( i!=p.end() )
	{
	string start = decString(i->cylStart()*new_head);
	string end  = decString((i->cylEnd()+1)*new_head-1);
	if( i->cylStart()==0 )
	    start = "first";
	if( i->cylEnd()>=cylinders()-1 )
	    end = "last";
	inpfile << "[" << start << "," << end << "]" << endl;
	++i;
	}
    inpfile.close();
    string cmd_line = FDASDBIN " -c " + inpname + " " + quote(device());
    if( execCheckFailed( cmd_line ) )
	{
	SystemCmd cmd( "cat " + inpname );
	ret = DASD_FDASD_FAILED;
	}
    if( ret==0 )
	{
	ProcPart ppart;
	p = partPair();
	i = p.begin();
	list<Partition*> rem_list;
	while( i!=p.end() )
	    {
	    if( i->deleted() )
		{
		rem_list.push_back( &(*i) );
		i->prepareRemove();
		}
	    if( i->created() )
		{
		unsigned long long s;
		Storage::waitForDevice(i->device());
		i->setCreated( false );
		if( ppart.getSize( i->device(), s ))
		    {
		    i->setSize( s );
		    }
		}
	    ++i;
	    }
	list<Partition*>::const_iterator pr = rem_list.begin();
	while( pr != rem_list.end() )
	    {
	    if( !removeFromList( *pr ) && ret==0 )
		ret = DISK_REMOVE_PARTITION_LIST_ERASE;
	    ++pr;
	    }
	}
    unlink( inpname.c_str() );
    y2mil("ret:" << ret);
    return( ret );
    }


void
Dasd::getCommitActions(list<commitAction>& l) const
    {
    y2mil("begin:" << name() << " init_disk:" << init_disk);
    Disk::getCommitActions( l );
    if( init_disk )
	{
	l.remove_if(stage_is(DECREASE));
	l.push_front(commitAction(DECREASE, staticType(), dasdfmtText(false), this, true));
	}
    }


string Dasd::dasdfmtTexts( bool single, const string& devs )
    {
    string txt;
    if( single )
	{
        // displayed text during action, %1$s is replaced by disk name (e.g. dasda),
        txt = sformat( _("Executing dasdfmt for disk %1$s..."), devs.c_str() );
	}
    else
	{
        // displayed text during action, %1$s is replaced by list of disk names (e.g. dasda dasdb dasdc),
        txt = sformat( _("Executing dasdfmt for disks: %1$s..."), devs.c_str() );
	}
    return( txt );
    }

string Dasd::dasdfmtText( bool doing ) const
    {
    string txt;
    if( doing )
        {
	txt = dasdfmtTexts( true, dev );
        }
    else
        {
        // displayed text before action, %1$s is replaced by disk name (e.g. /dev/dasda),
        txt = sformat( _("Execute dasdfmt on disk %1$s"), dev.c_str() );
        }
    return( txt );
    }


void
Dasd::getToCommit(CommitStage stage, list<const Container*>& col, list<const Volume*>& vol) const
{
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    if( stage==DECREASE ) 
	{
	ConstVolPair p = volPair( stageDecrease );
	if( !p.empty() )
	    vol.push_back( &(*(p.begin())) );
	if( deleted() || init_disk )
	    col.push_back( this );
	}
    else if( stage==INCREASE )
	{
	ConstVolPair p = volPair( stageCreate );
	if( !p.empty() )
	    vol.push_back( &(*(p.begin())) );
	}
    else
	Disk::getToCommit( stage, col, vol );
    if( col.size()!=oco || vol.size()!=ovo )
	y2mil("stage:" << stage << " col:" << col.size() << " vol:" << vol.size());
}


int Dasd::commitChanges( CommitStage stage )
    {
    y2mil("name:" << name() << " stage:" << stage);
    int ret = 0;
    if( stage==DECREASE && init_disk )
	{
	ret = doDasdfmt();
	}
    if( ret==0 )
	{
	ret = Disk::commitChanges( stage );
	}
    y2mil("ret:" << ret);
    return( ret );
    }

static bool needDasdfmt( const Disk&d )
    { 
    bool ret = d.isDasd() && d.initializeDisk();
    return( ret );
    }

int Dasd::doDasdfmt()
    {
    int ret = 0;
    y2mil("dasd:" << device());
    list<Disk*> dl;
    list<string> devs;
    getStorage()->getDiskList( needDasdfmt, dl );
    if( !dl.empty() )
	{
	for( list<Disk*>::const_iterator i = dl.begin(); i!=dl.end(); ++i )
	    {
	    devs.push_back( undevDevice((*i)->device()) );
	    }
	y2mil("devs:" << devs);
	if( !silent ) 
	    {
	    string txt = dasdfmtTexts( dl.size()==1, boost::join(devs, " ") );
	    getStorage()->showInfoCb( txt );
	    }
	for( list<string>::iterator i = devs.begin(); i!=devs.end(); ++i )
	    {
	    normalizeDevice(*i);
	    *i = "-f " + quote(*i);
	    }
	string cmd_line = DASDFMTBIN " -Y -P 4 -b 4096 -y -m 1 -d cdl " + boost::join(devs, " ");
	y2mil("cmdline:" << cmd_line);
	CallbackProgressBar cb = getStorage()->getCallbackProgressBarTheOne();
	ProgressBar* progressbar = new DasdfmtProgressBar( cb );
	SystemCmd cmd;
	cmd.setOutputProcessor(progressbar);
	if( execCheckFailed( cmd, cmd_line ) )
	    {
	    ret = DASD_DASDFMT_FAILED;
	    }
	if( ret==0 )
	    {
	    ProcPart ppart;
	    for( list<Disk*>::iterator i = dl.begin(); i!=dl.end(); ++i )
		{
		Dasd * ds = static_cast<Dasd *>(*i);
		ds->detectPartitions( ppart );
		ds->resetInitDisk();
		ds->removeFromMemory();
		}
	    }
	delete progressbar;
	}
    return( ret );
    }

int Dasd::initializeDisk( bool value )
    {
    y2mil("value:" << value << " old:" << init_disk);
    int ret = 0;
    if( init_disk != value )
	{
	init_disk = value;
	if( init_disk )
	    {
	    new_sector = sector = 96;
	    new_head = head = 15;
	    y2mil("new sector:" << sector << " head:" << head);
	    size_k = (head*sector*cyl)/2;
	    y2mil("new SizeK:" << size_k);
	    byte_cyl = head * sector * 512;
	    y2mil("new byte_cyl:" << byte_cyl);
	    ret = destroyPartitionTable( "dasd" );
	    }
	else
	    {
	    PartPair p = partPair();
	    PartIter i = p.begin();
	    list<Partition*> rem_list;
	    while( i!=p.end() )
		{
		if( i->deleted() )
		    {
		    i->setDeleted(false);
		    }
		if( i->created() )
		    {
		    rem_list.push_back( &(*i) );
		    }
		++i;
		}
	    list<Partition*>::const_iterator pr = rem_list.begin();
	    while( pr != rem_list.end() )
		{
		if( !removeFromList( *pr ) && ret==0 )
		    ret = DISK_REMOVE_PARTITION_LIST_ERASE;
		++pr;
		}
	    }
	}
    return( ret );
    }

Dasd& Dasd::operator= ( const Dasd& rhs )
    {
    y2deb("operator= from " << rhs.nm);
    *((Disk*)this) = rhs;
    fmt = rhs.fmt;
    return( *this );
    }

Dasd::Dasd( const Dasd& rhs ) : Disk(rhs)
    {
    fmt = DASDF_NONE;
    y2deb("constructed dasd by copy constructor from " << rhs.nm);
    }


std::ostream& operator<< (std::ostream& s, const Dasd& d )
    {
    s << *((Disk*)&d);
    s << " fmt:" << d.fmt;
    return( s );
    }

}
