/*
  Textdomain    "storage"
*/

#include <iostream>
#include <stdio.h>

#include <string>
#include <sstream>
#include <iomanip>

#include "y2storage/SystemCmd.h"
#include "y2storage/ProcPart.h"
#include "y2storage/Storage.h"
#include "y2storage/OutputProcessor.h"
#include "y2storage/Dasd.h"

using namespace std;
using namespace storage;

Dasd::Dasd( Storage * const s, const string& Name,
            unsigned long long SizeK ) : 
    Disk(s,Name,SizeK)
    {
    y2debug( "constructed dasd %s", dev.c_str() );
    }

Dasd::~Dasd()
    {
    y2debug( "destructed dasd %s", dev.c_str() );
    }

bool Dasd::detectPartitionsFdasd( ProcPart& ppart )
    {
    bool ret = true;
    string cmd_line = "/sbin/fdasd -p " + device();
    system_stderr.erase();
    y2milestone( "executing cmd:%s", cmd_line.c_str() );
    SystemCmd Cmd( cmd_line );
    y2milestone( "retcode:%d", Cmd.retcode() );
    if( Cmd.retcode() == 0 )
	checkFdasdOutput( Cmd, ppart );
    y2milestone( "ret:%d partitons:%zd", ret, vols.size() );
    return( ret );
    }

bool Dasd::detectPartitions( ProcPart& ppart )
    {
    bool ret = true;
    string cmd_line = "dasdview -x " + device();
    system_stderr.erase();
    detected_label = "dasd";
    setLabelData( "dasd" );
    y2milestone( "executing cmd:%s", cmd_line.c_str() );
    SystemCmd Cmd( cmd_line );
    y2milestone( "retcode:%d", Cmd.retcode() );
    if( Cmd.retcode() == 0 )
	{
	if( Cmd.select( "^format" )>0 )
	    {
	    string tmp = *Cmd.getLine(0, true);
	    y2milestone( "Format line:%s", tmp.c_str() );
	    tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	    tmp = extractNthWord( 4, tmp );
	    tolower( tmp );
	    if( tmp == "cdl" )
		fmt = DASDF_CDL;
	    else if( tmp == "ldl" )
		fmt = DASDF_LDL;
	    }
	getGeometry( Cmd, cyl, head, sector );
	new_cyl = cyl;
	new_head = head;
	new_sector = sector;

	y2milestone( "After dasdview Head:%u Sector:%u Cylinder:%lu SizeK:%llu",
		     head, sector, cyl, size_k );
	if( size_k==0 )
	    {
	    size_k = (head*sector*cyl)/2;
	    y2milestone( "New SizeK:%llu", size_k );
	    }
	y2mil( "fmt:" << fmt );
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
	y2milestone( "new sector:%u", sector );
	}
    byte_cyl = head * sector * 512;
    y2milestone( "byte_cyl:%lu", byte_cyl );
    y2milestone( "ret:%d partitons:%zd detected label:%s", ret, vols.size(), 
                 label.c_str() );
    ronly = fmt!=DASDF_CDL;
    y2milestone( "fmt:%d readonly:%d", fmt, ronly );
    return( ret );
    }

