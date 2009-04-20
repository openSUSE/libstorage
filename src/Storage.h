#ifndef STORAGE_H
#define STORAGE_H

#include <ostream>
#include <list>
#include <map>

#include "y2storage/StorageInterface.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/Container.h"
#include "y2storage/Volume.h"
#include "y2storage/Disk.h"
#include "y2storage/Partition.h"
#include "y2storage/LvmVg.h"
#include "y2storage/LvmLv.h"
#include "y2storage/DmraidCo.h"
#include "y2storage/Dmraid.h"
#include "y2storage/DmmultipathCo.h"
#include "y2storage/Dmmultipath.h"
#include "y2storage/MdCo.h"
#include "y2storage/Md.h"
#include "y2storage/DmCo.h"
#include "y2storage/LoopCo.h"
#include "y2storage/Loop.h"
#include "y2storage/NfsCo.h"
#include "y2storage/Nfs.h"
#include "y2storage/FilterIterator.h"
#include "y2storage/DerefIterator.h"
#include "y2storage/ListListIterator.h"
#include "y2storage/IterPair.h"
#include "y2storage/Lock.h"

namespace storage
{
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
class CastCheckIterator : public CheckType<Value>,
                          public FilterIterator< CheckType<Value>, Iter >
    {
    typedef FilterIterator<CheckType<Value>, Iter> _bclass;
    public:
	typedef CastResult value_type;
	typedef CastResult& reference;
	typedef CastResult* pointer;

	CastCheckIterator() : _bclass() {}
	CastCheckIterator( const Iter& b, const Iter& e, bool atend=false) :
	    _bclass( b, e, *this, atend ) {}
	CastCheckIterator( const IterPair<Iter>& pair, bool atend=false) :
	    _bclass( pair, *this, atend ) {}
	CastCheckIterator( const CastCheckIterator& i) { *this=i;}
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
class CastCheckFncIterator : public CheckByFnc<FncP>,
                            public FilterIterator< CheckByFnc<FncP>, Iter >
    {
    typedef FilterIterator<CheckByFnc<FncP>, Iter> _bclass;
    public:
	typedef CastResult value_type;
	typedef CastResult& reference;
	typedef CastResult* pointer;

	CastCheckFncIterator() : _bclass() {}
	CastCheckFncIterator( const Iter& b, const Iter& e, bool atend=false) :
	    _bclass( b, e, *this, atend ) {}
	CastCheckFncIterator( const IterPair<Iter>& pair, bool atend=false) :
	    _bclass( pair, *this, atend ) {}
	CastCheckFncIterator( const CastCheckFncIterator& i) { *this=i;}
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

/**
 * \brief Main class to access libstorage functionality.
 *
 * This is the main class with that one can get access to the
 * functionality provided by libstorage.
 * It contains a list of container objects.
 *
 * All modifying member functions of the storage library will
 * go through Storage class. This is the central place where
 * things like readonly access, locking, testmode, inst-sys etc.
 * are handled. It has the additional advantage the the complete
 * class hierarchy below Storage could be changed without affecting
 * the user interface of libstorage.
 */

class EtcFstab;
class DiskData;

class Storage : public storage::StorageInterface
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
	struct FreeInfo
	    {
	    unsigned long long resize_free;
	    unsigned long long df_free;
	    unsigned long long used;
	    bool win;
	    bool efi;
	    bool rok;
	    FreeInfo() { resize_free=df_free=used=0; efi=win=rok=false; }
	    FreeInfo( unsigned long long df,
		      unsigned long long resize,
		      unsigned long long usd, bool w=false, bool e=false,
		      bool r=true )
		      { resize_free=resize; df_free=df; used=usd; win=w; 
		        efi=e, rok=r; }
	    };

    public:
	static bool notDeleted( const Container&d ) { return( !d.deleted() ); };
	static bool isDmPart( const Container&d )
	    { return d.type() == storage::DMRAID || d.type() == storage::DMMULTIPATH; }

	static void initDefaultLogger ();

	Storage( bool ronly=false, bool testmode=false, bool autodetect=true );
	bool test() const { return( testmode ); }
	bool instsys() const { return( inst_sys ); }
	void setCacheChanges( bool val=true ) { cache = val; }
	bool isCacheChanges() const { return( cache ); }
	void assertInit() { if( !initialized ) initialize(); }
	void rescanEverything();
	int checkCache();
	const string& tDir() const { return( testdir ); }
	const string& root() const { return( rootprefix ); }
	string prependRoot(const string& mp) const;
	const string& tmpDir() const;
	bool efiBoot() const { return efiboot; }
	static const string& arch() { return( proc_arch ); }
	static bool isPPCMac() { return( is_ppc_mac ); }
	static bool isPPCPegasos() { return( is_ppc_pegasos ); }
	EtcFstab* getFstab() { return fstab; }
	void handleLogFile(const string& name) const;
	static bool testFilesEqual( const string& n1, const string& n2 );
	void printInfo(std::ostream& str) const;
	void logCo(const Container* c) const;
	void logProcData(const string& str = "") const;
	storage::UsedByType usedBy( const string& dev );
	bool usedBy( const string& dev, storage::usedBy& ub );
	bool clearUsedBy(const string& dev);
	bool setUsedBy(const string& dev, storage::UsedByType ub_type,
		       const string& ub_name);
	void fetchDanglingUsedBy(const string& dev, storage::usedBy& uby);
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
	void rootMounted();
	bool isRootMounted() const { return( root_mounted ); }
	string findNormalDevice( const string& device );
	bool findVolume( const string& device, Volume const* &vol );
	bool findDm( const string& device, const Dm*& dm );
	bool findDmUsing( const string& device, const Dm*& dm );
	bool removeDm( const string& device );

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
	int getDmInfo( deque<storage::DmInfo>& plist );
	int getNfsInfo( deque<storage::NfsInfo>& plist );
	int getLoopInfo( deque<storage::LoopInfo>& plist );
	int getDmraidInfo( const string& name,
	                   deque<storage::DmraidInfo>& plist );
	int getDmmultipathInfo( const string& name,
				deque<storage::DmmultipathInfo>& plist );
	int getContVolInfo( const string& dev, ContVolInfo& info);

	bool getFsCapabilities( storage::FsType fstype,
	                        storage::FsCapabilities& fscapabilities) const;
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
	int freeCylindersAfterPartition(const string& device, unsigned long& freeCyls);
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
	int getUnusedPartitionSlots(const string& disk, list<PartitionSlotInfo>& slots);
	int destroyPartitionTable( const string& disk, const string& label );
	int initializeDisk( const string& disk, bool value );
	string defaultDiskLabel() const;
	string defaultDiskLabelSize( unsigned long long size_k ) const;
	unsigned long long maxSizeLabelK( const string& label ) const;

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
	int resizeVolume( const string& device, unsigned long long newSizeMb );
	int resizeVolumeNoFs( const string& device, unsigned long long newSizeMb );
	int forgetResizeVolume( const string& device );
	void setRecursiveRemoval( bool val=true );
	bool getRecursiveRemoval() const { return recursiveRemove; }
	void setZeroNewPartitions( bool val=true );
	bool getZeroNewPartitions() const { return zeroNewPartitions; }
	void setDefaultMountBy (MountByType mby = MOUNTBY_DEVICE);
	MountByType getDefaultMountBy() const { return defaultMountBy; }
	void setDetectMountedVolumes( bool val=true );
	bool getDetectMountedVolumes() const { return detectMounted; }
	void setEfiBoot(bool val);
	bool getEfiBoot() const { return efiboot; }
	void setRootPrefix( const string& root );
	string getRootPrefix() const { return rootprefix; }
	int removeVolume( const string& device );
	int removeUsing( const string& device, const storage::usedBy& uby );
	bool checkDeviceMounted( const string& device, string& mp );
	bool umountDevice( const string& device );
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
	bool readFstab( const string& dir, deque<storage::VolumeInfo>& infos);
	bool getFreeInfo( const string& device, unsigned long long& resize_free,
	                  unsigned long long& df_free,
	                  unsigned long long& used, bool& win, bool& efi,
			  bool use_cache );
	static unsigned long long getDfSize(const string& mp);
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
			 unsigned long long sizeM, unsigned stripe,
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

	int nextFreeMd(int &nr, string &device);
	int createMd( const string& name, storage::MdType rtype,
		      const deque<string>& devs );
	int createMdAny( storage::MdType rtype, const deque<string>& devs,
			 string& device );
	int removeMd( const string& name, bool destroySb=true );
	int extendMd( const string& name, const string& dev );
	int shrinkMd( const string& name, const string& dev );
	int changeMdType( const string& name, storage::MdType rtype );
	int changeMdChunk( const string& name, unsigned long chunk );
	int changeMdParity( const string& name, storage::MdParity ptype );
	int checkMd( const string& name );
	int getMdStateInfo(const string& name, MdStateInfo& info);
	int computeMdSize(MdType md_type, list<string> devices,
			  unsigned long long& sizeK);

	int addNfsDevice( const string& nfsDev, const string& opts,
	                  unsigned long long sizeK, const string& mp );
	int checkNfsDevice( const string& nfsDev, const string& opts,
	                    unsigned long long& sizeK );

	int createFileLoop( const string& lname, bool reuseExisting,
			    unsigned long long sizeK, const string& mp,
			    const string& pwd, string& device );
	int modifyFileLoop( const string& device, const string& lname, 
	                    bool reuseExisting, unsigned long long sizeK );
	int removeFileLoop( const string& lname, bool removeFile );

	int removeDmraid( const string& name );

	void getCommitInfos(list<CommitInfo>& infos) const;
	const string& getLastAction() const { return lastAction; }
	const string& getExtendedErrorMessage() const { return extendedError; }
	void eraseFreeInfo( const string& device );

	static void waitForDevice();
	static int waitForDevice(const string& device);

	static int zeroDevice(const string& device, unsigned long long sizeK, bool random = false,
			      unsigned long long beginK = 200, unsigned long long endK = 10);

	void getDiskList( bool (* CheckFnc)( const Disk& ),
	                  std::list<Disk*>& dl );
	void changeDeviceName( const string& old, const string& nw );

        int commit();
	void handleHald( bool stop );
	void activateHld( bool val=true );
	void activateMultipath( bool val=true );
	void removeDmTableTo( const Volume& vol );
	void removeDmTableTo( const string& device );
	bool removeDmTable( const string& table );
	bool removeDmMapsTo( const string& dev );
	bool checkDmMapsTo( const string& dev );
	void updateDmEmptyPeMap();
	void dumpObjectList();

	void setCallbackProgressBar( storage::CallbackProgressBar pfnc )
	    { progress_bar_cb=pfnc; }
	storage::CallbackProgressBar getCallbackProgressBar() const
	    { return progress_bar_cb; }
	void setCallbackShowInstallInfo( storage::CallbackShowInstallInfo pfnc )
	    { install_info_cb=pfnc; }
	storage::CallbackShowInstallInfo getCallbackShowInstallInfo() const
	    { return install_info_cb; }
	void setCallbackInfoPopup( storage::CallbackInfoPopup pfnc )
	    { info_popup_cb=pfnc; }
	storage::CallbackInfoPopup getCallbackInfoPopup() const
	    { return info_popup_cb; }
	void setCallbackYesNoPopup( storage::CallbackYesNoPopup pfnc )
	    { yesno_popup_cb=pfnc; }
	storage::CallbackYesNoPopup getCallbackYesNoPopup() const
	    { return yesno_popup_cb; }
	void addInfoPopupText( const string& disk, const string txt );

	static void setCallbackProgressBarYcp( storage::CallbackProgressBar pfnc )
	    { progress_bar_cb_ycp=pfnc; }
	static storage::CallbackProgressBar getCallbackProgressBarYcp()
	    { return progress_bar_cb_ycp; }
	static void setCallbackShowInstallInfoYcp( storage::CallbackShowInstallInfo pfnc )
	    { install_info_cb_ycp=pfnc; }
	static storage::CallbackShowInstallInfo getCallbackShowInstallInfoYcp()
	    { return install_info_cb_ycp; }
	static void setCallbackInfoPopupYcp( storage::CallbackInfoPopup pfnc )
	    { info_popup_cb_ycp=pfnc; }
	static storage::CallbackInfoPopup getCallbackInfoPopupYcp()
	    { return info_popup_cb_ycp; }
	static void setCallbackYesNoPopupYcp( storage::CallbackYesNoPopup pfnc )
	    { yesno_popup_cb_ycp=pfnc; }
	static storage::CallbackYesNoPopup getCallbackYesNoPopupYcp()
	    { return yesno_popup_cb_ycp; }

	storage::CallbackProgressBar getCallbackProgressBarTheOne() const
	    { return progress_bar_cb ? progress_bar_cb : progress_bar_cb_ycp; }
	storage::CallbackShowInstallInfo getCallbackShowInstallInfoTheOne() const
	    { return install_info_cb ? install_info_cb : install_info_cb_ycp; }
	storage::CallbackInfoPopup getCallbackInfoPopupTheOne() const
	    { return info_popup_cb ? info_popup_cb : info_popup_cb_ycp; }
	storage::CallbackYesNoPopup getCallbackYesNoPopupTheOne() const
	    { return yesno_popup_cb ? yesno_popup_cb : yesno_popup_cb_ycp; }

	void progressBarCb( const string& id, unsigned cur, unsigned max );
	void showInfoCb( const string& info );
	void infoPopupCb( const string& info );
	bool yesnoPopupCb( const string& info );

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
	void detectDisks( ProcPart& ppart );
	void autodetectDisks( ProcPart& ppart );
	void detectMultipath();
	void detectMds();
	void detectLoops( ProcPart& ppart );
	void detectNfs( ProcMounts& mounts );
	void detectLvmVgs();
	void detectDmraid( ProcPart& ppart );
	void detectDmmultipath( ProcPart& ppart );
	void detectDm( ProcPart& ppart );
	void initDisk( DiskData& data, ProcPart& pp );
	void detectFsData( const VolIterator& begin, const VolIterator& end,
	                   ProcMounts& mounts );
	void detectFsDataTestMode( const string& file,
	                           const VolIterator& begin,
				   const VolIterator& end );
	int resizeVolume( const string& device, unsigned long long newSizeMb,
	                  bool ignore_fs );
	int resizePartition( const string& device, unsigned long sizeCyl,
	                     bool ignore_fs );
	static void detectArch();
	void addToList(Container* e);
	DiskIterator findDisk( const string& disk );
	DiskIterator findDiskId( const string& id );
	DiskIterator findDiskPath( const string& path );
	LvmVgIterator findLvmVg( const string& name );
	DmraidCoIterator findDmraidCo( const string& name );
	DmmultipathCoIterator findDmmultipathCo( const string& name );
	DmPartCoIterator findDmPartCo( const string& name );
	bool findVolume( const string& device, ContIterator& c,
	                 VolIterator& v  );
	bool findVolume( const string& device, VolIterator& v,
	                 bool also_del=false );
	bool findContainer( const string& device, ContIterator& c );

	bool haveMd( MdCo*& md );
	bool haveNfs( NfsCo*& co );
	bool haveLoop( LoopCo*& loop );
	int removeContainer( Container* val );
	void logContainersAndVolumes(const string& Dir) const;
	void logVolumes(const string& Dir) const;
	int commitPair( CPair& p, bool (* fnc)( const Container& ) );
	void sortCommitLists(storage::CommitStage stage, list<const Container*>& co,
			     list<const Volume*>& vl, list<commitAction>& todo) const;
	bool ignoreError(list<commitAction>::const_iterator i, const list<commitAction>& al) const;
	string backupStates() const;
	void detectObjects();
	void deleteClist( CCont& co );
	void deleteBackups();
	void setFreeInfo( const string& device, unsigned long long df_free,
			  unsigned long long resize_free,
			  unsigned long long used, bool win, bool efi,
			  bool resize_ok );
	bool getFreeInf( const string& device, unsigned long long& df_free,
			 unsigned long long& resize_free,
			 unsigned long long& used, bool& win, bool& efi,
			 bool& resize_ok );

	// protected internal member variables
	Lock lock;
	bool readonly;
	bool testmode;
	bool inst_sys;
	bool cache;
	bool initialized;
	bool autodetect;
	bool recursiveRemove;
	bool zeroNewPartitions;
	MountByType defaultMountBy;
	bool detectMounted;
	bool root_mounted;
	string testdir;
	string tempdir;
	string rootprefix;
	string logdir;
	unsigned hald_pid;
	bool efiboot;
	static string proc_arch;
	static bool is_ppc_mac;
	static bool is_ppc_pegasos;
	CCont cont;
	EtcFstab *fstab;

	storage::CallbackProgressBar progress_bar_cb;
	storage::CallbackShowInstallInfo install_info_cb;
	storage::CallbackInfoPopup info_popup_cb;
	storage::CallbackYesNoPopup yesno_popup_cb;
	static storage::CallbackProgressBar progress_bar_cb_ycp;
	static storage::CallbackShowInstallInfo install_info_cb_ycp;
	static storage::CallbackInfoPopup info_popup_cb_ycp;
	static storage::CallbackYesNoPopup yesno_popup_cb_ycp;

	friend std::ostream& operator<<(std::ostream& s, const Storage& v);

	map<string, storage::usedBy> danglingUsedBy;

	unsigned max_log_num;
	string lastAction;
	string extendedError;
	std::map<string,CCont> backups;
	std::map<string,FreeInfo> freeInfo;
	std::list<std::pair<string,string> > infoPopupTxts;
    };

}

#endif
