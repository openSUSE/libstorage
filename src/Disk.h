#ifndef DISK_H
#define DISK_H

#include <list>

using namespace std;

#include "y2storage/Container.h"
#include "y2storage/Partition.h"

using namespace storage;

class Storage;
class SystemCmd;
class ProcPart;
class istream;

class Disk : public Container
    {
    friend class Storage;

    struct label_info
	{
	string name;
	bool extended;
	unsigned primary;
	unsigned logical;
	};

    public:
	Disk( Storage * const s, const string& Name, unsigned long long Size );
	virtual ~Disk();

	unsigned long cylinders() const { return cyl; }
	unsigned heads() const { return head; }
	unsigned sectors() const { return sector; }
	unsigned long long sizeK() const { return size_k; }
	unsigned long minorNr() const { return mnr; }
	unsigned long majorNr() const { return mjr; }
	unsigned long numMinor() const { return range; }
	unsigned numPartitions() const;
	static CType const staticType() { return DISK; }
	friend inline ostream& operator<< (ostream&, const Disk& );

	static bool needP( const string& dev );
	int createPartition( PartitionType type, long unsigned start,
	                     long unsigned len, string& device,
			     bool checkRelaxed=false );
	int removePartition( unsigned nr );
	int changePartitionId( unsigned nr, unsigned id );
	int destroyPartitionTable( const string& new_label );
	unsigned availablePartNumber( PartitionType type=PRIMARY );
	int commitChanges( CommitStage stage );
	int checkResize( Volume* v, unsigned long long& newSize, 
	                 bool doit, bool &done );

	bool hasExtended() const;
	string setDiskLabelText( bool doing=true ) const;
	unsigned long long cylinderToKb( unsigned long ) const;
	unsigned long kbToCylinder( unsigned long long ) const;
	string getPartName( unsigned nr ) const;
	static string getPartName( const string& disk, unsigned nr );
	static string getPartName( const string& disk, const string& nr );
	static pair<string,long> getDiskPartition( const string& dev );

    protected:

	// iterators over partitions
        // protected typedefs for iterators over partitions
        typedef CastIterator<VIter, Partition *> PartInter;
        typedef CastIterator<CVIter, const Partition *> PartCInter;
        template< class Pred >
            struct PartitionPI { typedef ContainerIter<Pred, PartInter> type; };
        template< class Pred >
            struct PartitionCPI { typedef ContainerIter<Pred, PartCInter> type; };
        typedef CheckFnc<const Partition> CheckFncPartition;
        typedef CheckerIterator< CheckFncPartition, PartitionPI<CheckFncPartition>::type,
                                 PartInter, Partition > PartPIterator;
        typedef CheckerIterator< CheckFncPartition, PartitionCPI<CheckFncPartition>::type,
                                 PartCInter, const Partition > PartCPIterator;
	typedef DerefIterator<PartPIterator,Partition> PartIter;
	typedef DerefIterator<PartCPIterator,const Partition> ConstPartIter;
        typedef IterPair<PartIter> PartPair;
        typedef IterPair<ConstPartIter> ConstPartPair;

        PartPair partPair( bool (* CheckPart)( const Partition& )=NULL)
            {
            return( PartPair( partBegin( CheckPart ), partEnd( CheckPart ) ));
            }
        PartIter partBegin( bool (* CheckPart)( const Partition& )=NULL)
            {
	    IterPair<PartInter> p( (PartInter(begin())), (PartInter(end())) );
            return( PartIter( PartPIterator( p, CheckPart )) );
	    }
        PartIter partEnd( bool (* CheckPart)( const Partition& )=NULL)
            {
	    IterPair<PartInter> p( (PartInter(begin())), (PartInter(end())) );
            return( PartIter( PartPIterator( p, CheckPart, true )) );
	    }

        ConstPartPair partPair( bool (* CheckPart)( const Partition& )=NULL) const
            {
            return( ConstPartPair( partBegin( CheckPart ), partEnd( CheckPart ) ));
            }
        ConstPartIter partBegin( bool (* CheckPart)( const Partition& )=NULL) const
            {
	    IterPair<PartCInter> p( (PartCInter(begin())), (PartCInter(end())) );
            return( ConstPartIter( PartCPIterator( p, CheckPart )) );
	    }
        ConstPartIter partEnd( bool (* CheckPart)( const Partition& )=NULL) const
            {
	    IterPair<PartCInter> p( (PartCInter(begin())), (PartCInter(end())) );
            return( ConstPartIter( PartCPIterator( p, CheckPart, true )) );
	    }

	Disk( Storage * const s, const string& File );
	unsigned long long capacityInKb() const { return size_k; }
	bool detectGeometry();
	bool detectPartitions();
	bool getSysfsInfo( const string& SysFsDir );
	int checkSystemError( const string& cmd_line, const SystemCmd& cmd );
	int execCheckFailed( const string& cmd_line );
	bool checkPartedOutput( const SystemCmd& cmd );
	bool scanPartedLine( const string& Line, unsigned& nr,
	                     unsigned long& start, unsigned long& csize,
			     PartitionType& type, string& parted_start,
			     unsigned& id, bool& boot );
	bool checkPartedValid( const ProcPart& pp, const list<string>& ps,
	                       const list<Partition*>& pl );
	bool getPartedValues( Partition *p );
	virtual void print( ostream& s ) const { s << *this; }

	static bool notDeleted( const Partition&d ) { return( !d.deleted() ); }
	void getCommitActions( list<commitAction*>& l ) const;

	int doCreate( Volume* v );
	int doRemove( Volume* v );
	int doResize( Volume* v );
	int doSetType( Volume* v );
	int doCreateLabel();

	//list<Region> getUnusedRegions();
	void logData( const string& Dir );
	bool haveBsdPart() const;
	void setLabelData( const string& );

	static string defaultLabel();
	static label_info labels[];
	static string p_disks[];

	unsigned long cyl;
	unsigned head;
	unsigned sector;
	string label;
	string detected_label;
	string system_stderr;
	unsigned max_primary;
	bool ext_possible;
	unsigned max_logical;
	unsigned long byte_cyl;
	unsigned long long size_k;
	unsigned long mnr;
	unsigned long mjr;
	unsigned long range;
    };

inline ostream& operator<< (ostream& s, const Disk& d )
    {
    s << *((Container*)&d);
    s << " Cyl:" << d.cyl
      << " Head:" << d.head
      << " Sect:" << d.sector
      << " Node <" << d.mjr
      << ":" << d.mnr << ">"
      << " Range:" << d.range
      << " SizeM:" << d.size_k/1024
      << " Label:" << d.label
      << " MaxPrimary:" << d.max_primary;
    if( d.ext_possible )
	s << " ExtPossible MaxLogical:" << d.max_logical;
    return( s );
    }


#endif