bool
Dasd::scanFdasdLine( const string& Line, unsigned& nr, unsigned long& start,
                     unsigned long& csize )
    {
    unsigned long StartM, EndM;
    string PartitionType, TInfo;

    y2debug( "Line: %s", Line.c_str() );
    std::istringstream Data( Line );

    nr=0;
    StartM = EndM = 0;
    string devname;
    Data >> devname >> StartM >> EndM;
    devname.erase(0,device().size());
    devname >> nr;
    y2milestone( "Fields Num:%d Start:%lu End:%lu", nr, StartM, EndM );
    if( nr>0 )
      {
      start = StartM/head;
      csize = EndM/head-start+1;
      if( start+csize > cylinders() )
	  {
	  csize = cylinders()-start;
	  y2milestone( "new csize:%lu", csize );
	  }
      y2milestone( "Fields Num:%d Start:%ld Size:%ld", nr, start, csize );
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
    Regex part( "^"+device()+"[0-9]+$" );

    cmd.select( device() );
    cnt = cmd.numLines();
    for( int i=0; i<cnt; i++)
	{
	unsigned pnr;
	unsigned long c_start;
	unsigned long c_size;

	line = *cmd.getLine(i);
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
		    y2warning( "partition nr %d outside range %lu", pnr, range );
		}
	    }
	}
    y2mil( "nm:" << nm );
    string reg = nm;
    if( !reg.empty() && reg.find( '/' )!=string::npos &&
	isdigit(reg[reg.length()-1]) )
	reg += "p";
    reg += "[0-9]+";
    list<string> ps = ppart.getMatchingEntries( reg );
    y2mil( "regex " << reg << " ps " << ps );
    unsigned long dummy = 0;
    if( !checkPartedValid( ppart, nm, pl, dummy ) )
	{
	// popup text %1$s is replaced by disk name e.g. /dev/hda
	string txt = sformat(
_("The partitioning on disk %1$s is not readable by\n"
"the partitioning tool fdasd, which is used to change the\n"
"partition table.\n"
"\n"
"You can use the partitions on disk %1$s as they are.\n"
"You can format them and assign mount points to them, but you\n"
"cannot add, edit, resize, or remove partitions from that\n"
"disk with this tool."), dev.c_str() );

	getStorage()->infoPopupCb( txt );
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
	tmp = *cmd.getLine(0, true);
	y2milestone( "Cylinder line:%s", tmp.c_str() );
	tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	tmp = extractNthWord( 3, tmp );
	tmp >> val;
	if( val>0 )
	    {
	    y2mil( "val:" << val );
	    c=val;
	    }
	}
    if( cmd.select( "tracks per" )>0 )
	{
	val = 0;
	tmp = *cmd.getLine(0, true);
	y2milestone( "Tracks line:%s", tmp.c_str() );
	tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	tmp = extractNthWord( 3, tmp );
	tmp >> val;
	if( val>0 )
	    {
	    y2mil( "val:" << val );
	    h=val;
	    }
	}
    if( cmd.select( "blocks per" )>0 )
	{
	val = 0;
	tmp = *cmd.getLine(0, true);
	y2milestone( "Blocks line:%s", tmp.c_str() );
	tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	tmp = extractNthWord( 3, tmp );
	tmp >> val;
	if( val>0 )
	    {
	    y2mil( "val:" << val );
	    s=val;
	    }
	}
    if( cmd.select( "blocksize" )>0 )
	{
	val = 0;
	tmp = *cmd.getLine(0, true);
	y2milestone( "Bytes line:%s", tmp.c_str() );
	tmp = tmp.erase( 0, tmp.find( ':' ) + 1 );
	tmp = extractNthWord( 3, tmp );
	tmp >> val;
	if( val>0 )
	    {
	    y2mil( "val:" << val );
	    s*=val/512;
	    }
	}
    y2milestone( "c:%lu h:%u s:%u", c, h, s );
    }

bool Dasd::detectGeometry()
    {
    Disk::detectGeometry();
    sector *= 8;
    byte_cyl *= 8;
    cyl /= 8;
    y2milestone( "cyl:%lu sector:%u byte_cyl:%lu", cyl, sector, byte_cyl  );
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
    y2milestone( "begin nr %u", nr );
    int ret = Disk::removePartition( nr );
    if( ret==0 )
	{
	PartPair p = partPair( notDeleted );
	changeNumbers( p.begin(), p.end(), nr, -1 );
	}
    y2milestone( "ret %d", ret );
    return( ret );
    }

