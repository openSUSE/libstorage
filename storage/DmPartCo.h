/*
 * Copyright (c) [2004-2009] Novell, Inc.
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


#ifndef DM_PART_CO_H
#define DM_PART_CO_H

#include <list>

#include "storage/PeContainer.h"
#include "storage/Disk.h"
#include "storage/DmPart.h"


namespace storage
{
    using std::list;


class Storage;
    class SystemInfo;
class Region;


class DmPartCo : public PeContainer
    {
    friend class Storage;

    public:

	DmPartCo(Storage* s, const string& name, const string& device, CType t,
		 SystemInfo& systeminfo);
	DmPartCo(const DmPartCo& c);
	virtual ~DmPartCo();

	unsigned long long sizeK() const { return size_k; }
	const string& labelName() const { return disk->labelName(); }
	virtual list<string> udevId() const { return udev_id; }
	unsigned numPartitions() const { return disk->numPartitions(); }
	static storage::CType staticType() { return storage::DMRAID; }
	friend std::ostream& operator<< (std::ostream&, const DmPartCo& );
	void setUdevData(const list<string>& id);

	virtual string procName() const { return "dm-" + decString(mnr); }
	virtual string sysfsPath() const;

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
	int freeCylindersAroundPartition(const DmPart* p, unsigned long& freeCylsBefore,
					 unsigned long& freeCylsAfter) const;
	int resizePartition( DmPart* p, unsigned long newCyl );
	int resizeVolume( Volume* v, unsigned long long newSize );
	int removeVolume( Volume* v );
	int removeDmPart();

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
	virtual void getToCommit(storage::CommitStage stage, list<const Container*>& col,
				 list<const Volume*>& vol) const;
	virtual int commitChanges( storage::CommitStage stage );
	int commitChanges( storage::CommitStage stage, Volume* vol );

	Partition* getPartition( unsigned nr, bool del );
	void getInfo( storage::DmPartCoInfo& info ) const;
	bool equalContent( const DmPartCo& rhs ) const;

	void logDifference(std::ostream& log, const DmPartCo& rhs) const;

	static string undevName( const string& name );

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

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const = 0; // { return( new DmPartCo( *this ) ); }
	void activate_part( bool val );
	void init(SystemInfo& systeminfo);
	void createDisk(SystemInfo& systeminfo);
	void getVolumes(SystemInfo& si);
	void updatePointers( bool invalid=false );
	void updateMinor();
	virtual void newP( DmPart*& dm, unsigned num, Partition* p, SystemInfo& si );
	virtual void newP( DmPart*& dm, unsigned num, Partition* p );
	int addNewDev( string& device );
	int updateDelDev();
	void handleWholeDevice();
	void handleWholeDevice(SystemInfo& si);
	void removeFromMemory();
	void removePresentPartitions();
	bool validPartition( const Partition* p );
	bool findDm( unsigned nr, DmPartIter& i );

	int doCreate( Volume* v );
	int doRemove( Volume* v );
	int doResize( Volume* v );
	int doSetType( DmPart* v );
	int doCreateLabel();
	virtual int doRemove();
	virtual Text removeText( bool doing ) const;
	virtual Text setDiskLabelText( bool doing ) const;

	list<string> udev_id;
	Disk* disk;
	bool active;

	mutable storage::DmPartCoInfo info; // workaround for broken ycp bindings

    private:

	DmPartCo& operator=(const DmPartCo&); // disallow

    };

}

#endif
