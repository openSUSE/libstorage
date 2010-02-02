/*
 * File: MdPartCo.cc
 *
 * Declaration of MdPartCo class which represents single MD Device (RAID
 * Volume) like md126 which is a Container for partitions.
 *
 * Copyright (c) 2009, Intel Corporation.
 * Copyright (c) 2009 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MD_PART_CO_H
#define MD_PART_CO_H

#include <list>

#include "storage/Container.h"
#include "storage/Disk.h"
#include "storage/Md.h"
#include "storage/MdPart.h"

namespace storage
{
    class Storage;
    class SystemInfo;
    class Region;


/**
 * Class: MdPartCo
 *
 * Brief:
 *  MD RAID Volume that can be partitioned and used as container for partitions.
*/
class MdPartCo : public Container
    {
    friend class Storage;

    public:
    MdPartCo(Storage * const s, const string& Name, SystemInfo& systeminfo);
    MdPartCo(const MdPartCo& c);
    virtual ~MdPartCo();

    unsigned long long sizeK() const { return size_k; }
    const string& labelName() const { return disk->labelName(); }
    list<string> udevId() const { return udev_id; }
    unsigned numPartitions() const { return disk->numPartitions(); }
    static storage::CType staticType() { return storage::MDPART; }
    friend std::ostream& operator<< (std::ostream&, const MdPartCo& );
    void setUdevData(const list<string>& id);

    void getMdPartCoState(MdPartCoStateInfo& info) const;

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
    int nextFreePartition(storage::PartitionType type, unsigned& nr,
        string& device) const;
    int destroyPartitionTable( const string& new_label );
	int freeCylindersAroundPartition(const MdPart* p, unsigned long& freeCylsBefore,
					 unsigned long& freeCylsAfter) const;
    int resizePartition( MdPart* p, unsigned long newCyl );
    int resizeVolume( Volume* v, unsigned long long newSize );
    int removeVolume( Volume* v );
    int removeMdPart();

    unsigned maxPrimary() const { return disk->maxPrimary(); }
    bool extendedPossible() const { return disk->extendedPossible(); }
    unsigned maxLogical() const { return disk->maxLogical(); }

    unsigned int numPrimary() const { return disk->numPrimary(); }
    bool hasExtended() const { return disk->hasExtended(); }
    unsigned int numLogical() const { return disk->numLogical(); }

    void getUnusedSpace(std::list<Region>& free, bool all = true, bool logical = false) const
    { disk->getUnusedSpace(free, all, logical); }

    unsigned long long cylinderToKb( unsigned long val ) const
    { return disk->cylinderToKb( val ); }
    unsigned long kbToCylinder( unsigned long long val ) const
    { return disk->kbToCylinder( val ); }
    string getPartName( unsigned nr ) const;

    virtual void getCommitActions(list<commitAction>& l) const;
    virtual int getToCommit(CommitStage stage, list<const Container*>& col, list<const Volume*>& vol);
    virtual int commitChanges( storage::CommitStage stage );
    int commitChanges( storage::CommitStage stage, Volume* vol );

    Partition* getPartition( unsigned nr, bool del );
    int getPartitionInfo(deque<storage::PartitionInfo>& plist);
    void getInfo( storage::MdPartCoInfo& info ) const;
    bool equalContent( const Container& rhs ) const;
    virtual string getDiffString( const Container& d ) const;
    void logDifference( const MdPartCo& d ) const;
    MdPartCo& operator= ( const MdPartCo& rhs );
    static string undevName( const string& name );
    string numToName( unsigned mdNum ) const;

    static list<string> getMdRaids();

    void syncRaidtab();

    /* Returns Md number. */
    int nr() const;
    /* RAID Related functionality */
    unsigned long chunkSize() const { return chunk_size; }

    storage::MdType personality() const { return md_type; }

    storage::MdArrayState getArrayState() { return md_state; };

    void getMdUuid( string&val ) { val=md_uuid; }

    /* Raid Level of the RAID as string. */
	const string& pName() const { return Md::md_names[md_type]; }
    /* Parity for some of RAID's. */
	const string& ptName() const { return Md::par_names[md_parity]; }

    /* Devices from which RAID is composed. */
    void getDevs( std::list<string>& devices, bool all=true, bool spare=false ) const;


    void getSpareDevs(std::list<string>& devices );

    /* MD Device major number. */
    static unsigned mdMajor();

    static void activate( bool val, const string& tmpDir  );

    static bool isActive() { return active; }

	static bool hasPartitionTable(const string& name, SystemInfo& systeminfo);
	static bool hasFileSystem(const string& name, SystemInfo& systeminfo);

    static bool isImsmPlatform();

    static bool matchRegex( const string& dev );
    static bool mdStringNum( const string& name, unsigned& num );

    // This function will scann for MD RAIDs and will return
    // list with detected RAID names.
    static int scanForRaid(list<string>& raidNames);

    /* filterMdPartCo
     * Get list of detected MD RAIDs and filters them for
     * those which can be handled by MdPartCo.
     */
    static list<string> filterMdPartCo(list<string>& raidList,
				       SystemInfo& systeminfo,
                                       bool isInst);

    /* Returns uuid and possibly mdName for given MD Device.
     * Input parameter: dev name - like md1 */
    static bool getUuidName(const string dev,string& uuid, string& mdName);

    protected:
    // iterators over partitions
    // protected typedefs for iterators over partitions
    typedef CastIterator<VIter, MdPart *> MdPartInter;
    typedef CastIterator<CVIter, const MdPart *> MdPartCInter;
    template< class Pred >
        struct MdPartPI { typedef ContainerIter<Pred, MdPartInter> type; };
    template< class Pred >
        struct MdPartCPI { typedef ContainerIter<Pred, MdPartCInter> type; };
    typedef CheckFnc<const MdPart> CheckFncMdPart;
    typedef CheckerIterator< CheckFncMdPart, MdPartPI<CheckFncMdPart>::type,
                             MdPartInter, MdPart > MdPartPIterator;
    typedef CheckerIterator< CheckFncMdPart, MdPartCPI<CheckFncMdPart>::type,
                             MdPartCInter, const MdPart > MdPartCPIterator;
    typedef DerefIterator<MdPartPIterator,MdPart> MdPartIter;
    typedef DerefIterator<MdPartCPIterator,const MdPart> ConstMdPartIter;
    typedef IterPair<MdPartIter> MdPartPair;
    typedef IterPair<ConstMdPartIter> ConstMdPartPair;

    MdPartPair mdpartPair( bool (* CheckMdPart)( const MdPart& )=NULL)
        {
        return( MdPartPair( mdpartBegin( CheckMdPart ), mdpartEnd( CheckMdPart ) ));
        }
    MdPartIter mdpartBegin( bool (* CheckMdPart)( const MdPart& )=NULL)
        {
        IterPair<MdPartInter> p( (MdPartInter(begin())), (MdPartInter(end())) );
        return( MdPartIter( MdPartPIterator( p, CheckMdPart )) );
        }
    MdPartIter mdpartEnd( bool (* CheckMdPart)( const MdPart& )=NULL)
        {
        IterPair<MdPartInter> p( (MdPartInter(begin())), (MdPartInter(end())) );
        return( MdPartIter( MdPartPIterator( p, CheckMdPart, true )) );
        }

    ConstMdPartPair mdpartPair( bool (* CheckMdPart)( const MdPart& )=NULL) const
        {
        return( ConstMdPartPair( mdpartBegin( CheckMdPart ), mdpartEnd( CheckMdPart ) ));
        }
    ConstMdPartIter mdpartBegin( bool (* CheckMdPart)( const MdPart& )=NULL) const
        {
        IterPair<MdPartCInter> p( (MdPartCInter(begin())), (MdPartCInter(end())) );
        return( ConstMdPartIter( MdPartCPIterator( p, CheckMdPart )) );
        }
    ConstMdPartIter mdpartEnd( bool (* CheckMdPart)( const MdPart& )=NULL) const
        {
        IterPair<MdPartCInter> p( (MdPartCInter(begin())), (MdPartCInter(end())) );
        return( ConstMdPartIter( MdPartCPIterator( p, CheckMdPart, true )) );
        }

    MdPartCo( Storage * const s, const string& File );
    virtual void print( std::ostream& s ) const { s << *this; }
    virtual Container* getCopy() const { return( new MdPartCo( *this ) ); }
    void activate_part( bool val );
    void init(const ProcParts& ppart);
    void createDisk(const ProcParts& ppart);
    void getVolumes(const ProcParts& ppart);
    void updatePointers( bool invalid=false );
    void updateMinor();
    virtual void newP( MdPart*& dm, unsigned num, Partition* p );
    int addNewDev( string& device );
    int updateDelDev();
    void handleWholeDevice();
    void removeFromMemory();
    void removePresentPartitions();
    bool validPartition( const Partition* p );
    bool findMdPart( unsigned nr, MdPartIter& i );

    void updateEntry();
    string mdadmLine() const;

    static bool partNotDeleted( const MdPart&d ) { return( !d.deleted() ); }

    int doCreate( Volume* v );
    int doRemove( Volume* v );
    int doResize( Volume* v );
    int doSetType( MdPart* v );
    int doCreateLabel();
    virtual int doRemove();
    virtual Text removeText( bool doing ) const;
    virtual Text setDiskLabelText( bool doing ) const;

    void getMajorMinor();

    /* Initialize the MD part of object.*/
    void initMd();

    void setSize(unsigned long long size );

    bool isMdPart(const string& name) const;

    void getPartNum(const string& device, unsigned& num) const;

    void getMdProps();

    void setSpares();

	int unuseDevs();

    void logData(const string& Dir) const;

    list<string> udev_id;

    string logfile_name;

    Disk* disk;
    bool del_ptable;

    /* RAID Related */

    /* Returns container */
    void getParent();

    void setMetaData();

    void setMdDevs();

    void setMdParity();

    /* returns devices listed as slaves in sysfs directory */
    void getSlaves(const string name, std::list<string>& devs_list );

    /* fields in 'map' file */
    enum mdMap { MAP_DEV=0, MAP_META, MAP_UUID, MAP_NAME };
    static bool findMdMap(std::ifstream& file);


    int getContMember();

    //Input: 'mdXXX' device.
    static CType envSelection(const string& name);
    static bool havePartsInProc(const string& name, SystemInfo& systeminfo);

    static void getMdMajor();

    unsigned long chunk_size;
    storage::MdType md_type;
    storage::MdParity md_parity;
    storage::MdArrayState md_state;

    /* Md Container - */
    bool   has_container;
    string parent_container;
    string parent_uuid;
    string parent_metadata;
    string parent_md_name;
    string md_metadata;
    string md_uuid;
    string sb_ver;
    bool destrSb;
    std::list<string> devs;
    std::list<string> spare;

    static unsigned md_major;

    /* Name that is present in /dev/md directory.*/
    string md_name;

    static const string sysfs_path;

    enum MdProperty
    {
      METADATA=0,
      COMPONENT_SIZE,
      CHUNK_SIZE,
      ARRAY_STATE,
      LEVEL,
      LAYOUT,
      /* ... */
      MDPROP_LAST,
    };

    static const string md_props[MDPROP_LAST];

    bool readProp(enum MdProperty prop, string& val) const;

    /* For that RAID type parity means something */
    bool hasParity() const
    { return md_type == RAID5 || md_type == RAID6 || md_type == RAID10; }

    mutable storage::MdPartCoInfo info;

    static bool active;

    };
}

#endif
