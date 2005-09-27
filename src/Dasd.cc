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
#include "y2storage/Dasd.h"

using namespace std;
using namespace storage;

Dasd::Dasd( Storage * const s, const string& Name,
            unsigned long long SizeK ) : 
    Disk(s,Name,SizeK)
    {
    y2milestone( "constructed dasd %s", dev.c_str() );
    }

Dasd::~Dasd()
    {
    y2milestone( "destructed dasd %s", dev.c_str() );
    }

bool Dasd::detectPartitions()
    {
    bool ret = true;
    string cmd_line = "/sbin/fdasd -p " + device();
    setLabelData( "dasd" );
    system_stderr.erase();
    y2milestone( "executing cmd:%s", cmd_line.c_str() );
    SystemCmd Cmd( cmd_line );
    checkSystemError( cmd_line, Cmd );
    getGeometry( Cmd, cyl, head, sector );
    new_cyl = cyl;
    new_head = head;
    new_sector = sector;
    y2milestone( "After fdasd Head:%u Sector:%u Cylinder:%lu",
		 head, sector, cyl );
    byte_cyl = head * sector * 512;
    y2milestone( "byte_cyl:%lu", byte_cyl );
    checkFdasdOutput( Cmd );
    detected_label = "dasd";
    setLabelData( "dasd" );
    y2milestone( "ret:%d partitons:%zd detected label:%s", ret, vols.size(), 
                 label.c_str() );
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
Dasd::checkFdasdOutput( SystemCmd& cmd )
    {
    int cnt;
    string line;
    string tmp;
    ProcPart ppart;
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
    list<string> ps = ppart.getMatchingEntries( nm + "p?[0-9]+" );
    if( !checkPartedValid( ppart, ps, pl ) )
	{
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
		    Partition *p =
			new Partition( *this, pr.second, s, cyl_start, cyl,
			               type, id, false );
		    pl.push_back( p );
		    }
		else
		    y2warning( "partition nr %ld outside range %lu", 
		               pr.second, range );
		cyl_start += cyl;
		}
	    }
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
	y2milestone( "Cylinder line:%s", (*cmd.getLine(0, true)).c_str() );
	tmp = extractNthWord( 0, *cmd.getLine(0, true));
	tmp >> val;
	if( val>0 )
	    c=val;
	}
    if( cmd.select( "tracks per" )>0 )
	{
	val = 0;
	y2milestone( "Tracks line:%s", (*cmd.getLine(0, true)).c_str() );
	tmp = extractNthWord( 0, *cmd.getLine(0, true));
	tmp >> val;
	if( val>0 )
	    h=val;
	}
    if( cmd.select( "blocks per" )>0 )
	{
	val = 0;
	y2milestone( "Blocks line:%s", (*cmd.getLine(0, true)).c_str() );
	tmp = extractNthWord( 0, *cmd.getLine(0, true));
	tmp >> val;
	if( val>0 )
	    s=val;
	}
    if( cmd.select( "bytes  per" )>0 )
	{
	val = 0;
	y2milestone( "Bytes line:%s", (*cmd.getLine(0, true)).c_str() );
	tmp = extractNthWord( 0, *cmd.getLine(0, true));
	tmp >> val;
	if( val>0 )
	    s*=val/512;
	}
    y2milestone( "c:%lu h:%u s:%u", c, h, s );
    }

std::ostream& operator<< (std::ostream& s, const Dasd &p )
    { 
    s << *(Disk*)&p;
    return( s );
    }

bool Dasd::equalContent( const Dasd& rhs ) const
    {
    return( Disk::equalContent(rhs) );
    }

void Dasd::logDifference( const Dasd& rhs ) const
    {
    Disk::logDifference( rhs );
    }

Dasd& Dasd::operator= ( const Dasd& rhs )
    {
    y2milestone( "operator= from %s", rhs.nm.c_str() );
    *((Disk*)this) = rhs;
    return( *this );
    }

Dasd::Dasd( const Dasd& rhs ) : Disk(rhs)
    {
    y2milestone( "constructed dasd by copy constructor from %s",
                 rhs.nm.c_str() );
    }

