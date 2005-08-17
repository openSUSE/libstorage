#ifndef DISK_H
#define DISK_H

#include <list>

#include "y2storage/Container.h"
#include "y2storage/Partition.h"

class Storage;
class SystemCmd;
class ProcPart;
class Region;

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
	Disk( const Disk& rhs );
	virtual ~Disk();

	unsigned long cylinders() const { return cyl; }
	unsigned heads() const { return head; }
	unsigned sectors() const { return sector; }
	unsigned long long sizeK() const { return size_k; }
	unsigned long minorNr() const { return mnr; }
	unsigned long majorNr() const { return mjr; }
	unsigned long numMinor() const { return range; }
	unsigned long cylSizeB() const { return byte_cyl; }
	unsigned maxPrimary() const { return max_primary; }
	unsigned maxLogical() const { return max_logical; }
	const string& labelName() const { return label; }
	unsigned numPartitions() const;
	static storage::CType const staticType() { return storage::DISK; }
	friend std::ostream& operator<< (std::ostream&, const Disk& );

	static bool needP( const string& dev );
	int createPartition( storage::PartitionType type, long unsigned start,
	                     long unsigned len, string& device,
			     bool checkRelaxed=false );
	int createPartition( long unsigned len, string& device,
			     bool checkRelaxed=false );
	int createPartition( storage::PartitionType type, string& device );
	int removePartition( unsigned nr );
	int changePartitionId( unsigned nr, unsigned id );
	int forgetChangePartitionId( unsigned nr );
	int changePartitionArea( unsigned nr, unsigned long start, 
	                         unsigned long size, bool checkRelaxed=false );
	int nextFreePartition( storage::PartitionType type, unsigned& nr,
	                       string& device );
	int destroyPartitionTable( const string& new_label );
	unsigned availablePartNumber( storage::PartitionType type=storage::PRIMARY );
	void getCommitActions( std::list<storage::commitAction*>& l ) const;
	int getToCommit( storage::CommitStage stage, std::list<Container*>& col,
			 std::list<Volume*>& vol );
	int commitChanges( storage::CommitStage stage );
	int commitChanges( storage::CommitStage stage, Volume* vol );
	int resizePartition( Partition* p, unsigned long newCyl );
	int resizeVolume( Volume* v, unsigned long long newSize );
	int removeVolume( Volume* v );
	void getUnusedSpace( std::list<Region>& free, bool all=true, 
	                     bool logical=false );
	bool hasExtended() const;
	string setDiskLabelText( bool doing=true ) const;
	unsigned long long cylinderToKb( unsigned long ) const;
	unsigned long kbToCylinder( unsigned long long ) const;
	string getPartName( unsigned nr ) const;
	void getInfo( storage::DiskInfo& info ) const;
	bool equalContent( const Disk& rhs ) const;
	void logDifference( const Disk& d ) const;


	static string getPartName( const string& disk, unsigned nr );
	static string getPartName( const string& disk, const string& nr );
	static std::pair<string,long> getDiskPartition( const string& dev );

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
			     storage::PartitionType& type, 
			     unsigned& id, bool& boot );
	bool checkPartedValid( const ProcPart& pp, const std::list<string>& ps,
	                       const std::list<Partition*>& pl );
	bool getPartedValues( Partition *p );
	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new Disk( *this ) ); }

	static bool notDeleted( const Partition&d ) { return( !d.deleted() ); }

	int doCreate( Volume* v );
	int doRemove( Volume* v );
	int doResize( Volume* v );
	int doSetType( Volume* v );
	int doCreateLabel();

	//std::list<Region> getUnusedRegions();
	void logData( const string& Dir );
	bool haveBsdPart( const std::list<Partition*>& pl) const;
	void setLabelData( const string& );
	Disk& operator= ( const Disk& rhs );

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
	mutable storage::DiskInfo info;
    };

#endif
