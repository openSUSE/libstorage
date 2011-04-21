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


#ifndef STORAGE_H
#define STORAGE_H

#include <ostream>
#include <list>
#include <map>

#include "storage/StorageInterface.h"
#include "storage/StorageTypes.h"
#include "storage/StorageTmpl.h"
#include "storage/Container.h"
#include "storage/Volume.h"
#include "storage/Disk.h"
#include "storage/Partition.h"
#include "storage/LvmVg.h"
#include "storage/LvmLv.h"
#include "storage/DmraidCo.h"
#include "storage/Dmraid.h"
#include "storage/DmmultipathCo.h"
#include "storage/Dmmultipath.h"
#include "storage/MdCo.h"
#include "storage/Md.h"
#include "storage/MdPartCo.h"
#include "storage/MdPart.h"
#include "storage/DmCo.h"
#include "storage/LoopCo.h"
#include "storage/Loop.h"
#include "storage/BtrfsCo.h"
#include "storage/Btrfs.h"
#include "storage/TmpfsCo.h"
#include "storage/Tmpfs.h"
#include "storage/NfsCo.h"
#include "storage/Nfs.h"
#include "storage/FilterIterator.h"
#include "storage/DerefIterator.h"
#include "storage/ListListIterator.h"
#include "storage/IterPair.h"
#include "storage/Lock.h"
#include "storage/FreeInfo.h"
#include "storage/ArchInfo.h"


namespace storage
{
    // workaround for broken YCP bindings
    extern CallbackProgressBar progress_bar_cb_ycp;
    extern CallbackShowInstallInfo install_info_cb_ycp;
    extern CallbackInfoPopup info_popup_cb_ycp;
    extern CallbackYesNoPopup yesno_popup_cb_ycp;
    extern CallbackCommitErrorPopup commit_error_popup_cb_ycp;
    extern CallbackPasswordPopup password_popup_cb_ycp;


template <CType Value>
class CheckType
    {
    public:
	bool operator()( const Container& d ) const
	    {
	    return( d.type()==Value );
	    }
    };

template< class Iter, CType Value, class CastResult >
class CastCheckIterator : public FilterIterator< CheckType<Value>, Iter >
    {
    typedef FilterIterator<CheckType<Value>, Iter> _bclass;
    public:
	typedef CastResult value_type;
	typedef CastResult& reference;
	typedef CastResult* pointer;

	CastCheckIterator() : _bclass() {}
	CastCheckIterator( const Iter& b, const Iter& e, bool atend=false) :
	    _bclass( b, e, CheckType<Value>(), atend ) {}
	CastCheckIterator( const IterPair<Iter>& pair, bool atend=false) :
	    _bclass( pair, CheckType<Value>(), atend ) {}
	template< class It >
	CastCheckIterator( const It& i) : _bclass( i.begin(), i.end(), CheckType<Value>() )
	    { this->m_cur=i.cur();}
	CastResult operator*() const
	    {
	    return( static_cast<CastResult>(_bclass::operator*()) );
	    }
	CastResult* operator->() const
	    {
	    return( static_cast<CastResult*>(_bclass::operator->()) );
	    }
	CastCheckIterator& operator++()
	    {
	    _bclass::operator++(); return(*this);
	    }
	CastCheckIterator operator++(int)
	    {
	    y2war( "Expensive ++ CastCheckIterator" );
	    CastCheckIterator tmp(*this);
	    _bclass::operator++();
	    return(tmp);
	    }
	CastCheckIterator& operator--()
	    {
	    _bclass::operator--(); return(*this);
	    }
	CastCheckIterator operator--(int)
	    {
	    y2war( "Expensive -- CastCheckIterator" );
	    CastCheckIterator tmp(*this);
	    _bclass::operator--();
	    return(tmp);
	    }
    };

template < bool (* FncP)( const Container& c ) >
class CheckByFnc
    {
    public:
	bool operator()( const Container& d ) const
	    {
	    return( (*FncP)(d) );
	    }
    };

template< class Iter, bool (* FncP)( const Container& c ), class CastResult >
class CastCheckFncIterator : public FilterIterator< CheckByFnc<FncP>, Iter >
    {
    typedef FilterIterator<CheckByFnc<FncP>, Iter> _bclass;
    public:
	typedef CastResult value_type;
	typedef CastResult& reference;
	typedef CastResult* pointer;

	CastCheckFncIterator() : _bclass() {}
	CastCheckFncIterator( const Iter& b, const Iter& e, bool atend=false) :
	    _bclass( b, e, CheckByFnc<FncP>(), atend ) {}
	CastCheckFncIterator( const IterPair<Iter>& pair, bool atend=false) :
	    _bclass( pair, CheckByFnc<FncP>(), atend ) {}
	template< class It >
	CastCheckFncIterator( const It& i) : _bclass( i.begin(), i.end(), i.pred() )
	    { this->m_cur=i.cur();}
	CastResult operator*() const
	    {
	    return( static_cast<CastResult>(_bclass::operator*()) );
	    }
	CastResult* operator->() const
	    {
	    return( static_cast<CastResult*>(_bclass::operator->()) );
	    }
	CastCheckFncIterator& operator++()
	    {
	    _bclass::operator++(); return(*this);
	    }
	CastCheckFncIterator operator++(int)
	    {
	    y2war( "Expensive ++ CastCheckFncIterator" );
	    CastCheckFncIterator tmp(*this);
	    _bclass::operator++();
	    return(tmp);
	    }
	CastCheckFncIterator& operator--()
	    {
	    _bclass::operator--(); return(*this);
	    }
	CastCheckFncIterator operator--(int)
	    {
	    y2war( "Expensive -- CastCheckFncIterator" );
	    CastCheckFncIterator tmp(*this);
	    _bclass::operator--();
	    return(tmp);
	    }
    };


class EtcFstab;
    class EtcMdadm;
class DiskData;


    /**
     * \brief Main class to access libstorage functionality.
     *
     * This is the main class with that one can get access to the
     * functionality provided by libstorage. It contains a list of container
     * objects.
     *
     * All modifying member functions of the storage library will go through
     * Storage class. This is the central place where things like readonly
     * access, locking, testmode, inst-sys etc. are handled. It has the
     * additional advantage the the complete class hierarchy below Storage
     * could be changed without affecting the user interface of libstorage.
     */
    class Storage : public storage::StorageInterface, boost::noncopyable
    {
    protected:

	typedef std::list<Container*> CCont;
	typedef CCont::iterator CIter;
	typedef CCont::const_iterator CCIter;

	static bool isMd( const Container&d )
	    { return( d.type()==storage::MD ); }
	static bool isLoop( const Container&d )
	    { return( d.type()==storage::LOOP ); }
	static bool isNfs( const Container&d )
	    { return( d.type()==storage::NFSC ); }
	static bool isDm( const Container&d )
	    { return( d.type()==storage::DM ); }
	static bool isBtrfs( const Container&d )
	    { return( d.type()==storage::BTRFSC ); }
	static bool isNotBtrfs( const Container&d )
	    { return( d.type()!=storage::BTRFSC ); }
	static bool isTmpfs( const Container&d )
	    { return( d.type()==storage::TMPFSC ); }

    public:

	static bool isDmPart( const Container&d )
	    { return d.type() == storage::DMRAID || d.type() == storage::DMMULTIPATH; }
	static bool isMdPart( const Container&d )
	    { return d.type() == storage::MDPART; }

	Storage(const Environment& env);

	bool readonly() const { return env.readonly; }
	bool testmode() const { return env.testmode; }
	bool autodetect() const { return env.autodetect; }
	bool instsys() const { return env.instsys; }
	string logdir() const { return env.logdir; }
	string testdir() const { return env.testdir; }

	void setCacheChanges( bool val=true ) { cache = val; }
	bool isCacheChanges() const { return( cache ); }
	void assertInit() { if( !initialized ) initialize(); }
	void rescanEverything();
	bool rescanCryptedObjects();
	int checkCache();
	const string& root() const { return( rootprefix ); }
	string prependRoot(const string& mp) const;
	const string& tmpDir() const { return tempdir; }
	bool hasIScsiDisks() const;
	string bootMount() const;

	const ArchInfo& getArchInfo() const { return archinfo; }

	EtcFstab* getFstab() { return fstab; }
	EtcMdadm* getMdadm() { return mdadm; }
	void handleLogFile(const string& name) const;
	static bool testFilesEqual( const string& n1, const string& n2 );
	void printInfo(std::ostream& str) const;
	void logCo(const Container* c) const;
	void logProcData(const string& str = "") const;

	void clearUsedBy(const string& dev);
	void clearUsedBy(const list<string>& devs);
	void setUsedBy(const string& dev, UsedByType type, const string& device);
	void setUsedBy(const list<string>& devs, UsedByType type, const string& device);
	void setUsedByBtrfs( const string& dev, const string& uuid );
	void addUsedBy(const string& dev, UsedByType type, const string& device);
	void addUsedBy(const list<string>& devs, UsedByType type, const string& device);
	void removeUsedBy(const string& dev, UsedByType type, const string& device);
	void removeUsedBy(const list<string>& devs, UsedByType type, const string& device);
	bool isUsedBy(const string& dev);
	bool isUsedBy(const string& dev, UsedByType type);
	bool isUsedBySingleBtrfs( const Volume& vol ) const;
	bool canRemove( const Volume& vol ) const;

	void fetchDanglingUsedBy(const string& dev, list<UsedBy>& uby);

	bool canUseDevice( const string& dev, bool disks_allowed=false );
	bool knownDevice( const string& dev, bool disks_allowed=false );
	bool setDmcryptData( const string& dev, const string& dm, 
	                     unsigned dmnum, unsigned long long siz,
			     storage::EncryptType typ );
	bool deletedDevice(const string& dev) const;
	bool isDisk( const string& dev );
	const Volume* getVolume( const string& dev );
	unsigned long long deviceSize( const string& dev );
	string deviceByNumber(const string& majmin) const;
	const Device* deviceByNumber( unsigned long maj, unsigned long min ) const;

	void syncMdadm();
	void rootMounted();
	bool isRootMounted() const { return( root_mounted ); }

	string findNormalDevice( const string& device );
	bool findVolume( const string& device, Volume const* &vol, 
	                 bool no_btrfsc=false );
	bool findDm( const string& device, const Dm*& dm );
	bool findDmUsing( const string& device, const Dm*& dm );
	bool findDevice( const string& dev, const Device* &vol,
	                 bool search_by_minor=false );
	bool removeDm( const string& device );
	int unaccessDev( const string& device );

	virtual ~Storage();

	// functions for interface

        void getContainers( deque<storage::ContainerInfo>& infos );
	int getDiskInfo( const string& disk, storage::DiskInfo& info);
	int getLvmVgInfo( const string& name, storage::LvmVgInfo& info);
	int getDmraidCoInfo( const string& name, storage::DmraidCoInfo& info);
	int getDmmultipathCoInfo( const string& name, storage::DmmultipathCoInfo& info);
	int getContDiskInfo( const string& disk, storage::ContainerInfo& cinfo,
	                     storage::DiskInfo& info);
	int getContLvmVgInfo( const string& name, storage::ContainerInfo& cinfo,
	                      storage::LvmVgInfo& info);
	int getContDmraidCoInfo( const string& name,
	                         storage::ContainerInfo& cinfo,
	                         storage::DmraidCoInfo& info );
	int getContDmmultipathCoInfo( const string& name,
				      storage::ContainerInfo& cinfo,
				      storage::DmmultipathCoInfo& info );
	void getVolumes (deque<storage::VolumeInfo>& vlist);
	int getVolume( const string& device, storage::VolumeInfo& info);
	int getPartitionInfo( const string& disk,
			      deque<storage::PartitionInfo>& plist );
	int getLvmLvInfo( const string& name,
			  deque<storage::LvmLvInfo>& plist );
	int getMdInfo( deque<storage::MdInfo>& plist );
	int getMdPartInfo( const string& device, deque<storage::MdPartInfo>& plist );
	int getDmInfo( deque<storage::DmInfo>& plist );
	int getNfsInfo( deque<storage::NfsInfo>& plist );
	int getLoopInfo( deque<storage::LoopInfo>& plist );
	int getBtrfsInfo( deque<storage::BtrfsInfo>& plist );
	int getTmpfsInfo( deque<storage::TmpfsInfo>& plist );
	int getDmraidInfo( const string& name,
	                   deque<storage::DmraidInfo>& plist );
	int getDmmultipathInfo( const string& name,
				deque<storage::DmmultipathInfo>& plist );
	int getContVolInfo( const string& dev, ContVolInfo& info);

	bool getFsCapabilities( storage::FsType fstype,
	                        storage::FsCapabilities& fscapabilities) const;
	bool getDlabelCapabilities(const string& dlabel,
				   storage::DlabelCapabilities& dlabelcapabilities) const;

	list<string> getAllUsedFs() const;
	void setExtError( const string& txt );
	int createPartition( const string& disk, storage::PartitionType type,
	                     unsigned long start, unsigned long size,
			     string& device );
	int resizePartition( const string& device, unsigned long sizeCyl );
	int resizePartitionNoFs( const string& device, unsigned long sizeCyl );
	int nextFreePartition( const string& disk, storage::PartitionType type,
	                       unsigned &nr, string& device );
	int updatePartitionArea( const string& device,
				 unsigned long start, unsigned long size );
	int freeCylindersAroundPartition(const string& device, unsigned long& freeCylsBefore,
					 unsigned long& freeCylsAfter);
	int createPartitionKb( const string& disk, storage::PartitionType type,
	                       unsigned long long start,
			       unsigned long long sizek, string& device );
	int createPartitionAny( const string& disk, unsigned long long size,
				string& device );
	int createPartitionMax( const string& disk, storage::PartitionType type,
				string& device );
	unsigned long kbToCylinder( const string& disk, unsigned long long size );
	unsigned long long cylinderToKb( const string& disk, unsigned long size );
	int removePartition( const string& partition );
	int changePartitionId( const string& partition, unsigned id );
	int forgetChangePartitionId( const string& partition );

