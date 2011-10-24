/*
 * Copyright (c) [2004-2010] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#ifndef DISK_H
#define DISK_H

#include <list>

#include "storage/Container.h"
#include "storage/Partition.h"
#include "storage/Geometry.h"


namespace storage
{
    using std::list;


class Storage;
class SystemCmd;
    class SystemInfo;
    class ArchInfo;
class ProcParts;
class Region;


class Disk : public Container
    {
    friend class Storage;
    friend class DmPartCo;
    friend class MdPartCo;

    struct label_info
	{
	string name;
	bool extended;
	unsigned primary;
	unsigned logical;
	unsigned long long max_sectors;
	};

    public:

	Disk(Storage* s, const string& name, const string& device, unsigned long long Size,
	     SystemInfo& systeminfo);
	Disk(Storage* s, const string& name, const string& device, unsigned num, 
	     unsigned long long Size, SystemInfo& systeminfo);
	Disk(Storage* s, const xmlNode* node);
	Disk(const Disk& c);
	virtual ~Disk();

	void saveData(xmlNode* node) const;

	unsigned long cylinders() const { return geometry.cylinders; }
	unsigned heads() const { return geometry.heads; }
	unsigned sectors() const { return geometry.sectors; }
	unsigned sectorSize() const { return geometry.sector_size; }
	const Geometry& getGeometry() const { return geometry; }

	Region usableCylRegion() const;

	unsigned long numMinor() const { return range; }
	unsigned maxPrimary() const { return max_primary; }
	bool extendedPossible() const { return ext_possible; }
	unsigned maxLogical() const { return max_logical; }
	const string& labelName() const { return label; }
	virtual string udevPath() const { return udev_path; }
	virtual list<string> udevId() const { return udev_id; }
	void setSlave( bool val=true ) { dmp_slave=val; }
	void setAddpart( bool val=true ) { no_addpart=!val; }
	void setNumMinor( unsigned long val ) { range=val; }

	virtual string procName() const { return nm; }
	virtual string sysfsPath() const;
	static string sysfsPath( const string& device );

	unsigned numPartitions() const;
	bool isDasd() const { return( nm.find("dasd")==0 ); }
	bool isIScsi() const { return transport == ISCSI; }
	static bool isIScsi(const Disk& d) { return d.isIScsi(); }
	bool isLogical( unsigned nr ) const;
	bool detect(SystemInfo& systeminfo);
	static storage::CType staticType() { return storage::DISK; }
	friend std::ostream& operator<< (std::ostream&, const Disk& );
	void triggerUdevUpdate() const;

	static bool needP( const string& dev );
	static string partNaming(const string& disk);
	void setUdevData(const string& path, const list<string>& id);
	virtual int createPartition( storage::PartitionType type, long unsigned start,
				     long unsigned len, string& device,
				     bool checkRelaxed=false );
	int createPartition( long unsigned len, string& device,
			     bool checkRelaxed=false );
	int createPartition( storage::PartitionType type, string& device );
	virtual int removePartition( unsigned nr );
	virtual int changePartitionId( unsigned nr, unsigned id );
	virtual int initializeDisk( bool ) { return storage::DISK_INIT_NOT_POSSIBLE; }
	bool initializeDisk() const { return init_disk; }
	void resetInitDisk() { init_disk=false; }
	int forgetChangePartitionId( unsigned nr );
	int changePartitionArea(unsigned nr, const Region& cylRegion, bool checkRelaxed = false);
	int nextFreePartition(storage::PartitionType type, unsigned& nr,
			      string& device) const;
	int destroyPartitionTable( const string& new_label );
	unsigned availablePartNumber(storage::PartitionType type = storage::PRIMARY) const;
	virtual void getCommitActions(list<commitAction>& l) const;
	virtual void getToCommit(storage::CommitStage stage, list<const Container*>& col,
				 list<const Volume*>& vol) const;
	virtual int commitChanges( storage::CommitStage stage );
	int commitChanges( storage::CommitStage stage, Volume* vol );
	int freeCylindersAroundPartition(const Partition* p, unsigned long& freeCylsBefore,
					 unsigned long& freeCylsAfter) const;
	virtual int resizePartition( Partition* p, unsigned long newCyl );
	int resizeVolume( Volume* v, unsigned long long newSize );
	int removeVolume( Volume* v );
	void getUnusedSpace(std::list<Region>& free, bool all = true, 
			    bool logical = false) const;
	unsigned int numPrimary() const;
	bool hasExtended() const;
	unsigned int numLogical() const;
	Text setDiskLabelText(bool doing) const;

	unsigned long long cylinderToKb(unsigned long cylinder) const
	    { return geometry.cylinderToKb(cylinder); }
	unsigned long kbToCylinder(unsigned long long kb) const
	    { return geometry.kbToCylinder(kb); }

	unsigned long long sectorToKb(unsigned long long sector) const
	    { return geometry.sectorToKb(sector); }
	unsigned long long kbToSector(unsigned long long kb) const
	    { return geometry.kbToSector(kb); }

	string getPartName(unsigned nr) const;
	string getPartDevice(unsigned nr) const;

	void getInfo( storage::DiskInfo& info ) const;
	bool equalContent( const Container& rhs ) const;

	void logDifference(std::ostream& log, const Disk& rhs) const;
	virtual void logDifferenceWithVolumes(std::ostream& log, const Container& rhs) const;

	bool FakeDisk() const { return(range==1); }

	static std::pair<string,unsigned> getDiskPartition( const string& dev );

	static bool getDlabelCapabilities(const string& dlabel,
					  storage::DlabelCapabilities& dlabelcapabilities);

	struct SysfsInfo
	{
	    unsigned long range;
	    unsigned long long size;
	    bool vbd;
	};

	static bool getSysfsInfo(const string& sysfsdir, SysfsInfo& sysfsinfo);

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
        typedef IterPair<PartIter> PartPair;

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

    public:
	typedef DerefIterator<PartCPIterator,const Partition> ConstPartIter;
        typedef IterPair<ConstPartIter> ConstPartPair;
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

    protected:

	virtual bool detectGeometry();
	virtual bool detectPartitions(SystemInfo& systeminfo);
	bool getSysfsInfo();
	int checkSystemError( const string& cmd_line, const SystemCmd& cmd ) const;
	int execCheckFailed( const string& cmd_line, bool stop_hald=true );
	int execCheckFailed( SystemCmd& cmd, const string& cmd_line,
	                     bool stop_hald=true );
	bool checkPartedOutput(SystemInfo& systeminfo);
	list<string> partitionsKernelKnowns(const ProcParts& parts) const;
	bool checkPartedValid(SystemInfo& systeminfo, list<Partition*>& pl,
			      unsigned long& rng) const;
	virtual bool checkPartitionsValid(SystemInfo& systeminfo, const list<Partition*>& pl) const;

	bool callDelpart(unsigned nr) const;
	bool callAddpart(unsigned nr, const Region& blkRegion) const;

	bool getPartedValues( Partition *p ) const;
	bool getPartedSectors( const Partition *p, unsigned long long& start,
	                       unsigned long long& end ) const;
	const Partition * getPartitionAfter( const Partition * p ) const;
	void addPartition( unsigned num, unsigned long long sz, 
	                   SystemInfo& ppart );
	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new Disk( *this ) ); }
	virtual void redetectGeometry();
	void changeNumbers( const PartIter& b, const PartIter& e, 
	                    unsigned start, int incr );
	int createChecks(PartitionType& type, const Region& cylRegion, bool checkRelaxed) const;
	void removePresentPartitions();
	void removeFromMemory();
	void enlargeGpt();

	/* size of extended partition in proc and sysfs in 512 byte blocks */
	unsigned long long procExtendedBlks() const;

	virtual int doCreate( Volume* v );
	virtual int doRemove( Volume* v );
	virtual int doResize( Volume* v );
	virtual int doSetType( Volume* v );
	virtual int doCreateLabel();

	virtual void logData(const string& Dir) const;

	void setLabelData( const string& );

	string defaultLabel() const;

	static const label_info labels[];
	static const string p_disks[];

	Geometry geometry;
	Geometry new_geometry;

	string label;
	string udev_path;
	list<string> udev_id;
	string detected_label;
	string logfile_name;
	unsigned max_primary;
	bool ext_possible;
	unsigned max_logical;
	bool init_disk;
	Transport transport;
	bool dmp_slave;
	bool no_addpart;
	bool gpt_enlarge;
	unsigned long range;
	bool del_ptable;

	mutable storage::DiskInfo info; // workaround for broken ycp bindings

    private:

	Disk& operator=(const Disk&); // disallow

    };

}

#endif
