#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/Partition.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Disk.h"

Partition::Partition( const Disk& d, unsigned PNr, unsigned long long SizeK,
                      unsigned long Start, unsigned long CSize,
		      PartitionType Type, const string& PartedStart,
		      unsigned Id, bool Boot )
    : Volume( d, PNr, SizeK ), reg( Start, CSize )
    {
    bootflag = Boot;
    idt = Id;
    typ = Type;
    parted_start = PartedStart;
    orig_num = num;
    y2milestone( "constructed partition %s on disk %s", dev.c_str(),
                 cont->name().c_str() );
    }

Partition::Partition( const Disk& d, const string& Data ) :
        Volume( d, 0, 0 )
    {
    string ts, rs;
    istringstream i( Data );
    i >> num >> dev >> size_k >> mjr >> mnr >> reg >>
	 hex >> idt >> dec >> ts >> rs;
    nm = dev.substr (5);	// strip "/dev/"
    if( ts == "extended" )
	typ = EXTENDED;
    else if( ts == "logical" )
	typ = LOGICAL;
    else
	typ = PRIMARY;
    if( rs == "boot" )
	bootflag = true;
    else
	bootflag = false;
    orig_num = num;
    y2milestone( "constructed partition %s on disk %s", dev.c_str(),
                 cont->name().c_str() );
    }

bool Partition::intersectArea( const Region& r ) const
    {
    return( r.intersect( reg ) );
    }

void Partition::changeNumber( unsigned new_num )
    {
    if( new_num!=num )
	{
	if( orig_num==num )
	    {
	    orig_num = num;
	    }
	num = new_num;
	if( created() )
	    {
	    orig_num = num;
	    }
	setNameDev();
	getMajorMinor( dev, mjr, mnr );
	}
    }

void Partition::changeId( unsigned new_id )
    {
    if( new_id!=idt )
	{
	if( orig_id==idt )
	    {
	    orig_id = idt;
	    }
	idt = new_id;
	if( created() )
	    {
	    orig_id = idt;
	    }
	}
    }

void Partition::changeIdDone() 
    {
    orig_id = idt;
    }

ostream& Partition::logData( ostream& file ) const
    {
    file << num << " " << dev << " " << size_k << " " <<  mjr << " "
         << mnr << " ";
    file << reg.start() << " " << reg.len() << " " << hex << idt << dec;
    if( typ == LOGICAL )
	file << " logical";
    else if( typ == EXTENDED )
	file << " extended";
    else
	file << " primary";
    if( bootflag )
	file << " boot";
    if( orig_num!=num )
	file << " OrigNr:" << orig_num;
    return( file );
    }

Partition::~Partition()
    {
    y2milestone( "destructed partition %s", dev.c_str() );
    }

PartitionInfo
Partition::getPartitionInfo () const
{
    PartitionInfo tmp;
    tmp.name = name ();
    tmp.partitiontype = type ();
    return tmp;
}