	string getPartitionPrefix(const string& disk);
	string getPartitionName(const string& disk, int partition_no);

	int getUnusedPartitionSlots(const string& disk, list<PartitionSlotInfo>& slots);
	int destroyPartitionTable( const string& disk, const string& label );
	int initializeDisk( const string& disk, bool value );
	string defaultDiskLabel(const string& device);

	int changeFormatVolume( const string& device, bool format,
	                        storage::FsType fs );
	int changeLabelVolume( const string& device, const string& label );
	int eraseLabelVolume( const string& device );
	int changeMkfsOptVolume( const string& device, const string& opts );
	int changeTunefsOptVolume( const string& device, const string& opts );
	int changeDescText( const string& device, const string& txt );
	int changeMountPoint( const string& device, const string& mount );
	int getMountPoint( const string& device, string& mount );
	int changeMountBy( const string& device, storage::MountByType mby );
	int getMountBy( const string& device, storage::MountByType& mby );
	int changeFstabOptions( const string&, const string& options );
	int getFstabOptions( const string& device, string& options );
	int addFstabOptions( const string&, const string& options );
	int removeFstabOptions( const string&, const string& options );
	int setCryptPassword( const string& device, const string& pwd );
	int verifyCryptPassword( const string& device, const string& pwd, 
	                         bool erase );
	int verifyCryptFilePassword( const string& file, const string& pwd );
	bool needCryptPassword( const string& device );
	int forgetCryptPassword( const string& device );
	int getCryptPassword( const string& device, string& pwd );
	int setCrypt( const string& device, bool val );
	int setCryptType( const string& device, bool val, EncryptType typ );
	int getCrypt( const string& device, bool& val );
	int setIgnoreFstab( const string& device, bool val );
	int getIgnoreFstab( const string& device, bool& val );
	int addFstabEntry( const string& device, const string& mount,
	                   const string& vfs, const string& options,
			   unsigned freq, unsigned passno );
	int resizeVolume(const string& device, unsigned long long newSizeK);
	int resizeVolumeNoFs(const string& device, unsigned long long newSizeK);
	int forgetResizeVolume( const string& device );
	void setRecursiveRemoval( bool val=true );
	bool getRecursiveRemoval() const { return recursiveRemove; }

	int getRecursiveUsing(const string& device, list<string>& devices);
	int getRecursiveUsingHelper(const string& device, list<string>& devices);

	void setZeroNewPartitions( bool val=true );
	bool getZeroNewPartitions() const { return zeroNewPartitions; }

	void setPartitionAlignment( PartAlign val );
	PartAlign getPartitionAlignment() const { return partAlignment; }

	void setDefaultMountBy(MountByType mby);
	MountByType getDefaultMountBy() const { return defaultMountBy; }

	void setDefaultFs (FsType fs);
	FsType getDefaultFs() const { return defaultFs; }

	void setDetectMountedVolumes( bool val=true );
	bool getDetectMountedVolumes() const { return detectMounted; }
	bool getEfiBoot();
	void setRootPrefix( const string& root );
	string getRootPrefix() const { return rootprefix; }
	int removeVolume( const string& device );
	int removeUsing(const string& device, const list<UsedBy>& uby);
	bool checkDeviceMounted(const string& device, list<string>& mps);
	bool umountDevice( const string& device )
	    { return( umountDev( device, true )); }
	bool umountDev( const string& device, bool dounsetup=false );
	bool mountDev( const string& device, const string& mp, bool ro=true,
	               const string& opts="" );
	bool mountDevice( const string& device, const string& mp )
	    { return( mountDev( device, mp, false )); }
	bool mountDeviceOpts( const string& device, const string& mp,
	                      const string& opts )
	    { return( mountDev( device, mp, false, opts )); }
	bool mountDeviceRo( const string& device, const string& mp, 
	                    const string& opts )
	    { return( mountDev( device, mp, true, opts )); }
	int activateEncryption( const string& device, bool on );
	bool readFstab( const string& dir, deque<storage::VolumeInfo>& infos);

	bool getFreeInfo(const string& device, bool get_resize, ResizeInfo& resize_info, 
			 bool get_content, ContentInfo& content_info, bool use_cache);

	int createBackupState( const string& name );
	int removeBackupState( const string& name );
	int restoreBackupState( const string& name );
	bool checkBackupState( const string& name ) const;
	bool equalBackupStates( const string& lhs, const string& rhs,
	                        bool verbose_log ) const;

	int createLvmVg( const string& name, unsigned long long peSizeK,
	                 bool lvm1, const deque<string>& devs );
	int removeLvmVg( const string& name );
	int extendLvmVg( const string& name, const deque<string>& devs );
	int shrinkLvmVg( const string& name, const deque<string>& devs );
	int createLvmLv( const string& vg, const string& name,
			 unsigned long long sizeK, unsigned stripes,
			 string& device );
	int removeLvmLvByDevice( const string& device );
	int removeLvmLv( const string& vg, const string& name );
	int changeLvStripeCount( const string& vg, const string& name,
				 unsigned long stripes );
	int changeLvStripeSize( const string& vg, const string& name,
				unsigned long long stripeSize );

	int createLvmLvSnapshot(const string& vg, const string& origin,
				const string& name, unsigned long long cowSizeK,
				string& device);
	int removeLvmLvSnapshot(const string& vg, const string& name);
	int getLvmLvSnapshotStateInfo(const string& vg, const string& name, 
				      LvmLvSnapshotStateInfo& info);

	int nextFreeMd(unsigned& nr, string &device);
	bool checkMdNumber(unsigned num);
	int createMd(const string& name, MdType rtype, const list<string>& devs,
		     const list<string>& spares);
	int createMdAny(MdType rtype, const list<string>& devs, const list<string>& spares,
			string& device);
	int removeMd( const string& name, bool destroySb=true );
	int extendMd(const string& name, const list<string>& devs, const list<string>& spares);
	int shrinkMd(const string& name, const list<string>& devs, const list<string>& spares);
	int changeMdType( const string& name, storage::MdType rtype );
	int changeMdChunk( const string& name, unsigned long chunk );
	int changeMdParity( const string& name, storage::MdParity ptype );
	int checkMd( const string& name );
	int getMdStateInfo(const string& name, MdStateInfo& info);
	int computeMdSize(MdType md_type, const list<string>& devices, const list<string>& spares,
			  unsigned long long& sizeK);
	list<int> getMdAllowedParity(MdType md_type, unsigned devices );
	void setImsmDriver(ImsmDriver val) { imsm_driver = val; }
	ImsmDriver getImsmDriver() const { return imsm_driver; }

	int getMdPartCoInfo( const string& name, MdPartCoInfo& info);
	int getContMdPartCoInfo( const string& name, ContainerInfo& cinfo,
	                                         MdPartCoInfo& info);
        int getMdPartCoStateInfo(const string& name, MdPartCoStateInfo& info);
        int removeMdPartCo(const string& devName, bool destroySb);

	int addNfsDevice(const string& nfsDev, const string& opts,
			 unsigned long long sizeK, const string& mp, bool nfs4);
	int checkNfsDevice(const string& nfsDev, const string& opts, bool nfs4,
			   unsigned long long& sizeK);

	int createFileLoop( const string& lname, bool reuseExisting,
			    unsigned long long sizeK, const string& mp,
			    const string& pwd, string& device );
	int modifyFileLoop( const string& device, const string& lname, 
	                    bool reuseExisting, unsigned long long sizeK );
	int removeFileLoop( const string& lname, bool removeFile );

	int removeDmraid( const string& name );

	int createSubvolume( const string& device, const string& name );
	int removeSubvolume( const string& device, const string& name );
	int extendBtrfsVolume( const string& device, const string& dev );
	int extendBtrfsVolume( const string& device, const deque<string>& devs );
	int shrinkBtrfsVolume( const string& device, const string& dev );
	int shrinkBtrfsVolume( const string& device, const deque<string>& devs );
	int newBtrfs( const string& device );

	int addTmpfsMount( const string& mp, const string& opts );
	int removeTmpfsMount( const string& mp );

	void getCommitInfos(list<CommitInfo>& infos) const;
	const string& getLastAction() const { return lastAction.text; }
	const string& getExtendedErrorMessage() const { return extendedError; }
	void eraseCachedFreeInfo(const string& device);

	static void waitForDevice();
	static int waitForDevice(const string& device);

	static int zeroDevice(const string& device, unsigned long long sizeK, bool random = false,
			      unsigned long long beginK = 200, unsigned long long endK = 10);

	void getDiskList( bool (* CheckFnc)( const Disk& ),
	                  std::list<Disk*>& dl );
	void changeDeviceName( const string& old, const string& nw );

        int commit();

	string getErrorString(int error) const;

	void handleHald( bool stop );

	void activateHld(bool val = true);
	void activateMultipath(bool val = true);

	void removeDmTableTo( const Volume& vol );
	void removeDmTableTo( const string& device );
	void removeDmTableTo( unsigned long mjr, unsigned long mnr );
	bool removeDmTable( const string& table );
	bool removeDmMapsTo( const string& dev );
	bool checkDmMapsTo( const string& dev );
	void updateDmEmptyPeMap();
	void dumpObjectList();
	void dumpCommitInfos() const;
	bool mountTmpRo( const Volume* vol, string& mp )
	    { return mountTmp( vol, mp, true ); }
	bool mountTmp( const Volume* vol, string& mp, bool ro=false );

	void setCallbackProgressBar(CallbackProgressBar pfnc) { progress_bar_cb = pfnc; }
	CallbackProgressBar getCallbackProgressBar() const { return progress_bar_cb; }
	void setCallbackShowInstallInfo(CallbackShowInstallInfo pfnc) { install_info_cb = pfnc; }
	CallbackShowInstallInfo getCallbackShowInstallInfo() const { return install_info_cb; }
	void setCallbackInfoPopup(CallbackInfoPopup pfnc) { info_popup_cb = pfnc; }
	CallbackInfoPopup getCallbackInfoPopup() const { return info_popup_cb; }
	void setCallbackYesNoPopup(CallbackYesNoPopup pfnc) { yesno_popup_cb = pfnc; }
	CallbackYesNoPopup getCallbackYesNoPopup() const { return yesno_popup_cb; }
	void setCallbackCommitErrorPopup(CallbackCommitErrorPopup pfnc) { commit_error_popup_cb = pfnc; }
	CallbackCommitErrorPopup getCallbackCommitErrorPopup() const { return commit_error_popup_cb; }
	void setCallbackPasswordPopup(CallbackPasswordPopup pfnc) { password_popup_cb = pfnc; }
	CallbackPasswordPopup getCallbackPasswordPopup() const { return password_popup_cb; }

	void addInfoPopupText( const string& disk, const Text& txt );

	CallbackProgressBar getCallbackProgressBarTheOne() const
	    { return progress_bar_cb ? progress_bar_cb : progress_bar_cb_ycp; }
	CallbackShowInstallInfo getCallbackShowInstallInfoTheOne() const
	    { return install_info_cb ? install_info_cb : install_info_cb_ycp; }
	CallbackInfoPopup getCallbackInfoPopupTheOne() const
	    { return info_popup_cb ? info_popup_cb : info_popup_cb_ycp; }
	CallbackYesNoPopup getCallbackYesNoPopupTheOne() const
	    { return yesno_popup_cb ? yesno_popup_cb : yesno_popup_cb_ycp; }
	CallbackCommitErrorPopup getCallbackCommitErrorPopupTheOne() const
	    { return commit_error_popup_cb ? commit_error_popup_cb : commit_error_popup_cb_ycp; }
	CallbackPasswordPopup getCallbackPasswordPopupTheOne() const
	    { return password_popup_cb ? password_popup_cb : password_popup_cb_ycp; }

	void progressBarCb(const string& id, unsigned cur, unsigned max) const;
	void showInfoCb(const Text& info);
	void infoPopupCb(const Text& info) const;
	bool yesnoPopupCb(const Text& info) const;
	bool commitErrorPopupCb(int error, const Text& last_action, const string& extended_message) const;
	bool passwordPopupCb(const string& device, int attempts, string& password) const;

// iterators over container
    protected:
	// protected typedefs for iterators over container
	template< class Pred >
	    struct ConstContainerPI { typedef ContainerIter<Pred, CCIter> type; };
	typedef CheckFnc<const Container> CheckFncCont;
	typedef CheckerIterator< CheckFncCont, ConstContainerPI<CheckFncCont>::type,
	                         CCIter, Container > ConstContPIterator;
	template< class Pred >
	    struct ContainerPI { typedef ContainerIter<Pred, CIter> type; };
	template< class Pred >
	    struct ContainerI
		{ typedef ContainerDerIter<Pred, typename ContainerPI<Pred>::type,
					   Container> type; };
	typedef CheckerIterator< CheckFncCont, ContainerPI<CheckFncCont>::type,
	                         CIter, Container > ContPIterator;
	typedef DerefIterator<ContPIterator,Container> ContIterator;
	typedef IterPair<ContIterator> CPair;

    public:
	// public typedefs for iterators over containers
	template< class Pred >
	    struct ConstContainerI
		{ typedef ContainerDerIter<Pred, typename ConstContainerPI<Pred>::type,
					   const Container> type; };
	template< class Pred >
	    struct ContCondIPair { typedef MakeCondIterPair<Pred,
	                           typename ConstContainerI<Pred>::type> type;};
	typedef DerefIterator<ConstContPIterator,const Container> ConstContIterator;
	typedef IterPair<ConstContIterator> ConstContPair;

