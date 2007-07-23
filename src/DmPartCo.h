#ifndef DMPART_CO_H
#define DMPART_CO_H

#include <list>

#include "y2storage/PeContainer.h"
#include "y2storage/Disk.h"
#include "y2storage/DmPart.h"

namespace storage
{

class Storage;
class SystemCmd;
class ProcPart;
class Region;

class DmPartCo : public PeContainer
    {
    friend class Storage;

    public:
	DmPartCo( Storage * const s, const string& Name, storage::CType t,
	          ProcPart& ppart );
	DmPartCo( const DmPartCo& rhs );
	virtual ~DmPartCo();

	unsigned long long sizeK() const { return size_k; }
	unsigned isValid() const { return valid; }
	const string& labelName() const { return disk->labelName(); }
	const string& udevPath() const { return udev_path; }
	const std::list<string>& udevId() const { return udev_id; }
	unsigned numPartitions() const { return disk->numPartitions(); }
	static storage::CType const staticType() { return storage::DMRAID; }
	friend std::ostream& operator<< (std::ostream&, const DmPartCo& );
	void setUdevData( const string& id );

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
	int resizePartition( DmPart* p, unsigned long newCyl );
	int resizeVolume( Volume* v, unsigned long long newSize );
	int removeVolume( Volume* v );
	int removeDmPart();
	void getUnusedSpace( std::list<Region>& free, bool all=true, 
	                     bool logical=false );
	bool hasExtended() const;
	unsigned long long cylinderToKb( unsigned long val ) const 
	    { return disk->cylinderToKb( val ); }
	unsigned long kbToCylinder( unsigned long long val ) const
	    { return disk->kbToCylinder( val ); }
	string getPartName( unsigned nr ) const;

	virtual void getCommitActions( std::list<storage::commitAction*>& l ) const;
	virtual int getToCommit( storage::CommitStage stage,
				 std::list<Container*>& col,
			         std::list<Volume*>& vol );
	virtual int commitChanges( storage::CommitStage stage );
	int commitChanges( storage::CommitStage stage, Volume* vol );

	Partition* getPartition( unsigned nr, bool del );
	void getInfo( storage::DmPartCoInfo& info ) const;
	bool equalContent( const DmPartCo& rhs ) const;
	virtual string getDiffString( const Container& d ) const;
	void logDifference( const DmPartCo& d ) const;
	DmPartCo& operator= ( const DmPartCo& rhs );
	static string undevName( const string& name );
	string numToName( unsigned num ) const;

    protected:
	// iterators over partitions
        // protected typedefs for iterators over partitions
        typedef CastIterator<VIter, DmPart *> DmPartInter;
        typedef CastIterator<CVIter, const DmPart *> DmPartCInter;
        template< class Pred >
            struct DmPartPI { typedef ContainerIter<Pred, DmPartInter> type; };
        template< class Pred >
            struct DmPartCPI { typedef ContainerIter<Pred, DmPartCInter> type; };
        typedef CheckFnc<const DmPart> CheckFncDmPart;
        typedef CheckerIterator< CheckFncDmPart, DmPartPI<CheckFncDmPart>::type,
                                 DmPartInter, DmPart > DmPartPIterator;
        typedef CheckerIterator< CheckFncDmPart, DmPartCPI<CheckFncDmPart>::type,
                                 DmPartCInter, const DmPart > DmPartCPIterator;
	typedef DerefIterator<DmPartPIterator,DmPart> DmPartIter;
	typedef DerefIterator<DmPartCPIterator,const DmPart> ConstDmPartIter;
        typedef IterPair<DmPartIter> DmPartPair;
        typedef IterPair<ConstDmPartIter> ConstDmPartPair;

        DmPartPair dmpartPair( bool (* CheckDmPart)( const DmPart& )=NULL)
            {
            return( DmPartPair( dmpartBegin( CheckDmPart ), dmpartEnd( CheckDmPart ) ));
            }
        DmPartIter dmpartBegin( bool (* CheckDmPart)( const DmPart& )=NULL)
            {
	    IterPair<DmPartInter> p( (DmPartInter(begin())), (DmPartInter(end())) );
            return( DmPartIter( DmPartPIterator( p, CheckDmPart )) );
	    }
        DmPartIter dmpartEnd( bool (* CheckDmPart)( const DmPart& )=NULL)
            {
	    IterPair<DmPartInter> p( (DmPartInter(begin())), (DmPartInter(end())) );
            return( DmPartIter( DmPartPIterator( p, CheckDmPart, true )) );
	    }

        ConstDmPartPair dmpartPair( bool (* CheckDmPart)( const DmPart& )=NULL) const
            {
            return( ConstDmPartPair( dmpartBegin( CheckDmPart ), dmpartEnd( CheckDmPart ) ));
            }
        ConstDmPartIter dmpartBegin( bool (* CheckDmPart)( const DmPart& )=NULL) const
            {
	    IterPair<DmPartCInter> p( (DmPartCInter(begin())), (DmPartCInter(end())) );
            return( ConstDmPartIter( DmPartCPIterator( p, CheckDmPart )) );
	    }
        ConstDmPartIter dmpartEnd( bool (* CheckDmPart)( const DmPart& )=NULL) const
            {
	    IterPair<DmPartCInter> p( (DmPartCInter(begin())), (DmPartCInter(end())) );
            return( ConstDmPartIter( DmPartCPIterator( p, CheckDmPart, true )) );
	    }

	DmPartCo( Storage * const s, const string& File );
	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new DmPartCo( *this ) ); }
	void activate_part( bool val );
	void init( ProcPart& ppart );
	void createDisk( ProcPart& ppart );
	void getVolumes( ProcPart& ppart );
	void updatePointers( bool invalid=false );
	void updateMinor();
	virtual void newP( DmPart*& dm, unsigned num, Partition* p );
	int addNewDev( string& device );
	int updateDelDev();
	void handleWholeDevice();
	void removeFromMemory();
	void removePresentPartitions();
	bool validPartition( const Partition* p );
	bool findDm( unsigned nr, DmPartIter& i );

	static bool partNotDeleted( const DmPart&d ) { return( !d.deleted() ); }

	int doCreate( Volume* v );
	int doRemove( Volume* v );
	int doResize( Volume* v );
	int doSetType( DmPart* v );
	int doCreateLabel();
	virtual int doRemove();
	virtual string removeText( bool doing ) const;
	virtual string setDiskLabelText( bool doing ) const;

	void logData( const string& Dir );
	string udev_path;
	std::list<string> udev_id;
	string logfile_name;
	string sysfs_dir;

	Disk* disk;
	bool active;
	bool valid;
	bool del_ptable;
	unsigned num_part;

	mutable storage::DmPartCoInfo info;
    };

}

#endif