int Dasd::createPartition( PartitionType type, unsigned long start,
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
	    y2milestone( "number %u", number );
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
    y2milestone( "ret %d", ret );
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
    string cmd_line =  "/sbin/fdasd -c " + inpname + " " + device();
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
		}
	    if( i->created() )
		{
		unsigned long long s;
		getStorage()->waitForDevice( i->device() );
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void Dasd::getCommitActions( list<commitAction*>& l ) const
    {
    y2mil( "begin:" << name() << " init_disk:" << init_disk );
    Disk::getCommitActions( l );
    if( init_disk )
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
				        dasdfmtText(false), true, true ));
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

int Dasd::getToCommit( CommitStage stage, list<Container*>& col,
                       list<Volume*>& vol )
    {
    int ret = 0;
    unsigned long oco = col.size();
    unsigned long ovo = vol.size();
    if( stage==DECREASE ) 
	{
	VolPair p = volPair( stageDecrease );
	if( !p.empty() )
	    vol.push_back( &(*(p.begin())) );
	if( deleted() || init_disk )
	    col.push_back( this );
	}
    else if( stage==INCREASE )
	{
	VolPair p = volPair( stageCreate );
	if( !p.empty() )
	    vol.push_back( &(*(p.begin())) );
	}
    else
	ret = Disk::getToCommit( stage, col, vol );
    if( col.size()!=oco || vol.size()!=ovo )
	y2milestone( "ret:%d col:%zd vol:%zd", ret, col.size(), vol.size());
    return( ret );
    }

int Dasd::commitChanges( CommitStage stage )
    {
    y2milestone( "name %s stage %d", name().c_str(), stage );
    int ret = 0;
    if( stage==DECREASE && init_disk )
	{
	ret = doDasdfmt();
	}
    if( ret==0 )
	{
	ret = Disk::commitChanges( stage );
	}
    y2milestone( "ret:%d", ret );
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
    y2milestone( "dasd:%s", device().c_str() );
    list<Disk*> dl;
    list<string> devs;
    getStorage()->getDiskList( needDasdfmt, dl );
    if( !dl.empty() )
	{
	for( list<Disk*>::const_iterator i = dl.begin(); i!=dl.end(); ++i )
	    {
	    devs.push_back( undevDevice((*i)->device()) );
	    }
	y2mil( "devs:" << devs );
	if( !silent ) 
	    {
	    string txt = dasdfmtTexts( dl.size()==1, mergeString(devs) );
	    getStorage()->showInfoCb( txt );
	    }
	for( list<string>::iterator i = devs.begin(); i!=devs.end(); ++i )
	    {
	    normalizeDevice(*i);
	    *i = "-f " + *i;
	    }
	string cmd_line = "dasdfmt -Y -P 4 -b 4096 -y -m 1 -d cdl " +
	                  mergeString(devs);
	y2milestone( "cmdline:%s", cmd_line.c_str() );
	CallbackProgressBar cb = getStorage()->getCallbackProgressBarTheOne();
	ScrollBarHandler* sb = new DasdfmtScrollbar( cb );
	SystemCmd cmd;
	cmd.setOutputProcessor( sb );
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
	}
    return( ret );
    }

int Dasd::initializeDisk( bool value )
    {
    y2milestone( "value:%d old:%d", value, init_disk );
    int ret = 0;
    if( init_disk != value )
	{
	init_disk = value;
	if( init_disk )
	    {
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
    y2debug( "operator= from %s", rhs.nm.c_str() );
    *((Disk*)this) = rhs;
    fmt = rhs.fmt;
    return( *this );
    }

Dasd::Dasd( const Dasd& rhs ) : Disk(rhs)
    {
    fmt = DASDF_NONE;
    y2debug( "constructed dasd by copy constructor from %s", rhs.nm.c_str() );
    }

namespace storage
{
std::ostream& operator<< (std::ostream& s, const Dasd& d )
    {
    s << *((Disk*)&d);
    s << " fmt:" << d.fmt;
    return( s );
    }
}