	// public member functions for iterators over containers
	ConstContPair contPair( bool (* CheckFnc)( const Container& )=NULL ) const
	    {
	    return( ConstContPair( contBegin( CheckFnc ), contEnd( CheckFnc ) ));
	    }
	ConstContIterator contBegin( bool (* CheckFnc)( const Container& )=NULL ) const
	    {
	    return( ConstContIterator( ConstContPIterator( cont.begin(), cont.end(), CheckFnc )) );
	    }
	ConstContIterator contEnd( bool (* CheckFnc)( const Container& )=NULL ) const
	    {
	    return( ConstContIterator( ConstContPIterator( cont.begin(), cont.end(), CheckFnc, true )) );
	    }
	template< class Pred > typename ContCondIPair<Pred>::type contCondPair( const Pred& p ) const
	    {
	    return( typename ContCondIPair<Pred>::type( contCondBegin( p ), contCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstContainerI<Pred>::type contCondBegin( const Pred& p ) const
	    {
	    return( typename ConstContainerI<Pred>::type( typename ConstContainerPI<Pred>::type( cont.begin(), cont.end(), p )) );
	    }
	template< class Pred > typename ConstContainerI<Pred>::type contCondEnd( const Pred& p ) const
	    {
	    return( typename ConstContainerI<Pred>::type( typename ConstContainerPI<Pred>::type( cont.begin(), cont.end(), p, true )) );
	    }
    protected:
	// protected member functions for iterators over containers
	CPair cPair( bool (* CheckFnc)( const Container& )=NULL )
	    {
	    return( CPair( cBegin(CheckFnc), cEnd(CheckFnc) ));
	    }
	ContIterator cBegin( bool (* CheckFnc)( const Container& )=NULL )
	    {
	    return( ContIterator( ContPIterator( cont.begin(), cont.end(), CheckFnc )) );
	    }
	ContIterator cEnd( bool (* CheckFnc)( const Container& )=NULL )
	    {
	    return( ContIterator( ContPIterator( cont.begin(), cont.end(), CheckFnc, true )) );
	    }

// iterators over disks
    protected:
	// protected typedefs for iterators over disks
	typedef CastCheckIterator<CCIter, storage::DISK, const Disk *> ContainerCDiskIter;
	template< class Pred >
	    struct ConstDiskPI { typedef ContainerIter<Pred, ContainerCDiskIter> type; };
	typedef CastCheckIterator<CIter, storage::DISK, Disk *> ContainerDiskIter;
	template< class Pred >
	    struct DiskPI { typedef ContainerIter<Pred, ContainerDiskIter> type; };
	template< class Pred >
	    struct DiskI { typedef ContainerDerIter<Pred, typename DiskPI<Pred>::type, Disk> type; };
	typedef CheckFnc<const Disk> CheckFncDisk;
	typedef CheckerIterator< CheckFncDisk, ConstDiskPI<CheckFncDisk>::type,
	                         ContainerCDiskIter, Disk > ConstDiskPIterator;
	typedef CheckerIterator< CheckFncDisk, DiskPI<CheckFncDisk>::type,
	                         ContainerDiskIter, Disk > DiskPIterator;
	typedef DerefIterator<DiskPIterator,Disk> DiskIterator;
	typedef IterPair<DiskIterator> DiskPair;

    public:
	// public typedefs for iterators over disks
	typedef DerefIterator<ConstDiskPIterator,const Disk> ConstDiskIterator;
	template< class Pred >
	    struct ConstDiskI
		{ typedef ContainerDerIter<Pred, typename ConstDiskPI<Pred>::type,
					   const Disk> type; };
	template< class Pred >
	    struct DiskCondIPair { typedef MakeCondIterPair<Pred, typename ConstDiskI<Pred>::type> type; };
	typedef IterPair<ConstDiskIterator> ConstDiskPair;

	// public member functions for iterators over disks
	ConstDiskPair diskPair( bool (* CheckFnc)( const Disk& )=NULL ) const
	    {
	    return( ConstDiskPair( diskBegin( CheckFnc ), diskEnd( CheckFnc ) ));
	    }
	ConstDiskIterator diskBegin( bool (* CheckFnc)( const Disk& )=NULL ) const
	    {
	    IterPair<ContainerCDiskIter> p( ContainerCDiskIter( cont.begin(), cont.end() ),
	                                    ContainerCDiskIter( cont.begin(), cont.end(), true ));
	    return( ConstDiskIterator( ConstDiskPIterator( p, CheckFnc )) );
	    }
	ConstDiskIterator diskEnd( bool (* CheckFnc)( const Disk& )=NULL ) const
	    {
	    IterPair<ContainerCDiskIter> p( ContainerCDiskIter( cont.begin(), cont.end() ),
	                                    ContainerCDiskIter( cont.begin(), cont.end(), true ));
	    return( ConstDiskIterator( ConstDiskPIterator( p, CheckFnc, true )) );
	    }
	template< class Pred > typename DiskCondIPair<Pred>::type diskCondPair( const Pred& p ) const
	    {
	    return( typename DiskCondIPair<Pred>::type( diskCondBegin( p ), diskCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstDiskI<Pred>::type diskCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCDiskIter> pair( ContainerCDiskIter( cont.begin(), cont.end() ),
					       ContainerCDiskIter( cont.begin(), cont.end(), true ));
	    return( typename ConstDiskI<Pred>::type( typename ConstDiskPI<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename ConstDiskI<Pred>::type diskCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCDiskIter> pair( ContainerCDiskIter( cont.begin(), cont.end() ),
					       ContainerCDiskIter( cont.begin(), cont.end(), true ));
	    return( typename ConstDiskI<Pred>::type( typename ConstDiskPI<Pred>::type( pair, p, true )) );
	    }
    protected:
	// protected member functions for iterators over disks
	DiskPair dPair( bool (* CheckFnc)( const Disk& )=NULL )
	    {
	    return( DiskPair( dBegin( CheckFnc ), dEnd( CheckFnc ) ));
	    }
	DiskIterator dBegin( bool (* CheckFnc)( const Disk& )=NULL )
	    {
	    IterPair<ContainerDiskIter> p( ContainerDiskIter( cont.begin(), cont.end() ),
	                                   ContainerDiskIter( cont.begin(), cont.end(), true ));
	    return( DiskIterator( DiskPIterator( p, CheckFnc )) );
	    }
	DiskIterator dEnd( bool (* CheckFnc)( const Disk& )=NULL )
	    {
	    IterPair<ContainerDiskIter> p( ContainerDiskIter( cont.begin(), cont.end() ),
	                                   ContainerDiskIter( cont.begin(), cont.end(), true ));
	    return( DiskIterator( DiskPIterator( p, CheckFnc, true )) );
	    }


// iterators over LVM VGs
    protected:
	// protected typedefs for iterators over LVM VGs
	typedef CastCheckIterator<CCIter, storage::LVM, const LvmVg *> ContainerCLvmVgIter;
	template< class Pred >
	    struct ConstLvmVgPI { typedef ContainerIter<Pred, ContainerCLvmVgIter> type; };
	typedef CastCheckIterator<CIter, storage::LVM, LvmVg *> ContainerLvmVgIter;
	template< class Pred >
	    struct LvmVgPI { typedef ContainerIter<Pred, ContainerLvmVgIter> type; };
	template< class Pred >
	    struct LvmVgI { typedef ContainerDerIter<Pred, typename LvmVgPI<Pred>::type, LvmVg> type; };
	typedef CheckFnc<const LvmVg> CheckFncLvmVg;
	typedef CheckerIterator< CheckFncLvmVg, ConstLvmVgPI<CheckFncLvmVg>::type,
	                         ContainerCLvmVgIter, LvmVg > ConstLvmVgPIterator;
	typedef CheckerIterator< CheckFncLvmVg, LvmVgPI<CheckFncLvmVg>::type,
	                         ContainerLvmVgIter, LvmVg > LvmVgPIterator;
	typedef DerefIterator<LvmVgPIterator,LvmVg> LvmVgIterator;
	typedef IterPair<LvmVgIterator> LvmVgPair;

    public:
	// public typedefs for iterators over LVM VGs
	typedef DerefIterator<ConstLvmVgPIterator,const LvmVg> ConstLvmVgIterator;
	template< class Pred >
	    struct ConstLvmVgI
		{ typedef ContainerDerIter<Pred, typename ConstLvmVgPI<Pred>::type,
					   const LvmVg> type; };
	template< class Pred >
	    struct LvmVgCondIPair { typedef MakeCondIterPair<Pred, typename ConstLvmVgI<Pred>::type> type; };
	typedef IterPair<ConstLvmVgIterator> ConstLvmVgPair;

	// public member functions for iterators over LVM VGs
	ConstLvmVgPair lvmVgPair( bool (* CheckFnc)( const LvmVg& )=NULL ) const
	    {
	    return( ConstLvmVgPair( lvmVgBegin( CheckFnc ), lvmVgEnd( CheckFnc ) ));
	    }
	ConstLvmVgIterator lvmVgBegin( bool (* CheckFnc)( const LvmVg& )=NULL ) const
	    {
	    IterPair<ContainerCLvmVgIter> p( ContainerCLvmVgIter( cont.begin(), cont.end() ),
	                                     ContainerCLvmVgIter( cont.begin(), cont.end(), true ));
	    return( ConstLvmVgIterator( ConstLvmVgPIterator( p, CheckFnc )) );
	    }
	ConstLvmVgIterator lvmVgEnd( bool (* CheckFnc)( const LvmVg& )=NULL ) const
	    {
	    IterPair<ContainerCLvmVgIter> p( ContainerCLvmVgIter( cont.begin(), cont.end() ),
	                                     ContainerCLvmVgIter( cont.begin(), cont.end(), true ));
	    return( ConstLvmVgIterator( ConstLvmVgPIterator( p, CheckFnc, true )) );
	    }
	template< class Pred > typename LvmVgCondIPair<Pred>::type lvmVgCondPair( const Pred& p ) const
	    {
	    return( typename LvmVgCondIPair<Pred>::type( lvmVgCondBegin( p ), lvmVgCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstLvmVgI<Pred>::type lvmVgCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCLvmVgIter> pair( ContainerCLvmVgIter( cont.begin(), cont.end() ),
					        ContainerCLvmVgIter( cont.begin(), cont.end(), true ));
	    return( typename ConstLvmVgI<Pred>::type( typename ConstLvmVgPI<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename ConstLvmVgI<Pred>::type lvmVgCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCLvmVgIter> pair( ContainerCLvmVgIter( cont.begin(), cont.end() ),
					        ContainerCLvmVgIter( cont.begin(), cont.end(), true ));
	    return( typename ConstLvmVgI<Pred>::type( typename ConstLvmVgPI<Pred>::type( pair, p, true )) );
	    }
    protected:
	// protected member functions for iterators over LVM VGs
	LvmVgPair lvgPair( bool (* CheckFnc)( const LvmVg& )=NULL )
	    {
	    return( LvmVgPair( lvgBegin( CheckFnc ), lvgEnd( CheckFnc ) ));
	    }
	LvmVgIterator lvgBegin( bool (* CheckFnc)( const LvmVg& )=NULL )
	    {
	    IterPair<ContainerLvmVgIter> p( ContainerLvmVgIter( cont.begin(), cont.end() ),
					    ContainerLvmVgIter( cont.begin(), cont.end(), true ));
	    return( LvmVgIterator( LvmVgPIterator( p, CheckFnc )) );
	    }
	LvmVgIterator lvgEnd( bool (* CheckFnc)( const LvmVg& )=NULL )
	    {
	    IterPair<ContainerLvmVgIter> p( ContainerLvmVgIter( cont.begin(), cont.end() ),
					    ContainerLvmVgIter( cont.begin(), cont.end(), true ));
	    return( LvmVgIterator( LvmVgPIterator( p, CheckFnc, true )) );
	    }

// iterators over DmPart container
    protected:
	// protected typedefs for iterators over DmPart container
	typedef CastCheckFncIterator<CCIter, isDmPart, const DmPartCo *> ContainerCDmPartIter;
	template< class Pred >
	    struct ConstDmPartCoPI { typedef ContainerIter<Pred, ContainerCDmPartIter> type; };
	typedef CastCheckFncIterator<CIter, isDmPart, DmPartCo *> ContainerDmPartIter;
	template< class Pred >
	    struct DmPartCoPI { typedef ContainerIter<Pred, ContainerDmPartIter> type; };
	template< class Pred >
	    struct DmPartCoI { typedef ContainerDerIter<Pred, typename DmPartCoPI<Pred>::type, DmPartCo> type; };
	typedef CheckFnc<const DmPartCo> CheckFncDmPartCo;
	typedef CheckerIterator< CheckFncDmPartCo, ConstDmPartCoPI<CheckFncDmPartCo>::type,
	                         ContainerCDmPartIter, DmPartCo > ConstDmPartCoPIterator;
	typedef CheckerIterator< CheckFncDmPartCo, DmPartCoPI<CheckFncDmPartCo>::type,
	                         ContainerDmPartIter, DmPartCo > DmPartCoPIterator;
	typedef DerefIterator<DmPartCoPIterator,DmPartCo> DmPartCoIterator;
	typedef IterPair<DmPartCoIterator> DmPartCoPair;

    public:
	// public typedefs for iterators over DmPart container
	typedef DerefIterator<ConstDmPartCoPIterator,const DmPartCo> ConstDmPartCoIterator;
	template< class Pred >
	    struct ConstDmPartCoI
		{ typedef ContainerDerIter<Pred, typename ConstDmPartCoPI<Pred>::type,
					   const DmPartCo> type; };
	template< class Pred >
	    struct DmPartCoCondIPair { typedef MakeCondIterPair<Pred, typename ConstDmPartCoI<Pred>::type> type; };
	typedef IterPair<ConstDmPartCoIterator> ConstDmPartCoPair;

	// public member functions for iterators over DmPart container
	ConstDmPartCoPair dmpartCoPair( bool (* CheckFnc)( const DmPartCo& )=NULL ) const
	    {
	    return( ConstDmPartCoPair( dmpartCoBegin( CheckFnc ), dmpartCoEnd( CheckFnc ) ));
	    }
	ConstDmPartCoIterator dmpartCoBegin( bool (* CheckFnc)( const DmPartCo& )=NULL ) const
	    {
	    IterPair<ContainerCDmPartIter> p( ContainerCDmPartIter( cont.begin(), cont.end() ),
	                                      ContainerCDmPartIter( cont.begin(), cont.end(), true ));
	    return( ConstDmPartCoIterator( ConstDmPartCoPIterator( p, CheckFnc )) );
	    }
	ConstDmPartCoIterator dmpartCoEnd( bool (* CheckFnc)( const DmPartCo& )=NULL ) const
	    {
	    IterPair<ContainerCDmPartIter> p( ContainerCDmPartIter( cont.begin(), cont.end() ),
	                                      ContainerCDmPartIter( cont.begin(), cont.end(), true ));
	    return( ConstDmPartCoIterator( ConstDmPartCoPIterator( p, CheckFnc, true )) );
	    }
	template< class Pred > typename DmPartCoCondIPair<Pred>::type dmPartCoCondPair( const Pred& p ) const
	    {
	    return( typename DmPartCoCondIPair<Pred>::type( dmpartCoCondBegin( p ), dmpartCoCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstDmPartCoI<Pred>::type dmpartCoCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCDmPartIter> pair( ContainerCDmPartIter( cont.begin(), cont.end() ),
					         ContainerCDmPartIter( cont.begin(), cont.end(), true ));
	    return( typename ConstDmPartCoI<Pred>::type( typename ConstDmPartCoPI<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename ConstDmPartCoI<Pred>::type dmpartCoCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCDmPartIter> pair( ContainerCDmPartIter( cont.begin(), cont.end() ),
					         ContainerCDmPartIter( cont.begin(), cont.end(), true ));
	    return( typename ConstDmPartCoI<Pred>::type( typename ConstDmPartCoPI<Pred>::type( pair, p, true )) );
	    }
    protected:
	// protected member functions for iterators over DmPart container
	DmPartCoPair dmpCoPair( bool (* CheckFnc)( const DmPartCo& )=NULL )
	    {
	    return( DmPartCoPair( dmpCoBegin( CheckFnc ), dmpCoEnd( CheckFnc ) ));
	    }
	DmPartCoIterator dmpCoBegin( bool (* CheckFnc)( const DmPartCo& )=NULL )
	    {
	    IterPair<ContainerDmPartIter> p( ContainerDmPartIter( cont.begin(), cont.end() ),
					     ContainerDmPartIter( cont.begin(), cont.end(), true ));
	    return( DmPartCoIterator( DmPartCoPIterator( p, CheckFnc )) );
	    }
	DmPartCoIterator dmpCoEnd( bool (* CheckFnc)( const DmPartCo& )=NULL )
	    {
	    IterPair<ContainerDmPartIter> p( ContainerDmPartIter( cont.begin(), cont.end() ),
					     ContainerDmPartIter( cont.begin(), cont.end(), true ));
	    return( DmPartCoIterator( DmPartCoPIterator( p, CheckFnc, true )) );
	    }

// iterators over DMRAID container
    protected:
	// protected typedefs for iterators over DMRAID container
	typedef CastCheckIterator<CCIter, storage::DMRAID, const DmraidCo *> ContainerCDmraidIter;
	template< class Pred >
	    struct ConstDmraidCoPI { typedef ContainerIter<Pred, ContainerCDmraidIter> type; };
	typedef CastCheckIterator<CIter, storage::DMRAID, DmraidCo *> ContainerDmraidIter;
	template< class Pred >
	    struct DmraidCoPI { typedef ContainerIter<Pred, ContainerDmraidIter> type; };
	template< class Pred >
	    struct DmraidCoI { typedef ContainerDerIter<Pred, typename DmraidCoPI<Pred>::type, DmraidCo> type; };
	typedef CheckFnc<const DmraidCo> CheckFncDmraidCo;
	typedef CheckerIterator< CheckFncDmraidCo, ConstDmraidCoPI<CheckFncDmraidCo>::type,
	                         ContainerCDmraidIter, DmraidCo > ConstDmraidCoPIterator;
	typedef CheckerIterator< CheckFncDmraidCo, DmraidCoPI<CheckFncDmraidCo>::type,
	                         ContainerDmraidIter, DmraidCo > DmraidCoPIterator;
	typedef DerefIterator<DmraidCoPIterator,DmraidCo> DmraidCoIterator;
	typedef IterPair<DmraidCoIterator> DmraidCoPair;

    public:
	// public typedefs for iterators over DMRAID container
	typedef DerefIterator<ConstDmraidCoPIterator,const DmraidCo> ConstDmraidCoIterator;
	template< class Pred >
	    struct ConstDmraidCoI
		{ typedef ContainerDerIter<Pred, typename ConstDmraidCoPI<Pred>::type,
					   const DmraidCo> type; };
	template< class Pred >
	    struct DmraidCoCondIPair { typedef MakeCondIterPair<Pred, typename ConstDmraidCoI<Pred>::type> type; };
	typedef IterPair<ConstDmraidCoIterator> ConstDmraidCoPair;

	// public member functions for iterators over DMRAID container
	ConstDmraidCoPair dmraidCoPair( bool (* CheckFnc)( const DmraidCo& )=NULL ) const
	    {
	    return( ConstDmraidCoPair( dmraidCoBegin( CheckFnc ), dmraidCoEnd( CheckFnc ) ));
	    }
	ConstDmraidCoIterator dmraidCoBegin( bool (* CheckFnc)( const DmraidCo& )=NULL ) const
	    {
	    IterPair<ContainerCDmraidIter> p( ContainerCDmraidIter( cont.begin(), cont.end() ),
	                                      ContainerCDmraidIter( cont.begin(), cont.end(), true ));
	    return( ConstDmraidCoIterator( ConstDmraidCoPIterator( p, CheckFnc )) );
	    }
	ConstDmraidCoIterator dmraidCoEnd( bool (* CheckFnc)( const DmraidCo& )=NULL ) const
	    {
	    IterPair<ContainerCDmraidIter> p( ContainerCDmraidIter( cont.begin(), cont.end() ),
	                                      ContainerCDmraidIter( cont.begin(), cont.end(), true ));
	    return( ConstDmraidCoIterator( ConstDmraidCoPIterator( p, CheckFnc, true )) );
	    }
	template< class Pred > typename DmraidCoCondIPair<Pred>::type dmraidCoCondPair( const Pred& p ) const
	    {
	    return( typename DmraidCoCondIPair<Pred>::type( dmraidCoCondBegin( p ), dmraidCoCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstDmraidCoI<Pred>::type dmraidCoCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCDmraidIter> pair( ContainerCDmraidIter( cont.begin(), cont.end() ),
					         ContainerCDmraidIter( cont.begin(), cont.end(), true ));
	    return( typename ConstDmraidCoI<Pred>::type( typename ConstDmraidCoPI<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename ConstDmraidCoI<Pred>::type dmraidCoCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCDmraidIter> pair( ContainerCDmraidIter( cont.begin(), cont.end() ),
					         ContainerCDmraidIter( cont.begin(), cont.end(), true ));
	    return( typename ConstDmraidCoI<Pred>::type( typename ConstDmraidCoPI<Pred>::type( pair, p, true )) );
	    }
    protected:
	// protected member functions for iterators over DMRAID container
	DmraidCoPair dmrCoPair( bool (* CheckFnc)( const DmraidCo& )=NULL )
	    {
	    return( DmraidCoPair( dmrCoBegin( CheckFnc ), dmrCoEnd( CheckFnc ) ));
	    }
	DmraidCoIterator dmrCoBegin( bool (* CheckFnc)( const DmraidCo& )=NULL )
	    {
	    IterPair<ContainerDmraidIter> p( ContainerDmraidIter( cont.begin(), cont.end() ),
					     ContainerDmraidIter( cont.begin(), cont.end(), true ));
	    return( DmraidCoIterator( DmraidCoPIterator( p, CheckFnc )) );
	    }
	DmraidCoIterator dmrCoEnd( bool (* CheckFnc)( const DmraidCo& )=NULL )
	    {
	    IterPair<ContainerDmraidIter> p( ContainerDmraidIter( cont.begin(), cont.end() ),
					     ContainerDmraidIter( cont.begin(), cont.end(), true ));
	    return( DmraidCoIterator( DmraidCoPIterator( p, CheckFnc, true )) );
	    }


// iterators over DMMULTIPATH container
    protected:
	// protected typedefs for iterators over DMMULTIPATH container
	typedef CastCheckIterator<CCIter, storage::DMMULTIPATH, const DmmultipathCo *> ContainerCDmmultipathIter;
	template< class Pred >
	    struct ConstDmmultipathCoPI { typedef ContainerIter<Pred, ContainerCDmmultipathIter> type; };
	typedef CastCheckIterator<CIter, storage::DMMULTIPATH, DmmultipathCo *> ContainerDmmultipathIter;
	template< class Pred >
	    struct DmmultipathCoPI { typedef ContainerIter<Pred, ContainerDmmultipathIter> type; };
	template< class Pred >
	    struct DmmultipathCoI { typedef ContainerDerIter<Pred, typename DmmultipathCoPI<Pred>::type, DmmultipathCo> type; };
	typedef CheckFnc<const DmmultipathCo> CheckFncDmmultipathCo;
	typedef CheckerIterator< CheckFncDmmultipathCo, ConstDmmultipathCoPI<CheckFncDmmultipathCo>::type,
	                         ContainerCDmmultipathIter, DmmultipathCo > ConstDmmultipathCoPIterator;
	typedef CheckerIterator< CheckFncDmmultipathCo, DmmultipathCoPI<CheckFncDmmultipathCo>::type,
	                         ContainerDmmultipathIter, DmmultipathCo > DmmultipathCoPIterator;
	typedef DerefIterator<DmmultipathCoPIterator,DmmultipathCo> DmmultipathCoIterator;
	typedef IterPair<DmmultipathCoIterator> DmmultipathCoPair;

    public:
	// public typedefs for iterators over DMMULTIPATH container
	typedef DerefIterator<ConstDmmultipathCoPIterator,const DmmultipathCo> ConstDmmultipathCoIterator;
	template< class Pred >
	    struct ConstDmmultipathCoI
		{ typedef ContainerDerIter<Pred, typename ConstDmmultipathCoPI<Pred>::type,
					   const DmmultipathCo> type; };
	template< class Pred >
	    struct DmmultipathCoCondIPair { typedef MakeCondIterPair<Pred, typename ConstDmmultipathCoI<Pred>::type> type; };
	typedef IterPair<ConstDmmultipathCoIterator> ConstDmmultipathCoPair;

	// public member functions for iterators over DMMULTIPATH container
	ConstDmmultipathCoPair dmmultipathCoPair( bool (* CheckFnc)( const DmmultipathCo& )=NULL ) const
	    {
	    return( ConstDmmultipathCoPair( dmmultipathCoBegin( CheckFnc ), dmmultipathCoEnd( CheckFnc ) ));
	    }
	ConstDmmultipathCoIterator dmmultipathCoBegin( bool (* CheckFnc)( const DmmultipathCo& )=NULL ) const
	    {
	    IterPair<ContainerCDmmultipathIter> p( ContainerCDmmultipathIter( cont.begin(), cont.end() ),
	                                      ContainerCDmmultipathIter( cont.begin(), cont.end(), true ));
	    return( ConstDmmultipathCoIterator( ConstDmmultipathCoPIterator( p, CheckFnc )) );
	    }
	ConstDmmultipathCoIterator dmmultipathCoEnd( bool (* CheckFnc)( const DmmultipathCo& )=NULL ) const
	    {
	    IterPair<ContainerCDmmultipathIter> p( ContainerCDmmultipathIter( cont.begin(), cont.end() ),
	                                      ContainerCDmmultipathIter( cont.begin(), cont.end(), true ));
	    return( ConstDmmultipathCoIterator( ConstDmmultipathCoPIterator( p, CheckFnc, true )) );
	    }
	template< class Pred > typename DmmultipathCoCondIPair<Pred>::type dmmultipathCoCondPair( const Pred& p ) const
	    {
	    return( typename DmmultipathCoCondIPair<Pred>::type( dmmultipathCoCondBegin( p ), dmmultipathCoCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstDmmultipathCoI<Pred>::type dmmultipathCoCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCDmmultipathIter> pair( ContainerCDmmultipathIter( cont.begin(), cont.end() ),
					         ContainerCDmmultipathIter( cont.begin(), cont.end(), true ));
	    return( typename ConstDmmultipathCoI<Pred>::type( typename ConstDmmultipathCoPI<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename ConstDmmultipathCoI<Pred>::type dmmultipathCoCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCDmmultipathIter> pair( ContainerCDmmultipathIter( cont.begin(), cont.end() ),
					         ContainerCDmmultipathIter( cont.begin(), cont.end(), true ));
	    return( typename ConstDmmultipathCoI<Pred>::type( typename ConstDmmultipathCoPI<Pred>::type( pair, p, true )) );
	    }
    protected:
	// protected member functions for iterators over DMMULTIPATH container
	DmmultipathCoPair dmmCoPair( bool (* CheckFnc)( const DmmultipathCo& )=NULL )
	    {
	    return( DmmultipathCoPair( dmmCoBegin( CheckFnc ), dmmCoEnd( CheckFnc ) ));
	    }
	DmmultipathCoIterator dmmCoBegin( bool (* CheckFnc)( const DmmultipathCo& )=NULL )
	    {
	    IterPair<ContainerDmmultipathIter> p( ContainerDmmultipathIter( cont.begin(), cont.end() ),
					     ContainerDmmultipathIter( cont.begin(), cont.end(), true ));
	    return( DmmultipathCoIterator( DmmultipathCoPIterator( p, CheckFnc )) );
	    }
	DmmultipathCoIterator dmmCoEnd( bool (* CheckFnc)( const DmmultipathCo& )=NULL )
	    {
	    IterPair<ContainerDmmultipathIter> p( ContainerDmmultipathIter( cont.begin(), cont.end() ),
					     ContainerDmmultipathIter( cont.begin(), cont.end(), true ));
	    return( DmmultipathCoIterator( DmmultipathCoPIterator( p, CheckFnc, true )) );
	    }


// iterators over MdPart container
    protected:
	// protected typedefs for iterators over DmPart container
	typedef CastCheckFncIterator<CCIter, isMdPart, const MdPartCo *> ContainerCMdPartIter;
	template< class Pred >
	struct ConstMdPartCoPI { typedef ContainerIter<Pred, ContainerCMdPartIter> type; };
	typedef CastCheckFncIterator<CIter, isMdPart, MdPartCo *> ContainerMdPartIter;
	template< class Pred >
	struct MdPartCoPI { typedef ContainerIter<Pred, ContainerMdPartIter> type; };
	template< class Pred >
	struct MdPartCoI { typedef ContainerDerIter<Pred, typename MdPartCoPI<Pred>::type, MdPartCo> type; };
	typedef CheckFnc<const MdPartCo> CheckFncMdPartCo;
	typedef CheckerIterator< CheckFncMdPartCo, ConstMdPartCoPI<CheckFncMdPartCo>::type,
	ContainerCMdPartIter, MdPartCo > ConstMdPartCoPIterator;
	typedef CheckerIterator< CheckFncMdPartCo, MdPartCoPI<CheckFncMdPartCo>::type,
	ContainerMdPartIter, MdPartCo > MdPartCoPIterator;
	typedef DerefIterator<MdPartCoPIterator,MdPartCo> MdPartCoIterator;
	typedef IterPair<MdPartCoIterator> MdPartCoPair;

    public:
	// public typedefs for iterators over MdPart container
	typedef DerefIterator<ConstMdPartCoPIterator,const MdPartCo> ConstMdPartCoIterator;
	template< class Pred >
	struct ConstMdPartCoI
	{ typedef ContainerDerIter<Pred, typename ConstMdPartCoPI<Pred>::type,
	  const MdPartCo> type; };
	template< class Pred >
	struct MdPartCoCondIPair { typedef MakeCondIterPair<Pred, typename ConstMdPartCoI<Pred>::type> type; };
	typedef IterPair<ConstMdPartCoIterator> ConstMdPartCoPair;

	// public member functions for iterators over MdPart container
	ConstMdPartCoPair mdpartCoPair( bool (* CheckFnc)( const MdPartCo& )=NULL ) const
	{
	  return( ConstMdPartCoPair( mdpartCoBegin( CheckFnc ), mdpartCoEnd( CheckFnc ) ));
	}
	ConstMdPartCoIterator mdpartCoBegin( bool (* CheckFnc)( const MdPartCo& )=NULL ) const
	{
	  IterPair<ContainerCMdPartIter> p( ContainerCMdPartIter( cont.begin(), cont.end() ),
	      ContainerCMdPartIter( cont.begin(), cont.end(), true ));
	  return( ConstMdPartCoIterator( ConstMdPartCoPIterator( p, CheckFnc )) );
	}
	ConstMdPartCoIterator mdpartCoEnd( bool (* CheckFnc)( const MdPartCo& )=NULL ) const
	{
	  IterPair<ContainerCMdPartIter> p( ContainerCMdPartIter( cont.begin(), cont.end() ),
	      ContainerCMdPartIter( cont.begin(), cont.end(), true ));
	  return( ConstMdPartCoIterator( ConstMdPartCoPIterator( p, CheckFnc, true )) );
	}
	template< class Pred > typename MdPartCoCondIPair<Pred>::type mdPartCoCondPair( const Pred& p ) const
	{
	  return( typename MdPartCoCondIPair<Pred>::type( mdpartCoCondBegin( p ), mdpartCoCondEnd( p ) ) );
	}
	template< class Pred > typename ConstMdPartCoI<Pred>::type mdpartCoCondBegin( const Pred& p ) const
	{
	  IterPair<ContainerCMdPartIter> pair( ContainerCMdPartIter( cont.begin(), cont.end() ),
	      ContainerCMdPartIter( cont.begin(), cont.end(), true ));
	  return( typename ConstMdPartCoI<Pred>::type( typename ConstMdPartCoPI<Pred>::type( pair, p )) );
	}
	template< class Pred > typename ConstMdPartCoI<Pred>::type mdpartCoCondEnd( const Pred& p ) const
	{
	  IterPair<ContainerCMdPartIter> pair( ContainerCMdPartIter( cont.begin(), cont.end() ),
	      ContainerCMdPartIter( cont.begin(), cont.end(), true ));
	  return( typename ConstMdPartCoI<Pred>::type( typename ConstMdPartCoPI<Pred>::type( pair, p, true )) );
	}
    protected:
	// protected member functions for iterators over MdPart container
	MdPartCoPair mdpCoPair( bool (* CheckFnc)( const MdPartCo& )=NULL )
	{
	  return( MdPartCoPair( mdpCoBegin( CheckFnc ), mdpCoEnd( CheckFnc ) ));
	}
	MdPartCoIterator mdpCoBegin( bool (* CheckFnc)( const MdPartCo& )=NULL )
	{
	  IterPair<ContainerMdPartIter> p( ContainerMdPartIter( cont.begin(), cont.end() ),
	      ContainerMdPartIter( cont.begin(), cont.end(), true ));
	  return( MdPartCoIterator( MdPartCoPIterator( p, CheckFnc )) );
	}
	MdPartCoIterator mdpCoEnd( bool (* CheckFnc)( const MdPartCo& )=NULL )
	{
	  IterPair<ContainerMdPartIter> p( ContainerMdPartIter( cont.begin(), cont.end() ),
	      ContainerMdPartIter( cont.begin(), cont.end(), true ));
	  return( MdPartCoIterator( MdPartCoPIterator( p, CheckFnc, true )) );
	}



// iterators over volumes
    protected:
	// protected typedefs for iterators over volumes
	typedef ListListIterator<Container::ConstPlainIterator, ConstContIterator> ConstVolInter;
	template< class Pred >
	    struct ConstVolumePI { typedef ContainerIter<Pred, ConstVolInter> type; };
	typedef CheckFnc<const Volume> CheckFncVol;
	typedef CheckerIterator< CheckFncVol, ConstVolumePI<CheckFncVol>::type,
	                         ConstVolInter, Volume > ConstVolPIterator;
	typedef ListListIterator<Container::PlainIterator, ContIterator> VolPart;
	template< class Pred >
	    struct VolumeI { typedef ContainerIter<Pred, VolPart> type; };
	typedef CheckerIterator< CheckFncVol, VolumeI<CheckFncVol>::type,
	                         VolPart, Volume > VolPIterator;
	typedef DerefIterator<VolPIterator,Volume> VolIterator;
	typedef IterPair<VolIterator> VPair;

    public:
	// public typedefs for iterators over volumes
	template< class Pred >
	    struct ConstVolumeI { typedef ContainerDerIter<Pred, typename ConstVolumePI<Pred>::type, const Volume> type; };
	template< class Pred >
	    struct VolCondIPair { typedef MakeCondIterPair<Pred, typename ConstVolumeI<Pred>::type> type;};
	typedef DerefIterator<ConstVolPIterator,const Volume> ConstVolIterator;
	typedef IterPair<ConstVolIterator> ConstVolPair;

	// public member functions for iterators over volumes
	ConstVolPair volPair( bool (* CheckCnt)( const Container& )) const
	    {
	    return( ConstVolPair( volBegin( CheckCnt ), volEnd( CheckCnt ) ));
	    }
	ConstVolPair volPair( bool (* CheckVol)( const Volume& )=NULL,
			      bool (* CheckCnt)( const Container& )=NULL) const
	    {
	    return( ConstVolPair( volBegin( CheckVol, CheckCnt ),
	                          volEnd( CheckVol, CheckCnt ) ));
	    }
	ConstVolIterator volBegin( bool (* CheckCnt)( const Container& )) const
	    {
	    return( volBegin( NULL, CheckCnt ) );
	    }
	ConstVolIterator volBegin( bool (* CheckVol)( const Volume& )=NULL,
	                           bool (* CheckCnt)( const Container& )=NULL) const
	    {
	    IterPair<ConstVolInter> p( (ConstVolInter( contPair( CheckCnt ))),
				       (ConstVolInter( contPair( CheckCnt ), true )));
	    return( ConstVolIterator( ConstVolPIterator(p, CheckVol )));
	    }
	ConstVolIterator volEnd( bool (* CheckCnt)( const Container& )) const
	    {
	    return( volEnd( NULL, CheckCnt ) );
	    }
	ConstVolIterator volEnd( bool (* CheckVol)( const Volume& )=NULL,
	                         bool (* CheckCnt)( const Container& )=NULL) const
	    {
	    IterPair<ConstVolInter> p( (ConstVolInter( contPair( CheckCnt ))),
				       (ConstVolInter( contPair( CheckCnt ), true )));
	    return( ConstVolIterator( ConstVolPIterator(p, CheckVol, true )));
	    }
	template< class Pred > typename VolCondIPair<Pred>::type volCondPair( const Pred& p ) const
	    {
	    return( typename VolCondIPair<Pred>::type( volCondBegin( p ), volCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstVolumeI<Pred>::type volCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstVolInter> pair( (ConstVolInter( contPair())),
					  (ConstVolInter( contPair(), true )));
	    return( typename ConstVolumeI<Pred>::type( typename ConstVolumePI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstVolumeI<Pred>::type volCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstVolInter> pair( (ConstVolInter( contPair())),
					  (ConstVolInter( contPair(), true )));
	    return( typename ConstVolumeI<Pred>::type( typename ConstVolumePI<Pred>::type(pair, p, true )) );
	    }

    protected:
	// protected member functions for iterators over volumes
	VPair vPair( bool (* CheckCnt)( const Container& ))
	    {
	    return( VPair( vBegin( CheckCnt ), vEnd( CheckCnt ) ));
	    }
	VPair vPair( bool (* CheckVol)( const Volume& )=NULL,
		     bool (* CheckCnt)( const Container& )=NULL)
	    {
	    return( VPair( vBegin( CheckVol, CheckCnt ),
			   vEnd( CheckVol, CheckCnt ) ));
	    }
	VolIterator vBegin( bool (* CheckCnt)( const Container& ))
	    {
	    return( vBegin( NULL, CheckCnt ) );
	    }
	VolIterator vBegin( bool (* CheckVol)( const Volume& )=NULL,
			    bool (* CheckCnt)( const Container& )=NULL)
	    {
	    IterPair<VolPart> p( (VolPart( cPair( CheckCnt ))),
				 (VolPart( cPair( CheckCnt ), true )));
	    return( VolIterator( VolPIterator( p, CheckVol )));
	    }
	VolIterator vEnd( bool (* CheckCnt)( const Container& ))
	    {
	    return( vEnd( NULL, CheckCnt ) );
	    }
	VolIterator vEnd( bool (* CheckVol)( const Volume& )=NULL,
			  bool (* CheckCnt)( const Container& )=NULL)
	    {
	    IterPair<VolPart> p( (VolPart( cPair( CheckCnt ))),
				 (VolPart( cPair( CheckCnt ), true )));
	    return( VolIterator( VolPIterator( p, CheckVol, true )));
	    }

// iterators over partitions
    protected:
	// protected typedefs for iterators over partitions
	typedef ListListIterator<Container::ConstPlainIterator, ConstDiskIterator> ConstPartInter;
	typedef CastIterator<ConstPartInter, Partition *> ConstPartInter2;
	template< class Pred >
	    struct ConstPartitionPI { typedef ContainerIter<Pred, ConstPartInter2> type; };
	typedef CheckFnc<const Partition> CheckFncPartition;
	typedef CheckerIterator< CheckFncPartition, ConstPartitionPI<CheckFncPartition>::type,
	                         ConstPartInter2, Partition > ConstPartPIterator;
    public:
	// public typedefs for iterators over partitions
	template< class Pred >
	    struct ConstPartitionI
		{ typedef ContainerDerIter<Pred, typename ConstPartitionPI<Pred>::type,
		                           const Partition> type; };
	template< class Pred >
	    struct PartCondIPair
		{ typedef MakeCondIterPair<Pred, typename ConstPartitionI<Pred>::type> type;};
	typedef DerefIterator<ConstPartPIterator, const Partition> ConstPartIterator;
	typedef IterPair<ConstPartIterator> ConstPartPair;

	// public member functions for iterators over partitions
	ConstPartPair partPair( bool (* CheckCnt)( const Disk& )) const
	    {
	    return( ConstPartPair( partBegin( CheckCnt ), partEnd( CheckCnt ) ));
	    }
	ConstPartPair partPair( bool (* CheckPart)( const Partition& )=NULL,
				bool (* CheckCnt)( const Disk& )=NULL) const
	    {
	    return( ConstPartPair( partBegin( CheckPart, CheckCnt ),
	                           partEnd( CheckPart, CheckCnt ) ));
	    }
	ConstPartIterator partBegin( bool (* CheckDisk)( const Disk& )) const
	    {
	    return( partBegin( NULL, CheckDisk ) );
	    }
	ConstPartIterator partBegin( bool (* CheckPart)( const Partition& )=NULL,
	                             bool (* CheckDisk)( const Disk& )=NULL) const
	    {
	    IterPair<ConstPartInter2> p( (ConstPartInter(diskPair( CheckDisk ))),
					 (ConstPartInter(diskPair( CheckDisk ), true )));
	    return( ConstPartIterator( ConstPartPIterator(p, CheckPart )));
	    }
	ConstPartIterator partEnd( bool (* CheckDisk)( const Disk& )) const
	    {
	    return( partEnd( NULL, CheckDisk ) );
	    }
	ConstPartIterator partEnd( bool (* CheckPart)( const Partition& )=NULL,
	                           bool (* CheckDisk)( const Disk& )=NULL) const
	    {
	    IterPair<ConstPartInter2> p( (ConstPartInter(diskPair( CheckDisk ))),
					 (ConstPartInter(diskPair( CheckDisk ), true )));
	    return( ConstPartIterator( ConstPartPIterator(p, CheckPart, true )));
	    }
	template< class Pred > typename PartCondIPair<Pred>::type partCondPair( const Pred& p ) const
	    {
	    return( typename PartCondIPair<Pred>::type( partCondBegin( p ), partCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstPartitionI<Pred>::type partCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstPartInter2> pair( (ConstPartInter( diskPair())),
					    (ConstPartInter( diskPair(), true )));
	    return( typename ConstPartitionI<Pred>::type( typename ConstPartitionPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstPartitionI<Pred>::type partCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstPartInter2> pair( (ConstPartInter( diskPair())),
					    (ConstPartInter( diskPair(), true )));
	    return( typename ConstPartitionI<Pred>::type( typename ConstPartitionPI<Pred>::type(pair, p, true )) );
	    }

// iterators over LVM LVs
    protected:
	// protected typedefs for iterators over LVM LVs
	typedef ListListIterator<Container::ConstPlainIterator, ConstLvmVgIterator> ConstLvmLvInter;
	typedef CastIterator<ConstLvmLvInter, LvmLv *> ConstLvmLvInter2;
	template< class Pred >
	    struct ConstLvmLvPI { typedef ContainerIter<Pred, ConstLvmLvInter2> type; };
	typedef CheckFnc<const LvmLv> CheckFncLvmLv;
	typedef CheckerIterator< CheckFncLvmLv, ConstLvmLvPI<CheckFncLvmLv>::type,
	                         ConstLvmLvInter2, LvmLv > ConstLvmLvPIterator;
    public:
	// public typedefs for iterators over LVM LVs
	template< class Pred >
	    struct ConstLvmLvI
		{ typedef ContainerDerIter<Pred, typename ConstLvmLvPI<Pred>::type,
		                           const LvmLv> type; };
	template< class Pred >
	    struct LvmLvCondIPair
		{ typedef MakeCondIterPair<Pred, typename ConstLvmLvI<Pred>::type> type;};
	typedef DerefIterator<ConstLvmLvPIterator, const LvmLv> ConstLvmLvIterator;
	typedef IterPair<ConstLvmLvIterator> ConstLvmLvPair;

	// public member functions for iterators over LVM LVs
	ConstLvmLvPair lvmLvPair( bool (* CheckLvmVg)( const LvmVg& )) const
	    {
	    return( ConstLvmLvPair( lvmLvBegin( CheckLvmVg ), lvmLvEnd( CheckLvmVg ) ));
	    }
	ConstLvmLvPair lvmLvPair( bool (* CheckLvmLv)( const LvmLv& )=NULL,
				  bool (* CheckLvmVg)( const LvmVg& )=NULL) const
	    {
	    return( ConstLvmLvPair( lvmLvBegin( CheckLvmLv, CheckLvmVg ),
	                            lvmLvEnd( CheckLvmLv, CheckLvmVg ) ));
	    }
	ConstLvmLvIterator lvmLvBegin( bool (* CheckLvmVg)( const LvmVg& )) const
	    {
	    return( lvmLvBegin( NULL, CheckLvmVg ) );
	    }
	ConstLvmLvIterator lvmLvBegin( bool (* CheckLvmLv)( const LvmLv& )=NULL,
	                               bool (* CheckLvmVg)( const LvmVg& )=NULL) const
	    {
	    IterPair<ConstLvmLvInter2> p( (ConstLvmLvInter(lvmVgPair( CheckLvmVg ))),
					  (ConstLvmLvInter(lvmVgPair( CheckLvmVg ), true )));
	    return( ConstLvmLvIterator( ConstLvmLvPIterator(p, CheckLvmLv )));
	    }
	ConstLvmLvIterator lvmLvEnd( bool (* CheckLvmVg)( const LvmVg& )) const
	    {
	    return( lvmLvEnd( NULL, CheckLvmVg ) );
	    }
	ConstLvmLvIterator lvmLvEnd( bool (* CheckLvmLv)( const LvmLv& )=NULL,
	                             bool (* CheckLvmVg)( const LvmVg& )=NULL) const
	    {
	    IterPair<ConstLvmLvInter2> p( (ConstLvmLvInter(lvmVgPair( CheckLvmVg ))),
					  (ConstLvmLvInter(lvmVgPair( CheckLvmVg ), true )));
	    return( ConstLvmLvIterator( ConstLvmLvPIterator(p, CheckLvmLv, true )));
	    }
	template< class Pred > typename LvmLvCondIPair<Pred>::type lvmLvCondPair( const Pred& p ) const
	    {
	    return( typename LvmLvCondIPair<Pred>::type( lvmLvCondBegin( p ), lvmLvCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstLvmLvI<Pred>::type lvmLvCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstLvmLvInter2> pair( (ConstLvmLvInter( lvmVgPair())),
					     (ConstLvmLvInter( lvmVgPair(), true )));
	    return( typename ConstLvmLvI<Pred>::type( typename ConstLvmLvPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstLvmLvI<Pred>::type lvmLvCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstLvmLvInter2> pair( (ConstLvmLvInter( lvmVgPair())),
					     (ConstLvmLvInter( lvmVgPair(), true )));
	    return( typename ConstLvmLvI<Pred>::type( typename ConstLvmLvPI<Pred>::type(pair, p, true )) );
	    }

// iterators over software raid devices
    protected:
	// protected typedefs for iterators over software raid devices
	typedef CastIterator<ConstVolInter, Md *> ConstMdInter;
	template< class Pred >
	    struct ConstMdPI { typedef ContainerIter<Pred,
	                                             ConstMdInter> type; };
	typedef CheckFnc<const Md> CheckFncMd;
	typedef CheckerIterator< CheckFncMd, ConstMdPI<CheckFncMd>::type,
	                         ConstMdInter, Md > ConstMdPIterator;
    public:
	// public typedefs for iterators over software raid devices
	template< class Pred >
	    struct ConstMdI
		{ typedef ContainerDerIter<Pred, typename ConstMdPI<Pred>::type,
		                           const Md> type; };
	template< class Pred >
	    struct MdCondIPair
		{ typedef MakeCondIterPair<Pred, typename ConstMdI<Pred>::type> type;};
	typedef DerefIterator<ConstMdPIterator, const Md> ConstMdIterator;
	typedef IterPair<ConstMdIterator> ConstMdPair;

	// public member functions for iterators over software raid devices
	ConstMdPair mdPair( bool (* CheckMd)( const Md& )=NULL ) const
	    {
	    return( ConstMdPair( mdBegin( CheckMd ), mdEnd( CheckMd ) ));
	    }
	ConstMdIterator mdBegin( bool (* CheckMd)( const Md& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isMd ) );
	    ConstVolInter e( contPair( isMd ), true );
	    IterPair<ConstMdInter> p( (ConstMdInter(b)), (ConstMdInter(e)) );
	    return( ConstMdIterator( ConstMdPIterator(p, CheckMd )));
	    }
	ConstMdIterator mdEnd( bool (* CheckMd)( const Md& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isMd ) );
	    ConstVolInter e( contPair( isMd ), true );
	    IterPair<ConstMdInter> p( (ConstMdInter(b)), (ConstMdInter(e)) );
	    return( ConstMdIterator( ConstMdPIterator(p, CheckMd, true )));
	    }
	template< class Pred > typename MdCondIPair<Pred>::type mdCondPair( const Pred& p ) const
	    {
	    return( typename MdCondIPair<Pred>::type( mdCondBegin( p ), mdCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstMdI<Pred>::type mdCondBegin( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isMd ) );
	    ConstVolInter e( contPair( isMd ), true );
	    IterPair<ConstMdInter> pair( (ConstMdInter(b)), (ConstMdInter(e)) );
	    return( typename ConstMdI<Pred>::type( typename ConstMdPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstMdI<Pred>::type mdCondEnd( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isMd ) );
	    ConstVolInter e( contPair( isMd ), true );
	    IterPair<ConstMdInter> pair( (ConstMdInter(b)), (ConstMdInter(e)) );
	    return( typename ConstMdI<Pred>::type( typename ConstMdPI<Pred>::type(pair, p, true )) );
	    }

///////// iterators over software raid devices - Partitions
    protected:
	// protected typedefs for iterators over software raid devices
	typedef CastIterator<ConstVolInter, MdPart *> ConstMdPartInter;
	template< class Pred >
	struct ConstMdPartPI { typedef ContainerIter<Pred,
	  ConstMdPartInter> type; };
	typedef CheckFnc<const MdPart> CheckFncMdPart;
	typedef CheckerIterator< CheckFncMdPart, ConstMdPartPI<CheckFncMdPart>::type,
	ConstMdPartInter, MdPart > ConstMdPartPIterator;
    public:
	// public typedefs for iterators over software raid devices
	template< class Pred >
	struct ConstMdPartI
	{ typedef ContainerDerIter<Pred, typename ConstMdPartPI<Pred>::type,
	  const MdPart> type; };
	template< class Pred >
	struct MdPartCondIPair
	{ typedef MakeCondIterPair<Pred, typename ConstMdPartI<Pred>::type> type;};
	typedef DerefIterator<ConstMdPartPIterator, const MdPart> ConstMdPartIterator;
	typedef IterPair<ConstMdPartIterator> ConstMdPartPair;


	// public member functions for iterators over software raid devices
	ConstMdPartPair mdPartPair( bool (* CheckMdPart)( const MdPart& )=NULL ) const
	{
	  return( ConstMdPartPair( mdPartBegin( CheckMdPart ), mdPartEnd( CheckMdPart ) ));
	}

	ConstMdPartIterator mdPartBegin( bool (* CheckMdPart)( const MdPart& )=NULL ) const
	{
	  ConstVolInter b( contPair( isMdPart ) );
	  ConstVolInter e( contPair( isMdPart ), true );
	  IterPair<ConstMdPartInter> p( (ConstMdPartInter(b)), (ConstMdPartInter(e)) );
	  return( ConstMdPartIterator( ConstMdPartPIterator(p, CheckMdPart )));
	}
	ConstMdPartIterator mdPartEnd( bool (* CheckMdPart)( const MdPart& )=NULL ) const
	{
	  ConstVolInter b( contPair( isMdPart ) );
	  ConstVolInter e( contPair( isMdPart ), true );
	  IterPair<ConstMdPartInter> p( (ConstMdPartInter(b)), (ConstMdPartInter(e)) );
	  return( ConstMdPartIterator( ConstMdPartPIterator(p, CheckMdPart, true )));
	}
	template< class Pred > typename MdPartCondIPair<Pred>::type mdPartCondPair( const Pred& p ) const
	{
	  return( typename MdPartCondIPair<Pred>::type( mdPartCondBegin( p ), mdPartCondEnd( p ) ) );
	}
	template< class Pred > typename ConstMdPartI<Pred>::type mdPartCondBegin( const Pred& p ) const
	{
	  ConstVolInter b( contPair( isMdPart ) );
	  ConstVolInter e( contPair( isMdPart ), true );
	  IterPair<ConstMdPartInter> pair( (ConstMdPartInter(b)), (ConstMdPartInter(e)) );
	  return( typename ConstMdPartI<Pred>::type( typename ConstMdPartPI<Pred>::type(pair, p) ) );
	}
	template< class Pred > typename ConstMdPartI<Pred>::type mdPartCondEnd( const Pred& p ) const
	{
	  ConstVolInter b( contPair( isMdPart ) );
	  ConstVolInter e( contPair( isMdPart ), true );
	  IterPair<ConstMdPartInter> pair( (ConstMdPartInter(b)), (ConstMdPartInter(e)) );
	  return( typename ConstMdPartI<Pred>::type( typename ConstMdPartPI<Pred>::type(pair, p, true )) );
	}


// iterators over file based loop devices
    protected:
	// protected typedefs for iterators over file based loop devices
	typedef CastIterator<ConstVolInter, Loop *> ConstLoopInter;
	template< class Pred >
	    struct ConstLoopPI { typedef ContainerIter<Pred,
	                                             ConstLoopInter> type; };
	typedef CheckFnc<const Loop> CheckFncLoop;
	typedef CheckerIterator< CheckFncLoop, ConstLoopPI<CheckFncLoop>::type,
	                         ConstLoopInter, Loop > ConstLoopPIterator;
    public:
	// public typedefs for iterators over file based loop devices
	template< class Pred >
	    struct ConstLoopI
		{ typedef ContainerDerIter<Pred, typename ConstLoopPI<Pred>::type,
		                           const Loop> type; };
	template< class Pred >
	    struct LoopCondIPair
		{ typedef MakeCondIterPair<Pred, typename ConstLoopI<Pred>::type> type;};
	typedef DerefIterator<ConstLoopPIterator, const Loop> ConstLoopIterator;
	typedef IterPair<ConstLoopIterator> ConstLoopPair;

	// public member functions for iterators over file based loop devices
	ConstLoopPair loopPair( bool (* CheckLoop)( const Loop& )=NULL ) const
	    {
	    return( ConstLoopPair( loopBegin( CheckLoop ), loopEnd( CheckLoop ) ));
	    }
	ConstLoopIterator loopBegin( bool (* CheckLoop)( const Loop& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isLoop ) );
	    ConstVolInter e( contPair( isLoop ), true );
	    IterPair<ConstLoopInter> p( (ConstLoopInter(b)), (ConstLoopInter(e)) );
	    return( ConstLoopIterator( ConstLoopPIterator(p, CheckLoop )));
	    }
	ConstLoopIterator loopEnd( bool (* CheckLoop)( const Loop& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isLoop ) );
	    ConstVolInter e( contPair( isLoop ), true );
	    IterPair<ConstLoopInter> p( (ConstLoopInter(b)), (ConstLoopInter(e)) );
	    return( ConstLoopIterator( ConstLoopPIterator(p, CheckLoop, true )));
	    }
	template< class Pred > typename LoopCondIPair<Pred>::type loopCondPair( const Pred& p ) const
	    {
	    return( typename LoopCondIPair<Pred>::type( loopCondBegin( p ), loopCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstLoopI<Pred>::type loopCondBegin( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isLoop ) );
	    ConstVolInter e( contPair( isLoop ), true );
	    IterPair<ConstLoopInter> pair( (ConstLoopInter(b)), (ConstLoopInter(e)) );
	    return( typename ConstLoopI<Pred>::type( typename ConstLoopPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstLoopI<Pred>::type loopCondEnd( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isLoop ) );
	    ConstVolInter e( contPair( isLoop ), true );
	    IterPair<ConstLoopInter> pair( (ConstLoopInter(b)), (ConstLoopInter(e)) );
	    return( typename ConstLoopI<Pred>::type( typename ConstLoopPI<Pred>::type(pair, p, true )) );
	    }

// iterators over btrfs volumes
    protected:
	// protected typedefs for iterators over btrfs volumes
	typedef CastIterator<ConstVolInter, Btrfs *> ConstBtrfsInter;
	template< class Pred >
	    struct ConstBtrfsPI { typedef ContainerIter<Pred,
	                                             ConstBtrfsInter> type; };
	typedef CheckFnc<const Btrfs> CheckFncBtrfs;
	typedef CheckerIterator< CheckFncBtrfs, ConstBtrfsPI<CheckFncBtrfs>::type,
	                         ConstBtrfsInter, Btrfs > ConstBtrfsPIterator;
    public:
	// public typedefs for iterators over btrfs volumes
	template< class Pred >
	    struct ConstBtrfsI
		{ typedef ContainerDerIter<Pred, typename ConstBtrfsPI<Pred>::type,
		                           const Btrfs> type; };
	template< class Pred >
	    struct BtrfsCondIPair
		{ typedef MakeCondIterPair<Pred, typename ConstBtrfsI<Pred>::type> type;};
	typedef DerefIterator<ConstBtrfsPIterator, const Btrfs> ConstBtrfsIterator;
	typedef IterPair<ConstBtrfsIterator> ConstBtrfsPair;

	// public member functions for iterators over btrfs volumes
	ConstBtrfsPair btrfsPair( bool (* CheckBtrfs)( const Btrfs& )=NULL ) const
	    {
	    return( ConstBtrfsPair( btrfsBegin( CheckBtrfs ), btrfsEnd( CheckBtrfs ) ));
	    }
	ConstBtrfsIterator btrfsBegin( bool (* CheckBtrfs)( const Btrfs& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isBtrfs ) );
	    ConstVolInter e( contPair( isBtrfs ), true );
	    IterPair<ConstBtrfsInter> p( (ConstBtrfsInter(b)), (ConstBtrfsInter(e)) );
	    return( ConstBtrfsIterator( ConstBtrfsPIterator(p, CheckBtrfs )));
	    }
	ConstBtrfsIterator btrfsEnd( bool (* CheckBtrfs)( const Btrfs& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isBtrfs ) );
	    ConstVolInter e( contPair( isBtrfs ), true );
	    IterPair<ConstBtrfsInter> p( (ConstBtrfsInter(b)), (ConstBtrfsInter(e)) );
	    return( ConstBtrfsIterator( ConstBtrfsPIterator(p, CheckBtrfs, true )));
	    }
	template< class Pred > typename BtrfsCondIPair<Pred>::type btrfsCondPair( const Pred& p ) const
	    {
	    return( typename BtrfsCondIPair<Pred>::type( btrfsCondBegin( p ), btrfsCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstBtrfsI<Pred>::type btrfsCondBegin( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isBtrfs ) );
	    ConstVolInter e( contPair( isBtrfs ), true );
	    IterPair<ConstBtrfsInter> pair( (ConstBtrfsInter(b)), (ConstBtrfsInter(e)) );
	    return( typename ConstBtrfsI<Pred>::type( typename ConstBtrfsPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstBtrfsI<Pred>::type btrfsCondEnd( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isBtrfs ) );
	    ConstVolInter e( contPair( isBtrfs ), true );
	    IterPair<ConstBtrfsInter> pair( (ConstBtrfsInter(b)), (ConstBtrfsInter(e)) );
	    return( typename ConstBtrfsI<Pred>::type( typename ConstBtrfsPI<Pred>::type(pair, p, true )) );
	    }

// iterators over tmpfs volumes
    protected:
	// protected typedefs for iterators over tmpfs volumes
	typedef CastIterator<ConstVolInter, Tmpfs *> ConstTmpfsInter;
	template< class Pred >
	    struct ConstTmpfsPI { typedef ContainerIter<Pred,
	                                             ConstTmpfsInter> type; };
	typedef CheckFnc<const Tmpfs> CheckFncTmpfs;
	typedef CheckerIterator< CheckFncTmpfs, ConstTmpfsPI<CheckFncTmpfs>::type,
	                         ConstTmpfsInter, Tmpfs > ConstTmpfsPIterator;
    public:
	// public typedefs for iterators over tmpfs volumes
	template< class Pred >
	    struct ConstTmpfsI
		{ typedef ContainerDerIter<Pred, typename ConstTmpfsPI<Pred>::type,
		                           const Tmpfs> type; };
	template< class Pred >
	    struct TmpfsCondIPair
		{ typedef MakeCondIterPair<Pred, typename ConstTmpfsI<Pred>::type> type;};
	typedef DerefIterator<ConstTmpfsPIterator, const Tmpfs> ConstTmpfsIterator;
	typedef IterPair<ConstTmpfsIterator> ConstTmpfsPair;

	// public member functions for iterators over tmpfs volumes
	ConstTmpfsPair tmpfsPair( bool (* CheckTmpfs)( const Tmpfs& )=NULL ) const
	    {
	    return( ConstTmpfsPair( tmpfsBegin( CheckTmpfs ), tmpfsEnd( CheckTmpfs ) ));
	    }
	ConstTmpfsIterator tmpfsBegin( bool (* CheckTmpfs)( const Tmpfs& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isTmpfs ) );
	    ConstVolInter e( contPair( isTmpfs ), true );
	    IterPair<ConstTmpfsInter> p( (ConstTmpfsInter(b)), (ConstTmpfsInter(e)) );
	    return( ConstTmpfsIterator( ConstTmpfsPIterator(p, CheckTmpfs )));
	    }
	ConstTmpfsIterator tmpfsEnd( bool (* CheckTmpfs)( const Tmpfs& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isTmpfs ) );
	    ConstVolInter e( contPair( isTmpfs ), true );
	    IterPair<ConstTmpfsInter> p( (ConstTmpfsInter(b)), (ConstTmpfsInter(e)) );
	    return( ConstTmpfsIterator( ConstTmpfsPIterator(p, CheckTmpfs, true )));
	    }
	template< class Pred > typename TmpfsCondIPair<Pred>::type tmpfsCondPair( const Pred& p ) const
	    {
	    return( typename TmpfsCondIPair<Pred>::type( tmpfsCondBegin( p ), tmpfsCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstTmpfsI<Pred>::type tmpfsCondBegin( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isTmpfs ) );
	    ConstVolInter e( contPair( isTmpfs ), true );
	    IterPair<ConstTmpfsInter> pair( (ConstTmpfsInter(b)), (ConstTmpfsInter(e)) );
	    return( typename ConstTmpfsI<Pred>::type( typename ConstTmpfsPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstTmpfsI<Pred>::type tmpfsCondEnd( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isTmpfs ) );
	    ConstVolInter e( contPair( isTmpfs ), true );
	    IterPair<ConstTmpfsInter> pair( (ConstTmpfsInter(b)), (ConstTmpfsInter(e)) );
	    return( typename ConstTmpfsI<Pred>::type( typename ConstTmpfsPI<Pred>::type(pair, p, true )) );
	    }

// iterators over nfs devices
    protected:
	// protected typedefs for iterators over nfs devices
	typedef CastIterator<ConstVolInter, Nfs *> ConstNfsInter;
	template< class Pred >
	    struct ConstNfsPI { typedef ContainerIter<Pred,
	                                             ConstNfsInter> type; };
	typedef CheckFnc<const Nfs> CheckFncNfs;
	typedef CheckerIterator< CheckFncNfs, ConstNfsPI<CheckFncNfs>::type,
	                         ConstNfsInter, Nfs > ConstNfsPIterator;
    public:
	// public typedefs for iterators over nfs devices
	template< class Pred >
	    struct ConstNfsI
		{ typedef ContainerDerIter<Pred, typename ConstNfsPI<Pred>::type,
		                           const Nfs> type; };
	template< class Pred >
	    struct NfsCondIPair
		{ typedef MakeCondIterPair<Pred, typename ConstNfsI<Pred>::type> type;};
	typedef DerefIterator<ConstNfsPIterator, const Nfs> ConstNfsIterator;
	typedef IterPair<ConstNfsIterator> ConstNfsPair;

	// public member functions for iterators over nfs devices
	ConstNfsPair nfsPair( bool (* CheckNfs)( const Nfs& )=NULL ) const
	    {
	    return( ConstNfsPair( nfsBegin( CheckNfs ), nfsEnd( CheckNfs ) ));
	    }
	ConstNfsIterator nfsBegin( bool (* CheckNfs)( const Nfs& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isNfs ) );
	    ConstVolInter e( contPair( isNfs ), true );
	    IterPair<ConstNfsInter> p( (ConstNfsInter(b)), (ConstNfsInter(e)) );
	    return( ConstNfsIterator( ConstNfsPIterator(p, CheckNfs )));
	    }
	ConstNfsIterator nfsEnd( bool (* CheckNfs)( const Nfs& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isNfs ) );
	    ConstVolInter e( contPair( isNfs ), true );
	    IterPair<ConstNfsInter> p( (ConstNfsInter(b)), (ConstNfsInter(e)) );
	    return( ConstNfsIterator( ConstNfsPIterator(p, CheckNfs, true )));
	    }
	template< class Pred > typename NfsCondIPair<Pred>::type nfsCondPair( const Pred& p ) const
	    {
	    return( typename NfsCondIPair<Pred>::type( nfsCondBegin( p ), nfsCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstNfsI<Pred>::type nfsCondBegin( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isNfs ) );
	    ConstVolInter e( contPair( isNfs ), true );
	    IterPair<ConstNfsInter> pair( (ConstNfsInter(b)), (ConstNfsInter(e)) );
	    return( typename ConstNfsI<Pred>::type( typename ConstNfsPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstNfsI<Pred>::type nfsCondEnd( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isNfs ) );
	    ConstVolInter e( contPair( isNfs ), true );
	    IterPair<ConstNfsInter> pair( (ConstNfsInter(b)), (ConstNfsInter(e)) );
	    return( typename ConstNfsI<Pred>::type( typename ConstNfsPI<Pred>::type(pair, p, true )) );
	    }

// iterators over device mapper devices
    protected:
	// protected typedefs for iterators over device mapper devices
	typedef CastIterator<ConstVolInter, Dm *> ConstDmInter;
	template< class Pred >
	    struct ConstDmPI { typedef ContainerIter<Pred,
	                                             ConstDmInter> type; };
	typedef CheckFnc<const Dm> CheckFncDm;
	typedef CheckerIterator< CheckFncDm, ConstDmPI<CheckFncDm>::type,
	                         ConstDmInter, Dm > ConstDmPIterator;
    public:
	// public typedefs for iterators over device mapper devices
	template< class Pred >
	    struct ConstDmI
		{ typedef ContainerDerIter<Pred, typename ConstDmPI<Pred>::type,
		                           const Dm> type; };
	template< class Pred >
	    struct DmCondIPair
		{ typedef MakeCondIterPair<Pred, typename ConstDmI<Pred>::type> type;};
	typedef DerefIterator<ConstDmPIterator, const Dm> ConstDmIterator;
	typedef IterPair<ConstDmIterator> ConstDmPair;

	// public member functions for iterators over device mapper devices
	ConstDmPair dmPair( bool (* CheckDm)( const Dm& )=NULL ) const
	    {
	    return( ConstDmPair( dmBegin( CheckDm ), dmEnd( CheckDm ) ));
	    }
	ConstDmIterator dmBegin( bool (* CheckDm)( const Dm& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isDm ) );
	    ConstVolInter e( contPair( isDm ), true );
	    IterPair<ConstDmInter> p( (ConstDmInter(b)), (ConstDmInter(e)) );
	    return( ConstDmIterator( ConstDmPIterator(p, CheckDm )));
	    }
	ConstDmIterator dmEnd( bool (* CheckDm)( const Dm& )=NULL ) const
	    {
	    ConstVolInter b( contPair( isDm ) );
	    ConstVolInter e( contPair( isDm ), true );
	    IterPair<ConstDmInter> p( (ConstDmInter(b)), (ConstDmInter(e)) );
	    return( ConstDmIterator( ConstDmPIterator(p, CheckDm, true )));
	    }
	template< class Pred > typename DmCondIPair<Pred>::type dmCondPair( const Pred& p ) const
	    {
	    return( typename DmCondIPair<Pred>::type( dmCondBegin( p ), dmCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstDmI<Pred>::type dmCondBegin( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isDm ) );
	    ConstVolInter e( contPair( isDm ), true );
	    IterPair<ConstDmInter> pair( (ConstDmInter(b)), (ConstDmInter(e)) );
	    return( typename ConstDmI<Pred>::type( typename ConstDmPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstDmI<Pred>::type dmCondEnd( const Pred& p ) const
	    {
	    ConstVolInter b( contPair( isDm ) );
	    ConstVolInter e( contPair( isDm ), true );
	    IterPair<ConstDmInter> pair( (ConstDmInter(b)), (ConstDmInter(e)) );
	    return( typename ConstDmI<Pred>::type( typename ConstDmPI<Pred>::type(pair, p, true )) );
	    }

// iterators over dmraid devices
    protected:
	// protected typedefs for iterators over dmraid devices
	typedef ListListIterator<Container::ConstPlainIterator, ConstDmraidCoIterator> ConstDmraidInter;
	typedef CastIterator<ConstDmraidInter, Dmraid *> ConstDmraidInter2;
	template< class Pred >
	    struct ConstDmraidPI { typedef ContainerIter<Pred, ConstDmraidInter2> type; };
	typedef CheckFnc<const Dmraid> CheckFncDmraid;
	typedef CheckerIterator< CheckFncDmraid, ConstDmraidPI<CheckFncDmraid>::type,
	                         ConstDmraidInter2, Dmraid > ConstDmraidPIterator;
    public:
	// public typedefs for iterators over dmraid volumes
	template< class Pred >
	    struct ConstDmraidI
		{ typedef ContainerDerIter<Pred, typename ConstDmraidPI<Pred>::type,
		                           const Dmraid> type; };
	template< class Pred >
	    struct DmraidCondIPair
		{ typedef MakeCondIterPair<Pred, typename ConstDmraidI<Pred>::type> type;};
	typedef DerefIterator<ConstDmraidPIterator, const Dmraid> ConstDmraidIterator;
	typedef IterPair<ConstDmraidIterator> ConstDmraidPair;

	// public member functions for iterators over dmraid volumes
	ConstDmraidPair dmrPair( bool (* CheckDmraidCo)( const DmraidCo& )) const
	    {
	    return( ConstDmraidPair( dmrBegin( CheckDmraidCo ), dmrEnd( CheckDmraidCo ) ));
	    }
	ConstDmraidPair dmrPair( bool (* CheckDmraid)( const Dmraid& )=NULL,
				 bool (* CheckDmraidCo)( const DmraidCo& )=NULL) const
	    {
	    return( ConstDmraidPair( dmrBegin( CheckDmraid, CheckDmraidCo ),
				     dmrEnd( CheckDmraid, CheckDmraidCo ) ));
	    }
	ConstDmraidIterator dmrBegin( bool (* CheckDmraidCo)( const DmraidCo& )) const
	    {
	    return( dmrBegin( NULL, CheckDmraidCo ) );
	    }
	ConstDmraidIterator dmrBegin( bool (* CheckDmraid)( const Dmraid& )=NULL,
				      bool (* CheckDmraidCo)( const DmraidCo& )=NULL) const
	    {
	    IterPair<ConstDmraidInter2> p( (ConstDmraidInter(dmraidCoPair( CheckDmraidCo ))),
					   (ConstDmraidInter(dmraidCoPair( CheckDmraidCo ), true )));
	    return( ConstDmraidIterator( ConstDmraidPIterator(p, CheckDmraid )));
	    }
	ConstDmraidIterator dmrEnd( bool (* CheckDmraidCo)( const DmraidCo& )) const
	    {
	    return( dmrEnd( NULL, CheckDmraidCo ) );
	    }
	ConstDmraidIterator dmrEnd( bool (* CheckDmraid)( const Dmraid& )=NULL,
				     bool (* CheckDmraidCo)( const DmraidCo& )=NULL) const
	    {
	    IterPair<ConstDmraidInter2> p( (ConstDmraidInter(dmraidCoPair( CheckDmraidCo ))),
					   (ConstDmraidInter(dmraidCoPair( CheckDmraidCo ), true )));
	    return( ConstDmraidIterator( ConstDmraidPIterator(p, CheckDmraid, true )));
	    }
	template< class Pred > typename DmraidCondIPair<Pred>::type dmrCondPair( const Pred& p ) const
	    {
	    return( typename DmraidCondIPair<Pred>::type( dmrCondBegin( p ), dmrCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstDmraidI<Pred>::type dmrCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstDmraidInter2> pair( (ConstDmraidInter( dmraidCoPair())),
					      (ConstDmraidInter( dmraidCoPair(), true )));
	    return( typename ConstDmraidI<Pred>::type( typename ConstDmraidPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstDmraidI<Pred>::type dmrCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstDmraidInter2> pair( (ConstDmraidInter( dmrCoPair())),
					      (ConstDmraidInter( dmrCoPair(), true )));
	    return( typename ConstDmraidI<Pred>::type( typename ConstDmraidPI<Pred>::type(pair, p, true )) );
	    }


// iterators over dmmultipath devices
    protected:
	// protected typedefs for iterators over dmmultipath devices
	typedef ListListIterator<Container::ConstPlainIterator, ConstDmmultipathCoIterator> ConstDmmultipathInter;
	typedef CastIterator<ConstDmmultipathInter, Dmmultipath *> ConstDmmultipathInter2;
	template< class Pred >
	    struct ConstDmmultipathPI { typedef ContainerIter<Pred, ConstDmmultipathInter2> type; };
	typedef CheckFnc<const Dmmultipath> CheckFncDmmultipath;
	typedef CheckerIterator< CheckFncDmmultipath, ConstDmmultipathPI<CheckFncDmmultipath>::type,
	                         ConstDmmultipathInter2, Dmmultipath > ConstDmmultipathPIterator;
    public:
	// public typedefs for iterators over dmmultipath volumes
	template< class Pred >
	    struct ConstDmmultipathI
		{ typedef ContainerDerIter<Pred, typename ConstDmmultipathPI<Pred>::type,
		                           const Dmmultipath> type; };
	template< class Pred >
	    struct DmmultipathCondIPair
		{ typedef MakeCondIterPair<Pred, typename ConstDmmultipathI<Pred>::type> type; };
	typedef DerefIterator<ConstDmmultipathPIterator, const Dmmultipath> ConstDmmultipathIterator;
	typedef IterPair<ConstDmmultipathIterator> ConstDmmultipathPair;

	// public member functions for iterators over dmmultipath volumes
	ConstDmmultipathPair dmmPair( bool (* CheckDmmultipathCo)( const DmmultipathCo& )) const
	    {
	    return( ConstDmmultipathPair( dmmBegin( CheckDmmultipathCo ), dmmEnd( CheckDmmultipathCo ) ));
	    }
	ConstDmmultipathPair dmmPair( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL,
				 bool (* CheckDmmultipathCo)( const DmmultipathCo& )=NULL) const
	    {
	    return( ConstDmmultipathPair( dmmBegin( CheckDmmultipath, CheckDmmultipathCo ),
				     dmmEnd( CheckDmmultipath, CheckDmmultipathCo ) ));
	    }
	ConstDmmultipathIterator dmmBegin( bool (* CheckDmmultipathCo)( const DmmultipathCo& )) const
	    {
	    return( dmmBegin( NULL, CheckDmmultipathCo ) );
	    }
	ConstDmmultipathIterator dmmBegin( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL,
				      bool (* CheckDmmultipathCo)( const DmmultipathCo& )=NULL) const
	    {
	    IterPair<ConstDmmultipathInter2> p( (ConstDmmultipathInter(dmmultipathCoPair( CheckDmmultipathCo ))),
					   (ConstDmmultipathInter(dmmultipathCoPair( CheckDmmultipathCo ), true )));
	    return( ConstDmmultipathIterator( ConstDmmultipathPIterator(p, CheckDmmultipath )));
	    }
	ConstDmmultipathIterator dmmEnd( bool (* CheckDmmultipathCo)( const DmmultipathCo& )) const
	    {
	    return( dmmEnd( NULL, CheckDmmultipathCo ) );
	    }
	ConstDmmultipathIterator dmmEnd( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL,
				     bool (* CheckDmmultipathCo)( const DmmultipathCo& )=NULL) const
	    {
	    IterPair<ConstDmmultipathInter2> p( (ConstDmmultipathInter(dmmultipathCoPair( CheckDmmultipathCo ))),
					   (ConstDmmultipathInter(dmmultipathCoPair( CheckDmmultipathCo ), true )));
	    return( ConstDmmultipathIterator( ConstDmmultipathPIterator(p, CheckDmmultipath, true )));
	    }
	template< class Pred > typename DmmultipathCondIPair<Pred>::type dmmCondPair( const Pred& p ) const
	    {
	    return( typename DmmultipathCondIPair<Pred>::type( dmmCondBegin( p ), dmmCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstDmmultipathI<Pred>::type dmmCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstDmmultipathInter2> pair( (ConstDmmultipathInter( dmmultipathCoPair())),
					      (ConstDmmultipathInter( dmmultipathCoPair(), true )));
	    return( typename ConstDmmultipathI<Pred>::type( typename ConstDmmultipathPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstDmmultipathI<Pred>::type dmmCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstDmmultipathInter2> pair( (ConstDmmultipathInter( dmmCoPair())),
					      (ConstDmmultipathInter( dmmCoPair(), true )));
	    return( typename ConstDmmultipathI<Pred>::type( typename ConstDmmultipathPI<Pred>::type(pair, p, true )) );
	    }


    protected:
	// protected internal member functions
	void initialize();
	void logSystemInfo() const;
	void detectDisks(SystemInfo& systeminfo);
	void autodetectDisks(SystemInfo& systeminfo);
	void detectMds(SystemInfo& systeminfo);
	void detectBtrfs(SystemInfo& systeminfo);
	void detectMdParts(SystemInfo& systeminfo);
	bool discoverMdPVols();
	void detectLoops(SystemInfo& systeminfo);
	void detectNfs(const EtcFstab& fstab, SystemInfo& systeminfo);
	void detectTmpfs(const EtcFstab& fstab, SystemInfo& systeminfo);
	void detectLvmVgs(SystemInfo& systeminfo);
	void detectDmraid(SystemInfo& systeminfo);
	void detectDmmultipath(SystemInfo& systeminfo);
	void detectDm(SystemInfo& systeminfo, bool only_crypt);
	void initDisk( list<DiskData>& dl, SystemInfo& systeminfo);
	void detectFsData(const VolIterator& begin, const VolIterator& end, SystemInfo& systeminfo);
	int resizeVolume(const string& device, unsigned long long newSizeK,
			 bool ignore_fs);
	int resizePartition( const string& device, unsigned long sizeCyl,
	                     bool ignore_fs );
	void addToList(Container* e);
	DiskIterator findDisk( const string& disk );
	DiskIterator findDiskId( const string& id );
	DiskIterator findDiskPath( const string& path );
	LvmVgIterator findLvmVg( const string& name );
	DmraidCoIterator findDmraidCo( const string& name );
	DmmultipathCoIterator findDmmultipathCo( const string& name );
	DmPartCoIterator findDmPartCo( const string& name );

	MdPartCoIterator findMdPartCo( const string& name );

	bool findVolume( const string& device, ContIterator& c,
	                 VolIterator& v, bool no_btrfs=false  );
	bool findVolume( const string& device, ConstContIterator& c,
	                 ConstVolIterator& v  );
	bool findVolume( const string& device, VolIterator& v,
	                 bool also_del=false, bool no_btrfs=false );
	bool findVolume( const string& device, ConstVolIterator& v,
	                 bool also_del=false, bool no_btrfs=false );
	bool findContainer( const string& device, ContIterator& c );
	bool findContainer( const string& device, ConstContIterator& c );

	Device* findDevice(const string& dev, bool no_btrfs=false);

	void checkPwdBuf( const string& device );

	bool haveMd( MdCo*& md );
	list<unsigned> getMdPartMdNums() const;
	bool haveDm(DmCo*& dm);
	bool haveNfs( NfsCo*& co );
	bool haveLoop( LoopCo*& loop );
	bool haveBtrfs( BtrfsCo*& co );
	bool haveTmpfs( TmpfsCo*& co );
	int removeContainer( Container* val );
	void logContainersAndVolumes(const string& Dir) const;

	int commitPair( CPair& p, bool (* fnc)( const Container& ) );
	void sortCommitLists(storage::CommitStage stage, list<const Container*>& co,
			     list<const Volume*>& vl, list<commitAction>& todo) const;
	bool ignoreError(int error, list<commitAction>::const_iterator ca) const;
	string backupStates() const;
	void detectObjects();
	void deleteBackups();

	void setCachedFreeInfo(const string& device, bool resize_cached, const ResizeInfo& resize_info,
			       bool content_cached, const ContentInfo& content_info);
	bool getCachedFreeInfo(	const string& device, bool get_resize, ResizeInfo& resize_info,
				bool get_content, ContentInfo& content_info) const;
	void logFreeInfo(const string& Dir) const;
	void readFreeInfo(const string& file);

	void logArchInfo(const string& Dir) const;
	void readArchInfo(const string& file);

	list<commitAction> getCommitActions() const;

	// protected internal member variables
	const Environment env;
	Lock lock;
	bool cache;
	bool initialized;
	bool recursiveRemove;
	bool zeroNewPartitions;
	PartAlign partAlignment;
	MountByType defaultMountBy;
	FsType defaultFs;
	bool detectMounted;
	bool root_mounted;
	string tempdir;
	string rootprefix;
	unsigned hald_pid;

	ArchInfo archinfo;

	CCont cont;
	EtcFstab *fstab;
	EtcMdadm* mdadm;

	ImsmDriver imsm_driver;

	CallbackProgressBar progress_bar_cb;
	CallbackShowInstallInfo install_info_cb;
	CallbackInfoPopup info_popup_cb;
	CallbackYesNoPopup yesno_popup_cb;
	CallbackCommitErrorPopup commit_error_popup_cb;
	CallbackPasswordPopup password_popup_cb;

	friend std::ostream& operator<<(std::ostream& s, const Storage& v);
	friend std::ostream& operator<<(std::ostream& s, Storage& v);

	map<string, list<UsedBy>> danglingUsedBy;

	unsigned max_log_num;
	Text lastAction;
	string extendedError;
	std::map<string,CCont> backups;
	map<string, FreeInfo> free_infos;
	std::map<string,string> pwdBuf;
	std::list<std::pair<string, Text>> infoPopupTxts;
    };

}

#endif
