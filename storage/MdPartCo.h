/*
 * File: MdPartCo.cc
 *
 * Declaration of MdPartCo class which represents single MD Device (RAID
 * Volume) like md126 which is a Container for partitions.
 *
 * Copyright (c) 2009, Intel Corporation.
 * Copyright (c) [2009-2010] Novell, Inc.
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
	friend class Md;

    public:

	MdPartCo(Storage* s, const string& name, const string& device, SystemInfo& systeminfo);
    MdPartCo(const MdPartCo& c);
    virtual ~MdPartCo();

    const string& labelName() const { return disk->labelName(); }
	virtual list<string> udevId() const { return udev_id; }
    unsigned numPartitions() const { return disk->numPartitions(); }
    static storage::CType staticType() { return storage::MDPART; }
    friend std::ostream& operator<< (std::ostream&, const MdPartCo& );

	void setUdevData(SystemInfo& systeminfo);

	virtual string procName() const { return nm; }
	virtual string sysfsPath() const;

	static bool notDeleted(const MdPartCo& c) { return !c.deleted(); }

	int getMdPartCoState(MdPartCoStateInfo& info) const;

    int createPartition( storage::PartitionType type, long unsigned start,
        long unsigned len, string& device,
        bool checkRelaxed=false );
    int createPartition( long unsigned len, string& device,
        bool checkRelaxed=false );
    int createPartition( storage::PartitionType type, string& device );
    int removePartition( unsigned nr );
    int changePartitionId( unsigned nr, unsigned id );
    int forgetChangePartitionId( unsigned nr );
	int changePartitionArea(unsigned nr, const Region& cylRegion, bool checkRelaxed = false);
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

	string getPartName(unsigned nr) const;
	string getPartDevice(unsigned nr) const;

    virtual void getCommitActions(list<commitAction>& l) const;
    virtual void getToCommit(CommitStage stage, list<const Container*>& col,
			     list<const Volume*>& vol) const;
    virtual int commitChanges( storage::CommitStage stage );
    int commitChanges( storage::CommitStage stage, Volume* vol );

    Partition* getPartition( unsigned nr, bool del );

    void getInfo( storage::MdPartCoInfo& info ) const;
    bool equalContent( const Container& rhs ) const;

	void logDifference(std::ostream& log, const MdPartCo& rhs) const;
	virtual void logDifferenceWithVolumes(std::ostream& log, const Container& rhs) const;

    MdPartCo& operator= ( const MdPartCo& rhs );
    static string undevName( const string& name );

	static list<string> getMdRaids(SystemInfo& systeminfo);

	void syncMdadm(EtcMdadm* mdadm) const;

    /* Returns Md number. */
    unsigned nr() const { return mnr; }

	unsigned long chunkSizeK() const { return chunk_k; }

    storage::MdType personality() const { return md_type; }

	const string& getMdUuid() const { return md_uuid; }

    /* Devices from which RAID is composed. */
	list<string> getDevs(bool all = true, bool spare = false) const;

    static void activate( bool val, const string& tmpDir  );

    static bool isActive() { return active; }

	static bool hasPartitionTable(const string& name, SystemInfo& systeminfo);
	static bool hasFileSystem(const string& name, SystemInfo& systeminfo);

    static bool matchRegex( const string& dev );
    static bool mdStringNum( const string& name, unsigned& num );

    // This function will scan for MD RAIDs and will return
    // list with detected RAID names.
    static bool scanForRaid(list<string>& raidNames);

    /* filterMdPartCo
     * Get list of detected MD RAIDs and filters them for
     * those which can be handled by MdPartCo.
     */
	static list<string> filterMdPartCo(const list<string>& raidList, SystemInfo& systeminfo,
					   bool instsys);

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

    virtual void print( std::ostream& s ) const { s << *this; }
    virtual Container* getCopy() const { return( new MdPartCo( *this ) ); }
    void activate_part( bool val );
    void init(SystemInfo& systeminfo);
    void createDisk(SystemInfo& systeminfo);
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

	bool updateEntry(EtcMdadm* mdadm) const;

    int doCreate( Volume* v );
    int doRemove( Volume* v );
    int doResize( Volume* v );
    int doSetType( MdPart* v );
    int doCreateLabel();
    virtual int doRemove();
    virtual Text removeText( bool doing ) const;
    virtual Text setDiskLabelText( bool doing ) const;

    /* Initialize the MD part of object.*/
	void initMd(SystemInfo& systeminfo);

    void setSize(unsigned long long size );

    bool isMdPart(const string& name) const;

    void getPartNum(const string& device, unsigned& num) const;

	void unuseDevs() const;

    Disk* disk;

    //Input: 'mdXXX' device.
    static CType envSelection(const string& name);
    static bool havePartsInProc(const string& name, SystemInfo& systeminfo);

	MdType md_type;
	MdParity md_parity;
	unsigned long chunk_k;
	string md_uuid;
	string md_name;		// line in /dev/md/*
	string sb_ver;
	bool destrSb;
	list<string> devs;
	list<string> spare;

	list<string> udev_id;

	// In case of IMSM and DDF raids there is 'container'.
	bool has_container;
	string parent_container;
	string parent_uuid;
	string parent_md_name;
	string parent_metadata;
	string parent_member;

    mutable storage::MdPartCoInfo info; // workaround for broken ycp bindings

    static bool active;

    };
}

#endif
