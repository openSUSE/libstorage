#ifndef STORAGE_INTERFACE_H
#define STORAGE_INTERFACE_H


#include <string>
#include <deque>
#include <list>

using std::string;
using std::deque;
using std::list;


/*!
 * \mainpage libstorage Documentation
 *
 * \section Interface
 *
 * The functionality of libstorage is entirely accessed through the abstract
 * interface class \link storage::StorageInterface StorageInterface\endlink.
 * To ensure maximal possible compatibility user of libstorage must only
 * include the header file StorageInterface.h.
 *
 * \section Caching
 *
 * All modifying functions of libstorage can either operate on an
 * internal cache or directly on the system.
 *
 * When the caching mode is enabled a call of e.g. \link
 * storage::StorageInterface::createPartition() createPartition() \endlink
 * will only change the internal cache.  The user has to call \link
 * storage::StorageInterface::commit() commit() \endlink later on to actually
 * create the partition on the disk.
 *
 * When caching mode is disabled the call of e.g. \link
 * storage::StorageInterface::createPartition() createPartition() \endlink
 * will immediately create the partition on the disk.
 *
 * Caching mode can be set with \link
 * storage::StorageInterface::setCacheChanges() setCacheChanges() \endlink and
 * queried with \link storage::StorageInterface::isCacheChanges()
 * isCacheChanges()\endlink.
 *
 * \section Locking
 *
 * During initialisation libstorage installs a global lock so that several
 * programs trying to use libstorage at the same time do not interfere. This
 * lock is either read-only or read-write depending on the readonly parameter
 * used in \link storage::StorageInterface::createStorageInterface()
 * createStorageInterface() \endlink.
 *
 * Several processes may hold a read-lock, but only one process may hold a
 * read-write lock. An read-write lock excludes all other locks, both
 * read-only and read-write.
 *
 * The support for multiple read-only locks is experimental.
 *
 * Locking may also fail for other reasons, e.g. limited permissions.
 *
 * \section Example
 *
 * Here is a simple example to demonstrate the usage of libstorage:
 *
 * \code
 *
 * #include <y2storage/StorageInterface.h>
 *
 * using namespace storage;
 *
 * int
 * main ()
 * {
 *     // First we must create a concrete StorageInterface object.
 *     StorageInterface* s = createStorageInterface (false, false, true);
 *
 *     int ret;
 *     string name;
 *
 *     // Create a primary partition on /dev/hda.
 *     ret = s->createPartitionKb ("/dev/hda", PRIMARY, 0, 100000, name);
 *
 *     // Commit the change to the system.
 *     ret = s->commit ();
 *
 *     // Finally destroy the StorageInterface object.
 *     delete s;
 * }
 *
 * \endcode
 *
 * If you have installed the latest yast2-storage package you can find more
 * examples in the directory
 * /usr/share/doc/packages/yast2-storage/examples/liby2storage.
 */


namespace storage
{
    enum FsType { FSUNKNOWN, REISERFS, EXT2, EXT3, VFAT, XFS, JFS, HFS, NTFS, SWAP, HFSPLUS, NFS, FSNONE };

    enum PartitionType { PRIMARY, EXTENDED, LOGICAL, PTYPE_ANY };

    enum MountByType { MOUNTBY_DEVICE, MOUNTBY_UUID, MOUNTBY_LABEL, MOUNTBY_ID, MOUNTBY_PATH };

    enum EncryptType { ENC_NONE, ENC_TWOFISH, ENC_TWOFISH_OLD,
		       ENC_TWOFISH256_OLD, ENC_LUKS, ENC_UNKNOWN };

    enum MdType { RAID_UNK, RAID0, RAID1, RAID5, RAID6, RAID10, MULTIPATH };

    enum MdParity { PAR_NONE, LEFT_ASYMMETRIC, LEFT_SYMMETRIC,
		    RIGHT_ASYMMETRIC, RIGHT_SYMMETRIC };

    enum UsedByType { UB_NONE, UB_LVM, UB_MD, UB_DM, UB_DMRAID, UB_DMMULTIPATH };

    enum CType { CUNKNOWN, DISK, MD, LOOP, LVM, DM, DMRAID, NFSC, DMMULTIPATH,
		 COTYPE_LAST_ENTRY };

    /**
     * typedef for a pointer to a function that is called on progress bar
     * events.
     */
    typedef void (*CallbackProgressBar)( const string& id, unsigned cur, unsigned max );

    /**
     * typedef for a pointer to a function that is called with strings telling
     * the user what is currently going on.
     */
    typedef void (*CallbackShowInstallInfo)( const string& id );

    /**
     * typedef for a pointer to a function that displays a popup with the
     * given text and waits for user confirmation.
     */
    typedef void (*CallbackInfoPopup)( const string& text );

    /**
     * typedef for a pointer to a function that displays a popup with the
     * given text and two buttons labels "Yes" and "No". The user
     * has to press on of these buttons. If he presses "Yes" true is returned,
     * false otherwise.
     */
    typedef bool (*CallbackYesNoPopup)( const string& text );


    /**
     * Contains capabilities of a filesystem type.
     */
    struct FsCapabilities
    {
	FsCapabilities () {}
	bool isExtendable;
	bool isExtendableWhileMounted;
	bool isReduceable;
	bool isReduceableWhileMounted;
	bool supportsUuid;
	bool supportsLabel;
	bool labelWhileMounted;
	unsigned int labelLength;
	unsigned long long minimalFsSizeK;
    };

    /**
     * Contains info about a generic container.
     */
    struct ContainerInfo
    {
	ContainerInfo() {}
	CType type;
	unsigned volcnt;	// deprecated
	string device;
	string name;
	UsedByType usedByType;
	string usedByName;	// deprecated
	string usedByDevice;
	bool readonly;
    };

    /**
     * Contains info about a disk
     */
    struct DiskInfo
    {
	DiskInfo() {}
	unsigned long long sizeK;
	unsigned long long cylSizeB;
	unsigned long cyl;
	unsigned long heads;
	unsigned long sectors;
	string disklabel;
	string udevPath;
	string udevId;
	unsigned maxLogical;
	unsigned maxPrimary;
	bool initDisk;
	bool iscsi;
    };

    /**
     * Contains info about a LVM VG
     */
    struct LvmVgInfo
    {
	LvmVgInfo() {}
	unsigned long long sizeK;
	unsigned long long peSize;
	unsigned long peCount;
	unsigned long peFree;
	string uuid;
	bool lvm2;
	bool create;
	string devices;
	string devices_add;
	string devices_rem;
    };

    /**
     * Contains info about a DmPart disk
     */
    struct DmPartCoInfo
    {
	DmPartCoInfo() {}
	DiskInfo d;
	string devices;
	unsigned long minor;
    };

    struct DmraidCoInfo
    {
	DmraidCoInfo() {}
	DmPartCoInfo p;
    };

    struct DmmultipathCoInfo
    {
	DmmultipathCoInfo() {}
	DmPartCoInfo p;
	string vendor;
	string model;
    };

    /**
     * Contains info about a volume.
     */
    struct VolumeInfo
    {
	VolumeInfo() {}
	unsigned long long sizeK;
	unsigned long major;
	unsigned long minor;
	string name;
	string device;
	string mount;
	MountByType mount_by;
	UsedByType usedByType;
	string usedByName;	// deprecated
	string usedByDevice;
	string fstab_options;
	string uuid;
	string label;
	string mkfs_options;
	string tunefs_options;
	string loop;
	string dtxt;
	EncryptType encryption;
	string crypt_pwd;
	FsType fs;
	bool format;
	bool create;
	bool is_mounted;
	bool resize;
	bool ignore_fs;
	unsigned long long OrigSizeK;
    };

    struct PartitionAddInfo
    {
	PartitionAddInfo() {}
	unsigned nr;
	unsigned long cylStart;
	unsigned long cylSize;
	PartitionType partitionType;
	unsigned id;
	bool boot;
	string udevPath;
	string udevId;
    };

    /**
     * Contains info about a partition.
     */
    struct PartitionInfo
    {
	PartitionInfo() {}
	PartitionInfo& operator=( const PartitionAddInfo& rhs );
	VolumeInfo v;
	unsigned nr;
	unsigned long cylStart;
	unsigned long cylSize;
	PartitionType partitionType;
	unsigned id;
	bool boot;
	string udevPath;
	string udevId;
    };

    /**
     * Contains info about a LVM LV.
     */
    struct LvmLvInfo
    {
	LvmLvInfo() {}
	VolumeInfo v;
	unsigned stripe;
	unsigned stripe_size;
	string uuid;
	string status;
	string allocation;
	string dm_table;
	string dm_target;
	string origin;
	unsigned long long sizeK;
    };

    /**
     *
     */
    struct LvmLvSnapshotStateInfo
    {
	LvmLvSnapshotStateInfo() {}
	bool active;
	double allocated;
    };

    /**
     * Contains info about a software raid device.
     */
    struct MdInfo
    {
	MdInfo() {}
	VolumeInfo v;
	unsigned nr;
	unsigned type;
	unsigned parity;
	string uuid;
	string sb_ver;
	unsigned long chunk;
	string devices;
    };

    /**
     * Contains state info about a software raid device.
     */
    struct MdStateInfo
    {
	MdStateInfo() {}
	bool active;
	bool degraded;
    };

    /**
     * Contains info about a nfs volumes
     */
    struct NfsInfo
    {
	NfsInfo() {}
	VolumeInfo v;
    };

    /**
     * Contains info about a file based loop devices.
     */
    struct LoopInfo
    {
	LoopInfo() {}
	VolumeInfo v;
	bool reuseFile;
	unsigned nr;
	string file;
    };

    /**
     * Contains info about a DM volume.
     */
    struct DmInfo
    {
	DmInfo() {}
	VolumeInfo v;
	unsigned nr;
	string table;
	string target;
    };

    /**
     * Contains info about a DmPart volume.
     */
    struct DmPartInfo
    {
	DmPartInfo() {}
	VolumeInfo v;
	PartitionAddInfo p;
	bool part;
	string table;
	string target;
    };

    /**
     * Contains info about a DMRAID volume.
     */
    struct DmraidInfo
    {
	DmraidInfo() {}
	DmPartInfo p;
    };

    /**
     * Contains info about a DMMULTIPATH volume.
     */
    struct DmmultipathInfo
    {
	DmmultipathInfo() {}
	DmPartInfo p;
    };

    /**
     * Contains info about a DM volume.
     */
    struct ContVolInfo
    {
	ContVolInfo() { numeric=false; nr=0; type=CUNKNOWN; }
	CType type;
	string cname;
	string vname;
	bool numeric;
	unsigned nr;
    };

    /**
     * Contains info about a partition slot.
     */
    struct PartitionSlotInfo
    {
	PartitionSlotInfo() {}
	unsigned long cylStart;
	unsigned long cylSize;
	bool primarySlot;
	bool primaryPossible;
	bool extendedSlot;
	bool extendedPossible;
	bool logicalSlot;
	bool logicalPossible;
    };

    /**
     * Contains info about actions performed during commit().
     */
    struct CommitInfo
    {
	CommitInfo() {}
	bool destructive;
	string text;
    };


    /**
     * preliminary list of error codes, must have negative values
     */
    enum ErrorCodes
    {
	DISK_PARTITION_OVERLAPS_EXISTING = -1000,
	DISK_PARTITION_EXCEEDS_DISK = -1001,
	DISK_CREATE_PARTITION_EXT_ONLY_ONCE = -1002,
	DISK_CREATE_PARTITION_EXT_IMPOSSIBLE = -1003,
	DISK_PARTITION_NO_FREE_NUMBER = -1004,
	DISK_CREATE_PARTITION_INVALID_VOLUME = -1005,
	DISK_CREATE_PARTITION_INVALID_TYPE = -1006,
	DISK_CREATE_PARTITION_PARTED_FAILED = -1007,
	DISK_PARTITION_NOT_FOUND = -1008,
	DISK_CREATE_PARTITION_LOGICAL_NO_EXT = -1009,
	DISK_PARTITION_LOGICAL_OUTSIDE_EXT = -1010,
	DISK_SET_TYPE_INVALID_VOLUME = -1011,
	DISK_SET_TYPE_PARTED_FAILED = -1012,
	DISK_SET_LABEL_PARTED_FAILED = -1013,
	DISK_REMOVE_PARTITION_PARTED_FAILED = -1014,
	DISK_REMOVE_PARTITION_INVALID_VOLUME = -1015,
	DISK_REMOVE_PARTITION_LIST_ERASE = -1016,
	DISK_DESTROY_TABLE_INVALID_LABEL = -1017,
	DISK_PARTITION_ZERO_SIZE = -1018,
	DISK_CHANGE_READONLY = -1019,
	DISK_RESIZE_PARTITION_INVALID_VOLUME = -1020,
	DISK_RESIZE_PARTITION_PARTED_FAILED = -1021,
	DISK_RESIZE_NO_SPACE = -1022,
	DISK_CHECK_RESIZE_INVALID_VOLUME = -1023,
	DISK_REMOVE_PARTITION_CREATE_NOT_FOUND = -1024,
	DISK_COMMIT_NOTHING_TODO = -1025,
	DISK_CREATE_PARTITION_NO_SPACE = -1026,
	DISK_REMOVE_USED_BY = -1027,
	DISK_INIT_NOT_POSSIBLE = -1028,
	DISK_INVALID_PARTITION_ID = -1029,

	STORAGE_DISK_NOT_FOUND = -2000,
	STORAGE_VOLUME_NOT_FOUND = -2001,
	STORAGE_REMOVE_PARTITION_INVALID_CONTAINER = -2002,
	STORAGE_CHANGE_PARTITION_ID_INVALID_CONTAINER = -2003,
	STORAGE_CHANGE_READONLY = -2004,
	STORAGE_DISK_USED_BY = -2005,
	STORAGE_LVM_VG_EXISTS = -2006,
	STORAGE_LVM_VG_NOT_FOUND = -2007,
	STORAGE_LVM_INVALID_DEVICE = -2008,
	STORAGE_CONTAINER_NOT_FOUND = -2009,
	STORAGE_VG_INVALID_NAME = -2010,
	STORAGE_REMOVE_USED_VOLUME = -2011,
	STORAGE_REMOVE_USING_UNKNOWN_TYPE = -2012,
	STORAGE_NOT_YET_IMPLEMENTED = -2013,
	STORAGE_MD_INVALID_NAME = -2014,
	STORAGE_MD_NOT_FOUND = -2015,
	STORAGE_MEMORY_EXHAUSTED = -2016,
	STORAGE_LOOP_NOT_FOUND = -2017,
	STORAGE_CREATED_LOOP_NOT_FOUND = -2018,
	STORAGE_CHANGE_AREA_INVALID_CONTAINER = -2023,
	STORAGE_BACKUP_STATE_NOT_FOUND = -2024,
	STORAGE_INVALID_FSTAB_VALUE = -2025,
	STORAGE_NO_FSTAB_PTR = -2026,
	STORAGE_DEVICE_NODE_NOT_FOUND = -2027,
	STORAGE_DMRAID_CO_NOT_FOUND = -2028,
	STORAGE_RESIZE_INVALID_CONTAINER = -2029,
	STORAGE_DMMULTIPATH_CO_NOT_FOUND = -2030,
	STORAGE_ZERO_DEVICE_FAILED = -2031,
	STORAGE_INVALID_BACKUP_STATE_NAME = -2032,

	VOLUME_COMMIT_UNKNOWN_STAGE = -3000,
	VOLUME_FSTAB_EMPTY_MOUNT = -3001,
	VOLUME_UMOUNT_FAILED = -3002,
	VOLUME_MOUNT_FAILED = -3003,
	VOLUME_FORMAT_UNKNOWN_FS = -3005,
	VOLUME_FORMAT_FS_UNDETECTED = -3006,
	VOLUME_FORMAT_FS_TOO_SMALL = -3007,
	VOLUME_FORMAT_FAILED = -3008,
	VOLUME_TUNE2FS_FAILED = -3009,
	VOLUME_MKLABEL_FS_UNABLE = -3010,
	VOLUME_MKLABEL_FAILED = -3011,
	VOLUME_LOSETUP_NO_LOOP = -3012,
	VOLUME_LOSETUP_FAILED = -3013,
	VOLUME_CRYPT_NO_PWD = -3014,
	VOLUME_CRYPT_PWD_TOO_SHORT = -3015,
	VOLUME_CRYPT_NOT_DETECTED = -3016,
	VOLUME_FORMAT_EXTENDED_UNSUPPORTED = -3017,
	VOLUME_MOUNT_EXTENDED_UNSUPPORTED = -3018,
	VOLUME_MOUNT_POINT_INVALID = -3019,
	VOLUME_MOUNTBY_NOT_ENCRYPTED = -3020,
	VOLUME_MOUNTBY_UNSUPPORTED_BY_FS = -3021,
	VOLUME_LABEL_NOT_SUPPORTED = -3022,
	VOLUME_LABEL_TOO_LONG = -3023,
	VOLUME_LABEL_WHILE_MOUNTED = -3024,
	VOLUME_RESIZE_UNSUPPORTED_BY_FS = -3025,
	VOLUME_RESIZE_UNSUPPORTED_BY_CONTAINER = -3026,
	VOLUME_RESIZE_FAILED = -3027,
	VOLUME_ALREADY_IN_USE = -3028,
	VOLUME_LOUNSETUP_FAILED = -3029,
	VOLUME_DEVICE_NOT_PRESENT = -3030,
	VOLUME_DEVICE_NOT_BLOCK = -3031,
	VOLUME_MOUNTBY_UNSUPPORTED_BY_VOLUME = -3032,
	VOLUME_CRYPTFORMAT_FAILED = -3033,
	VOLUME_CRYPTSETUP_FAILED = -3034,
	VOLUME_CRYPTUNSETUP_FAILED = -3035,
	VOLUME_FORMAT_NOT_IMPLEMENTED = -3036,
	VOLUME_FORMAT_NFS_IMPOSSIBLE = -3037,
	VOLUME_CRYPT_NFS_IMPOSSIBLE = -3038,
	VOLUME_REMOUNT_FAILED = -3039,
	VOLUME_TUNEREISERFS_FAILED = -3040,

	LVM_CREATE_PV_FAILED = -4000,
	LVM_PV_ALREADY_CONTAINED = -4001,
	LVM_PV_DEVICE_UNKNOWN = -4002,
	LVM_PV_DEVICE_USED = -4003,
	LVM_VG_HAS_NONE_PV = -4004,
	LVM_LV_INVALID_NAME = -4005,
	LVM_LV_DUPLICATE_NAME = -4006,
	LVM_LV_NO_SPACE = -4007,
	LVM_LV_UNKNOWN_NAME = -4008,
	LVM_LV_NOT_IN_LIST = -4009,
	LVM_VG_CREATE_FAILED = -4010,
	LVM_VG_EXTEND_FAILED = -4011,
	LVM_VG_REDUCE_FAILED = -4012,
	LVM_VG_REMOVE_FAILED = -4013,
	LVM_LV_CREATE_FAILED = -4014,
	LVM_LV_REMOVE_FAILED = -4015,
	LVM_LV_RESIZE_FAILED = -4016,
	LVM_PV_STILL_ADDED = -4017,
	LVM_PV_REMOVE_NOT_FOUND = -4018,
	LVM_CREATE_LV_INVALID_VOLUME = -4019,
	LVM_REMOVE_LV_INVALID_VOLUME = -4020,
	LVM_RESIZE_LV_INVALID_VOLUME = -4021,
	LVM_CHANGE_READONLY = -4022,
	LVM_CHECK_RESIZE_INVALID_VOLUME = -4023,
	LVM_COMMIT_NOTHING_TODO = -4024,
	LVM_LV_REMOVE_USED_BY = -4025,
	LVM_LV_ALREADY_ON_DISK = -4026,
	LVM_LV_NO_STRIPE_SIZE = -4027,
	LVM_LV_UNKNOWN_ORIGIN = -4028,
	LVM_LV_NOT_ON_DISK = -4029,
	LVM_LV_NOT_SNAPSHOT = -4030,
	LVM_LV_HAS_SNAPSHOTS = -4031,
	LVM_LV_IS_SNAPSHOT = -4032,

	FSTAB_ENTRY_NOT_FOUND = -5000,
	FSTAB_CHANGE_PREFIX_IMPOSSIBLE = -5001,
	FSTAB_REMOVE_ENTRY_NOT_FOUND = -5002,
	FSTAB_UPDATE_ENTRY_NOT_FOUND = -5003,
	FSTAB_ADD_ENTRY_FOUND = -5004,

	MD_CHANGE_READONLY = -6000,
	MD_DUPLICATE_NUMBER = -6001,
	MD_TOO_FEW_DEVICES = -6002,
	MD_DEVICE_UNKNOWN = -6003,
	MD_DEVICE_USED = -6004,
	MD_CREATE_INVALID_VOLUME = -6005,
	MD_REMOVE_FAILED = -6006,
	MD_NOT_IN_LIST = -6007,
	MD_CREATE_FAILED = -6008,
	MD_UNKNOWN_NUMBER = -6009,
	MD_REMOVE_USED_BY = -6010,
	MD_NUMBER_TOO_LARGE = -6011,
	MD_REMOVE_INVALID_VOLUME = -6012,
	MD_REMOVE_CREATE_NOT_FOUND = -6013,
	MD_NO_RESIZE_ON_DISK = -6014,
	MD_ADD_DUPLICATE = -6015,
	MD_REMOVE_NONEXISTENT = -6016,
	MD_NO_CHANGE_ON_DISK = -6017,
	MD_NO_CREATE_UNKNOWN = -6018,
	MD_STATE_NOT_ON_DISK = -6019,

	LOOP_CHANGE_READONLY = -7000,
	LOOP_DUPLICATE_FILE = -7001,
	LOOP_UNKNOWN_FILE = -7002,
	LOOP_REMOVE_USED_BY = -7003,
	LOOP_FILE_CREATE_FAILED = -7004,
	LOOP_CREATE_INVALID_VOLUME = -7005,
	LOOP_REMOVE_FILE_FAILED = -7006,
	LOOP_REMOVE_INVALID_VOLUME = -7007,
	LOOP_NOT_IN_LIST = -7008,
	LOOP_REMOVE_CREATE_NOT_FOUND = -7009,
	LOOP_MODIFY_EXISTING = -7010,

	PEC_PE_SIZE_INVALID = -9000,
	PEC_PV_NOT_FOUND = -9001,
	PEC_REMOVE_PV_IN_USE = -9002,
	PEC_REMOVE_PV_SIZE_NEEDED = -9003,
	PEC_LV_NO_SPACE_STRIPED = -9004,
	PEC_LV_NO_SPACE_SINGLE = -9005,
	PEC_LV_PE_DEV_NOT_FOUND = -9006,

	DM_CHANGE_READONLY = -10000,
	DM_UNKNOWN_TABLE = -10001,
	DM_REMOVE_USED_BY = -10002,
	DM_REMOVE_CREATE_NOT_FOUND = -10003,
	DM_REMOVE_INVALID_VOLUME = -10004,
	DM_REMOVE_FAILED = -10005,
	DM_NOT_IN_LIST = -10006,

	DASD_NOT_POSSIBLE = -11000,
	DASD_FDASD_FAILED = -11001,
	DASD_DASDFMT_FAILED = -11002,

	DMPART_CHANGE_READONLY = -12001,
	DMPART_INTERNAL_ERR = -12002,
	DMPART_INVALID_VOLUME = -12003,
	DMPART_PARTITION_NOT_FOUND = -12004,
	DMPART_REMOVE_PARTITION_LIST_ERASE = -12005,
	DMPART_COMMIT_NOTHING_TODO = -12006,
	DMPART_NO_REMOVE = -12007,

	DMRAID_REMOVE_FAILED = -13001,

	NFS_VOLUME_NOT_FOUND = -14001,
	NFS_CHANGE_READONLY = -14002,
	NFS_REMOVE_VOLUME_CREATE_NOT_FOUND = -14003,
	NFS_REMOVE_VOLUME_LIST_ERASE = -14004,
	NFS_REMOVE_INVALID_VOLUME = -14005,

	CONTAINER_INTERNAL_ERROR = -99000,
	CONTAINER_INVALID_VIRTUAL_CALL = -99001,

    };


    /**
     * \brief Abstract class defining the interface for libstorage.
     */
    class StorageInterface
    {
    public:

	StorageInterface () {}
	virtual ~StorageInterface () {}

	/**
	 * Query all containers found in system
	 */
	virtual void getContainers( deque<ContainerInfo>& infos) = 0;

	/**
	 * Query disk info for a disk device
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param info record that get filled with disk special data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getDiskInfo( const string& disk, DiskInfo& info) = 0;

	/**
	 * Query disk info for a disk device
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param cinfo record that gets filled with container general data
	 * @param info record that gets filled with disk special data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getContDiskInfo( const string& disk, ContainerInfo& cinfo,
	                             DiskInfo& info ) = 0;

	/**
	 * Query info for a LVM volume group
	 *
	 * @param name name of volume group, e.g. system
	 * @param info record that gets filled with LVM VG special data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getLvmVgInfo( const string& name, LvmVgInfo& info) = 0;

	/**
	 * Query info for a LVM volume group
	 *
	 * @param name name of volume group, e.g. system
	 * @param cinfo record that gets filled with container general data
	 * @param info record that gets filled with LVM VG special data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getContLvmVgInfo( const string& name, ContainerInfo& cinfo,
	                              LvmVgInfo& info) = 0;

	/**
	 * Query container info for a DMRAID container
	 *
	 * @param name name of container, e.g. pdc_ccaihgii
	 * @param info record that gets filled with DMRAID Container special data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getDmraidCoInfo( const string& name, DmraidCoInfo& info) = 0;

	/**
	 * Query container info for a DMRAID container
	 *
	 * @param name name of container, e.g. pdc_ccaihgii
	 * @param cinfo record that gets filled with container general data
	 * @param info record that gets filled with DMRAID Container special data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getContDmraidCoInfo( const string& name, ContainerInfo& cinfo,
				         DmraidCoInfo& info) = 0;

	/**
	 * Query container info for a DMMULTIPATH container
	 *
	 * @param name name of container, e.g. 3600508b400105f590000900000300000
	 * @param info record that gets filled with DMMULTIPATH Container special data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getDmmultipathCoInfo( const string& name, DmmultipathCoInfo& info) = 0;

	/**
	 * Query container info for a DMMULTIPATH container
	 *
	 * @param name name of container, e.g. 3600508b400105f590000900000300000
	 * @param cinfo record that gets filled with container general data
	 * @param info record that gets filled with DMMULTIPATH Container special data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getContDmmultipathCoInfo( const string& name, ContainerInfo& cinfo,
					      DmmultipathCoInfo& info) = 0;

	/**
	 * Query all volumes found in system
	 *
	 * @param infos list of records that get filled with volume info
	 */
	virtual void getVolumes( deque<VolumeInfo>& infos) = 0;

	/**
	 * Query a volume by device name found in system
	 *
	 * @param device device name , e.g. /dev/hda1
	 * @param info record that gets filled with data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getVolume( const string& device, VolumeInfo& info) = 0;

	/**
	 * Query infos for partitions of a disk
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param plist list of records that get filled with partition specific info
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getPartitionInfo( const string& disk,
	                              deque<PartitionInfo>& plist ) = 0;

	/**
	 * Query infos for LVM LVs of a LVM VG
	 *
	 * @param name name of volume group, e.g. system
	 * @param plist list of records that get filled with LV specific info
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getLvmLvInfo( const string& name,
				  deque<LvmLvInfo>& plist ) = 0;

	/**
	 * Query infos for software raid devices in system
	 *
	 * @param plist list of records that get filled with MD specific info
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getMdInfo( deque<MdInfo>& plist ) = 0;

	/**
	 * Query infos for nfs devices in system
	 *
	 * @param plist list of records that get filled with nfs info
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getNfsInfo( deque<NfsInfo>& plist ) = 0;

	/**
	 * Query infos for file based loop devices in system
	 *
	 * @param plist list of records that get filled with loop specific info
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getLoopInfo( deque<LoopInfo>& plist ) = 0;

	/**
	 * Query infos for dm devices in system
	 *
	 * @param plist list of records that get filled with dm specific info
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getDmInfo( deque<DmInfo>& plist ) = 0;

	/**
	 * Query infos for dmraid devices in system
	 *
	 * @param plist list of records that get filled with dmraid specific info
	 * @param name name of dmraid, e.g. pdc_igeeeadj
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getDmraidInfo( const string& name,
	                           deque<DmraidInfo>& plist ) = 0;

	/**
	 * Query infos for dmmultipath devices in system
	 *
	 * @param plist list of records that get filled with dmmultipath specific info
	 * @param name name of dmmultipath, e.g. 3600508b400105f590000900000300000
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getDmmultipathInfo( const string& name,
					deque<DmmultipathInfo>& plist ) = 0;

	/**
	 * Query capabilities of a filesystem type.
	 */
	virtual bool getFsCapabilities (FsType fstype, FsCapabilities& fscapabilities) const = 0;

	/**
	 * Get list of filesystem types present on any block devices.
	 */
	virtual list<string> getAllUsedFs() const = 0;

	/**
	 * Create a new partition. Units given in disk cylinders.
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param type type of partition to create, e.g. primary or extended
	 * @param start cylinder number of partition start (cylinders are numbered starting with 0)
	 * @param sizeCyl size of partition in disk cylinders
	 * @param device is set to the device name of the new partition
	 * The name is returned instead of the number since creating the name from the
	 * number is not straight-forward.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createPartition( const string& disk, PartitionType type,
				     unsigned long start,
				     unsigned long sizeCyl,
				     string& device ) = 0;

	/**
	 * Resize an existing disk partition. Units given in disk cylinders.
	 * Filesystem data is resized accordingly.
	 *
	 * @param device device name of partition
	 * @param sizeCyl new size of partition in disk cylinders
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int resizePartition( const string& device,
				     unsigned long sizeCyl ) = 0;

	/**
	 * Resize an existing disk partition. Units given in disk cylinders.
	 * Filesystem data is ignored.
	 *
	 * @param device device name of partition
	 * @param sizeCyl new size of partition in disk cylinders
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int resizePartitionNoFs( const string& device,
					 unsigned long sizeCyl ) = 0;

	/**
	 * Update area used by a new partition. Units given in disk cylinders.
	 * This function can only be used with a partition created but not yet
	 * committed.
	 *
	 * @param device device name of partition, e.g. /dev/hda1
	 * @param start cylinder number of partition start (cylinders are numbered starting with 0)
	 * @param sizeCyl size of partition in disk cylinders
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int updatePartitionArea( const string& device,
	                                 unsigned long start,
					 unsigned long sizeCyl ) = 0;

	/**
	 * Return the number of free cylinders after a partition.
	 *
	 * @param device device name of partition, e.g. /dev/sda1
	 * @param freeCyls is set to the number of free cylinders
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int freeCylindersAfterPartition(const string& device, unsigned long& freeCyls) = 0;

	/**
	 * Determine the device name of the next created partition
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param type type of partition to create, e.g. primary or extended
	 * @param nr is set to the number of the next created partition
	 * @param device is set to the device name of the next created partition
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int nextFreePartition( const string& disk, PartitionType type,
	                               unsigned &nr, string& device ) = 0;

	/**
	 * Create a new partition. Units given in Kilobytes.
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param type type of partition to create, e.g. primary or extended
	 * @param start offset in kilobytes from start of disk
	 * @param size  size of partition in kilobytes
	 * @param device is set to the device name of the new partition
	 * The name is returned instead of the number since creating the name from the
	 * number is not straight-forward.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createPartitionKb( const string& disk, PartitionType type,
				       unsigned long long start,
				       unsigned long long size,
				       string& device ) = 0;

	/**
	 * Create a new partition of any type anywhere on the disk. Units given in Kilobytes.
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param size  size of partition in kilobytes
	 * @param device is set to the device name of the new partition
	 * The name is returned instead of the number since creating the name from the
	 * number is not straight-forward.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createPartitionAny( const string& disk,
					unsigned long long size,
					string& device ) = 0;

	/**
	 * Create a new partition of given type as large as possible.
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param type type of partition to create, e.g. primary or extended
	 * @param device is set to the device name of the new partition
	 * The name is returned instead of the number since creating the name from the
	 * number is not straight-forward.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createPartitionMax( const string& disk, PartitionType type,
					string& device ) = 0;

	/**
	 * Compute number of kilobytes of a given number of disk cylinders
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param size number of disk cylinders
	 * @return number of kilobytes of given cylinders
	 */
	virtual unsigned long long cylinderToKb( const string& disk,
	                                         unsigned long size ) = 0;

	/**
	 * Compute number of disk cylinders needed for given space
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param size number of kilobytes
	 * @return number of disk cylinders needed
	 */
	virtual unsigned long kbToCylinder( const string& disk,
					    unsigned long long size ) = 0;

	/**
	 * Remove a partition
	 *
	 * @param partition name of partition, e.g. /dev/hda1
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removePartition (const string& partition) = 0;

	/**
	 * Change partition id of a partition
	 *
	 * @param partition name of partition, e.g. /dev/hda1
	 * @param id new partition id (e.g. 0x82 swap, 0x8e for lvm, ...)
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changePartitionId (const string& partition, unsigned id) = 0;

	/**
	 * Forget previously issued change of partition id
	 *
	 * @param partition name of partition, e.g. /dev/hda1
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int forgetChangePartitionId (const string& partition ) = 0;

	/**
	 * Query unused slots on a disk suitable for creating partitions.
	 *
	 * This functions ignores size limitations of the partition table type,
	 * e.g. on MSDOS labels partitions cannot exceed 2TB.
	 *
	 * @param disk name of disk, e.g. /dev/hda1
	 * @param slots list of records that get filled with partition slot specific info
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getUnusedPartitionSlots(const string& disk, list<PartitionSlotInfo>& slots) = 0;

	/**
	 * Destroys the partition table of a disk. An empty disk label
	 * of the given type without any partition is created.
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param label disk label to create on disk, e.g. msdos, gpt, ...
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int destroyPartitionTable (const string& disk, const string& label) = 0;

	/**
	 * Do a what is needed for low level initialisation of a Disk.
	 * This function does nothing on normal disks but is needed e.g. on S390
	 * DASD devices where it executes a dasdfmt. If should be considered as
	 * destroying all data on the disk.
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param value toggle if disk should be initialized or not
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int initializeDisk( const string& disk, bool value ) = 0;

	/**
	 * Query the default disk label of the architecture of the
	 * machine (e.g. msdos for ix86, gpt for ia64, ...)
	 *
	 * @return default disk label of the architecture
	 */
	virtual string defaultDiskLabel() const = 0;

	/**
	 * Query the default disk label of the architecture of the
	 * machine (e.g. msdos for ix86, gpt for ia64, ...) for a disk
	 * with certain size
	 *
	 * @param size_k size of disk in kilobyte
	 *
	 * @return default disk label of the disk
	 */
	virtual string defaultDiskLabelSize( unsigned long long size_k ) const = 0;

	/**
	 * Query the maximal allowed size the given disk label supports.
	 *
	 * @return maximal supported size of disk label
	 */
	virtual unsigned long long maxSizeLabelK( const string& label ) const = 0;

	/**
	 * Sets or unsets the format flag for the given volume.
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param format flag if format is set on or off
	 * @param fs type of filesystem to create if format is true
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeFormatVolume( const string& device, bool format, FsType fs ) = 0;

	/**
	 * Sets the value of the filesystem label.
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param label value of the label
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeLabelVolume( const string& device, const string& label ) = 0;

	/**
	 * Sets the value of mkfs options.
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param opts options for mkfs command
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeMkfsOptVolume( const string& device, const string& opts ) = 0;

	/**
	 * Sets the value of tunefs options.
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param opts options for tunefs command
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeTunefsOptVolume( const string& device, const string& opts ) = 0;

	/**
	 * Changes the mount point of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param mount new mount point of the volume (e.g. /home).
	 *    it is valid to set an empty mount point
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeMountPoint( const string& device, const string& mount ) = 0;

	/**
	 * Get the mount point of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param mount will be set to the mount point of the volume (e.g. /home).
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getMountPoint( const string& device, string& mount ) = 0;

	/**
	 * Changes mount by value in fstab of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param mby new mount by value of the volume.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeMountBy( const string& device, MountByType mby ) = 0;

	/**
	 * Get mount by value in fstab of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param mby will be set to the mount by value of the volume.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
#ifndef SWIG
	virtual int getMountBy( const string& device, MountByType& mby ) = 0;
#else
	virtual int getMountBy( const string& device, MountByType& REFERENCE ) = 0;
#endif

	/**
	 * Changes the fstab options of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param options new fstab options of the volume (e.g. noauto,user,sync).
	 *    Multiple options are separated by ",".
	 *    It is valid to set an empty fstab option.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeFstabOptions( const string& device, const string& options ) = 0;

	/**
	 * Get the fstab options of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param options will be set to the fstab options of the volume (e.g. noauto,user,sync).
	 *    Multiple options are separated by ",".
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getFstabOptions( const string& device, string& options ) = 0;

	/**
	 * Add to the fstab options of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param options fstab options to add to already exiting options of the volume (e.g. noauto,user,sync).
	 *    Multiple options are separated by ",".
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int addFstabOptions( const string& device, const string& options ) = 0;

	/**
	 * Remove from the fstab options of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param options fstab options to remove from already existing options of the volume (e.g. noauto).
	 *    Multiple options are separated by ",".
	 *    It is possible to specify wildcards, so "uid=.*" matches every option starting with the string "uid=".
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeFstabOptions( const string& device, const string& options ) = 0;

	/**
	 * Set crypt password of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param pwd crypt password for this volume
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int setCryptPassword( const string& device, const string& pwd ) = 0;

	/**
	 * Makes library forget a crypt password of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int forgetCryptPassword( const string& device ) = 0;

	/**
	 * Get crypt password of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param pwd crypt password for this volume
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getCryptPassword( const string& device, string& pwd ) = 0;

	/**
	 * Set encryption state of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param val flag if encryption should be activated
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int setCrypt( const string& device, bool val ) = 0;

	/**
	 * Set encryption state of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param val flag if encryption should be activated
	 * @param typ type of encryption to set up
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int setCryptType( const string& device, bool val, EncryptType typ ) = 0;

	/**
	 * Get encryption state of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param val will be set if encryption is activated
	 * @return zero if all is ok, a negative number to indicate an error
	 */
#ifndef SWIG
	virtual int getCrypt( const string& device, bool& val ) = 0;
#else
	virtual int getCrypt( const string& device, bool& REFERENCE ) = 0;
#endif

	/**
	 * Set fstab handling state of a volume. This way one can make
	 * libstorage ignore fstab handling for a volume.
	 * Use this with care.
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param val flag if fstab should be ignored for this volume
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int setIgnoreFstab( const string& device, bool val ) = 0;

	/**
	 * Get fstab handling state of a volume.
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param val will be set if fstab should be ignored for this volume
	 * @return zero if all is ok, a negative number to indicate an error
	 */
#ifndef SWIG
	virtual int getIgnoreFstab( const string& device, bool& val ) = 0;
#else
	virtual int getIgnoreFstab( const string& device, bool& REFERENCE ) = 0;
#endif

	/**
	 * Sets the value of description text.
	 * This text will be returned together with the text returned by
	 * getCommitInfos().
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param txt description text for this partition
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeDescText( const string& device, const string& txt ) = 0;

	/**
	 * Adds the specified entry to /etc/fstab
	 *
	 * This function does not cache the changes but writes them
	 * immediately.
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param mount mount point, e.g. /home
	 * @param vfs virtual filesystem type, e.g. reiserfs or ext3
	 * @param options fstab options e.g. noauto,user,sync
	 * @param freq value for fifth fstab field
	 * @param passno value for sixth fstab field
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int addFstabEntry( const string& device, const string& mount,
	                           const string& vfs, const string& options,
				   unsigned freq, unsigned passno ) = 0;


	/**
	 * Resizes a volume while keeping the data on the filesystem
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param newSizeMb new size desired volume in Megabyte
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int resizeVolume( const string& device, unsigned long long newSizeMb ) = 0;

	/**
	 * Resizes a volume while ignoring the data on the filesystem
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param newSizeMb new size desired volume in Megabyte
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int resizeVolumeNoFs( const string& device, unsigned long long newSizeMb ) = 0;

	/**
	 * Forget about possible resize of an volume.
	 *
	 * @param device device name of volume
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int forgetResizeVolume( const string& device ) = 0;

	/**
	 * Set handling of deletion of entities that belong to other
	 * volumes. Normally it is not possible to remove a volume that
	 * is part of another entity (e.g. you cannot remove a partition
	 * that is part of an LVM Volume group or a Software raid).
	 * This setting makes the removal recursive, this means all
	 * entities where the removed volume is a part of are also
	 * removed. Use this setting with extreme care, it may cause
	 * the removal of LVM Volume group spanning multiple disks just
	 * because one partition of the LVM Volume group got deleted.
	 * Default value of this flag is false.
	 *
	 * @param val flag if removal is done recursive
	 */
	virtual void setRecursiveRemoval( bool val ) = 0;

	/**
	 * Get value of the flag for recursive removal
	 *
	 * @return value of the flag for recursive removal
	 */
	virtual bool getRecursiveRemoval() const = 0;

	/**
	 * Set handling of newly created partitions. With this flag
	 * once can make the library overwrite start and end of newly
	 * created partitions with zeroes. This prevents that obsolete
	 * structures (e.g. LVM VGs or MD superblocks) still exists on
	 * newly created partitions since the area on disk previously
	 * contained a LVM PV or a device of a software raid.
	 * volumes. Use this setting with extreme care, it make libstorage
	 * behave fundamentally different from all other partitioning tools.
	 * Default value of this flag is false.
	 *
	 * @param val flag if newly created partitions should be zeroed
	 */
	virtual void setZeroNewPartitions( bool val ) = 0;

	/**
	 * Get value of the flag for zeroing newly created partitions
	 *
	 * @return value of the flag for zeroing newly created partitions
	 */
	virtual bool getZeroNewPartitions() const = 0;

	/**
	 * Set default value for mount by.
	 *
	 * @param val new default mount by value.
	 */
	virtual void setDefaultMountBy( MountByType val ) = 0;

	/**
	 * Set default value for mount by.
	 *
	 * @return default value for mount by
	 */
	virtual MountByType getDefaultMountBy() const = 0;

	/**
	 * Set value for EFI boot.
	 *
	 * Currently this value affects the default disk label.
	 *
	 * @param val new efi boot value
	 */
	virtual void setEfiBoot(bool val) = 0;

	/**
	 * Get value for EFI boot.
	 *
	 * @return value for efi boot
	 */
	virtual bool getEfiBoot() const = 0;

	/**
	 * Set value for root prefix.
	 *
	 * This value is appended to all mount points of volumes, when
	 * changes are commited. Config files fstab, cryptotab, raidtab and
	 * mdadm.conf are also created relative to this prefix.
	 * This variable must be set before first call to commit.
	 *
	 * @param root new value for root prefix
	 */
	virtual void setRootPrefix( const string& root ) = 0;

	/**
	 * Get value for root prefix.
	 *
	 * @return value for root prefix
	 */
	virtual string getRootPrefix() const = 0;

	/**
	 * Determine of libstorage should detect mounted volumes.
	 *
	 * @param val flag if mounted volumes should be detected
	 */
	virtual void setDetectMountedVolumes( bool val ) = 0;

	/**
	 * Get value of the flag for detection of mounted volumes.
	 *
	 * @return value of the flag for detection of mounted volumes
	 */
	virtual bool getDetectMountedVolumes() const = 0;

	/**
	 * Removes a volume from the system. This function can be used
	 * for removing all types of volumes (partitions, LVM LVs, MD devices ...)
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeVolume( const string& device ) = 0;

	/**
	 * Create a LVM volume group
	 *
	 * @param name name of volume group, must not contain blanks, colons
	 * and shell special characters (e.g. system)
	 * @param peSizeK physical extent size in kilobytes
	 * @param lvm1 flag if lvm1 compatible format should be used
	 * @param devs list with physical devices to add to that volume group
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createLvmVg( const string& name,
	                         unsigned long long peSizeK, bool lvm1,
	                         const deque<string>& devs ) = 0;

	/**
	 * Remove a LVM volume group. If the volume group contains
	 * logical volumes, these are automatically also removed.
	 *
	 * @param name name of volume group
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeLvmVg( const string& name ) = 0;

	/**
	 * Extend a LVM volume group with additional physical devices
	 *
	 * @param name name of volume group
	 * @param devs list with physical devices to add to that volume group
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int extendLvmVg( const string& name,
	                         const deque<string>& devs ) = 0;

	/**
	 * Shrink a LVM volume group
	 *
	 * @param name name of volume group
	 * @param devs list with physical devices to remove from that volume group
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int shrinkLvmVg( const string& name,
	                         const deque<string>& devs ) = 0;

	/**
	 * Create a LVM logical volume
	 *
	 * @param vg name of volume group
	 * @param name of logical volume
	 * @param sizeM size of logical volume in megabytes
	 * @param stripe stripe count of logical volume (use 1 unless you know
	 * exactly what you are doing)
	 * @param device is set to the device name of the new LV
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createLvmLv( const string& vg, const string& name,
	                         unsigned long long sizeM, unsigned stripe,
				 string& device ) = 0;

	/**
	 * Remove a LVM logical volume
	 *
	 * @param device name of logical volume
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeLvmLvByDevice( const string& device ) = 0;

	/**
	 * Remove a LVM logical volume
	 *
	 * @param vg name of volume group
	 * @param name of logical volume
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeLvmLv( const string& vg, const string& name ) = 0;

	/**
	 * Change stripe count of a LVM logical volume.
	 * This can only be before the volume is created on disk.
	 *
	 * @param vg name of volume group
	 * @param name of logical volume
	 * @param stripes new stripe count of logical volume
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeLvStripeCount( const string& vg, const string& name,
	                                 unsigned long stripes ) = 0;

	/**
	 * Change stripe size of a LVM logical volume.
	 * This can only be before the volume is created on disk.
	 *
	 * @param vg name of volume group
	 * @param name of logical volume
	 * @param stripeSize new stripe size of logical volume
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeLvStripeSize( const string& vg, const string& name,
	                                unsigned long long stripeSize ) = 0;

	/**
	 * Create a LVM logical volume snapshot
	 *
	 * @param vg name of volume group
	 * @param origin name of logical volume origin
	 * @param name of logical volume snapshot
	 * @param cowSizeK size of snapshot in kilobytes
	 * @param device is set to the device name of the new snapshot
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createLvmLvSnapshot(const string& vg, const string& origin,
					const string& name, unsigned long long cowSizeK,
					string& device) = 0;

	/**
	 * Remove a LVM logical volume snapshot
	 *
	 * @param vg name of volume group
	 * @param name name of logical volume snapshot
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeLvmLvSnapshot(const string& vg, const string& name) = 0;

	/**
	 * Get state of a LVM logical volume snapshot
	 *
	 * @pre This can only be done after the snapshot has been created on disk.
	 *
	 * @param vg name of volume group
	 * @param name name of logical volume snapshot
	 * @param info record that gets filled with snapshot special data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getLvmLvSnapshotStateInfo(const string& vg, const string& name,
					      LvmLvSnapshotStateInfo& info) = 0;

	/**
         * Determine the device name of the next created software raid device
         *
         * @param nr is set to the number of the next created software raid device
         * @param device is set to the device name of the next created software raid device
         * @return zero if all is ok, a negative number to indicate an error
         */
	virtual int nextFreeMd(int &nr, string &device) = 0;

	/**
	 * Create a Software raid device by name
	 *
	 * @param name name of software raid device to create (e.g. /dev/md0)
	 * @param rtype raid personality of the new software raid
	 * @param devs list with physical devices for the new software raid
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createMd( const string& name, MdType rtype,
			      const deque<string>& devs ) = 0;

	/**
	 * Create a Software raid device. Name determined by library.
	 *
	 * @param rtype raid personality of the new software raid
	 * @param devs list with physical devices for the new software raid
	 * @param device device name of created software raid device
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createMdAny( MdType rtype, const deque<string>& devs,
				 string& device ) = 0;

	/**
	 * Remove a Software raid device.
	 *
	 * @param name name of software raid device to remove (e.g. /dev/md0)
	 * @param destroySb flag if the MD superblocks on the physical devices
	 *        should be destroyed after md device is deleted
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeMd( const string& name, bool destroySb ) = 0;

	/**
	 * Add a partition to a raid device.
	 * This can only be done before the raid is created on disk.
	 *
	 * @param name name of software raid device (e.g. /dev/md0)
	 * @param dev partition to add to that raid
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int extendMd( const string& name, const string& dev ) = 0;

	/**
	 * Remove a partition from a raid device.
	 * This can only be done before the raid is created on disk.
	 *
	 * @param name name of software raid device (e.g. /dev/md0)
	 * @param dev partition to remove from that raid
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int shrinkMd( const string& name, const string& dev ) = 0;

	/**
	 * Change raid type of a raid device.
	 * This can only be done before the raid is created on disk.
	 *
	 * @param name name of software raid device (e.g. /dev/md0)
	 * @param rtype new raid personality of the software raid
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeMdType( const string& name, MdType rtype ) = 0;

	/**
	 * Change chunk size of a raid device.
	 * This can only be done before the raid is created on disk.
	 *
	 * @param name name of software raid device (e.g. /dev/md0)
	 * @param chunk new chunk size of the software raid
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeMdChunk( const string& name, unsigned long chunk ) = 0;

	/**
	 * Change parity of a raid device with raid type raid5.
	 * This can only be done before the raid is created on disk.
	 *
	 * @param name name of software raid device (e.g. /dev/md0)
	 * @param ptype new parity of the software raid
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeMdParity( const string& name, MdParity ptype ) = 0;

	/**
	 * Check if a raid device is valid
	 *
	 * @param name name of software raid device (e.g. /dev/md0)
	 * @return true if all is ok, a false to indicate an error
	 */
	virtual int checkMd( const string& name ) = 0;

	/**
	 * Get state of a raid device.
	 *
	 * @pre This can only be done after the raid has been created on disk.
	 *
	 * @param name name of software raid device (e.g. /dev/md0)
	 * @param info record that gets filled with raid special data
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getMdStateInfo(const string& name, MdStateInfo& info) = 0;

	/**
	 * Compute the size of a raid device.
	 *
	 * The size compute may not be accurate. It must not be used for
	 * further computations. Do not used in new code.
	 *
	 * @param md_type raid type of the software raid
	 * @param devices list with physical devices for the software raid
	 * @param sizeK will contain the computed size in kilobytes
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int computeMdSize(MdType md_type, list<string> devices,
				  unsigned long long& sizeK) = 0;

	/**
	 * Add knowledge about existence of nfs device.
	 *
	 * @param nfsDev name of nfs device
	 * @param sizeK size of the nfs device
	 * @param opts mount options for nfs mount
	 * @param mp mount point of the nfs device
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int addNfsDevice( const string& nfsDev, const string& opts,
	                          unsigned long long sizeK,
				  const string& mp ) = 0;

	/**
	 * Check accessibility and size of nfs device.
	 *
	 * @param nfsDev name of nfs device
	 * @param opts mount options for nfs mount
	 * @param sizeK size of the nfs device
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int checkNfsDevice( const string& nfsDev, const string& opts,
	                            unsigned long long& sizeK ) = 0;

	/**
	 * Create a file based loop device. Encryption is automatically
	 * activated on the loop device.
	 *
	 * @param lname name of file the loop device is based on
	 * @param reuseExisting if true an already existing file will be
	 *    reused. if false the file will be created new. if false
	 *    the format flag for the device is set by default.
	 * @param sizeK size of the created file, this parameter is ignored
	 *    if reuseExisting is true and a file already exists.
	 * @param mp mount point of the file based loop device
	 * @param pwd crypt password for the loop device, encryption type
	 *    is determined automatically by the system
	 * @param device the name of the created loop device
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createFileLoop( const string& lname, bool reuseExisting,
	                            unsigned long long sizeK,
				    const string& mp, const string& pwd,
				    string& device ) = 0;

	/**
	 * Modify size and pathname of a file based loop device.
	 * This function can only be used between the creation of a
	 * device and the next call to commit(). Containers that
	 * are already created cannot have these properties changed.
	 * The size has only a meaning if reuseExisting is true,
	 * otherwise it is ignored.
	 *
	 * @param device device name of the loop device
	 * @param lname name of file the loop device is based on
	 * @param reuseExisting if true an already existing file will be
	 *    reused. if false the file will be created new. if false
	 *    the format flag for the device is set by default.
	 * @param sizeK size of the created file, this parameter is ignored
	 *    if reuseExisting is false
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int modifyFileLoop( const string& device, const string& lname,
	                            bool reuseExisting,
	                            unsigned long long sizeK ) = 0;

	/**
	 * Remove a file based loop device from the system.
	 *
	 * @param lname name of file the loop device is based on
	 * @param removeFile if true the file is removed together with
	 *    the based loop device. If false the file is not touched.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeFileLoop( const string& lname, bool removeFile ) = 0;

	/**
	 * Remove a Software raid device.
	 *
	 * @param name name of dmraid device to remove (e.g. pdc_dabaheedj)
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeDmraid( const string& name ) = 0;

	/**
	 * Gets info about actions to be executed after next call to commit().
	 *
	 * @param infos list of records that gets filled with infos
	 */
	virtual void getCommitInfos(list<CommitInfo>& infos) const = 0;

	/**
	 * Gets action performed last during previous call to commit()
	 *
	 * @return string presentable to the user
	 */
	virtual const string& getLastAction() const = 0;

	/**
	 * Gets a possible existing extended error message describing failure
	 * of to last call commit()
	 *
	 * @return string error text provided by external program
	 */
	virtual const string& getExtendedErrorMessage() const = 0;

// temporarily disable callback function for swig
#ifndef SWIG

	/**
	 * Sets the callback function called on progress bar events
	 *
	 * @param pfnc pointer to function
	 */
	virtual void setCallbackProgressBar( CallbackProgressBar pfnc ) = 0;

	/**
	 * Query the callback function called on progress bar events
	 *
	 * @return pointer to function currently called for progress bar events
	 */
	virtual CallbackProgressBar getCallbackProgressBar() const = 0;


	/**
	 * Sets the callback function called to display install info
	 *
	 * @param pfnc pointer to function
	 */
	virtual void setCallbackShowInstallInfo( CallbackShowInstallInfo pfnc ) = 0;

	/**
	 * Query the callback function called to display install info
	 *
	 * @return pointer to function currently called for progress bar events
	 */
	virtual CallbackShowInstallInfo getCallbackShowInstallInfo() const = 0;


	/**
	 * Sets the callback function called to display a info popup to the
	 * user
	 *
	 * @param pfnc pointer to function
	 */
	virtual void setCallbackInfoPopup( CallbackInfoPopup pfnc ) = 0;

	/**
	 * Query the callback function called to display info popup to the
	 * user
	 *
	 * @return pointer to function currently called for progress bar events
	 */
	virtual CallbackInfoPopup getCallbackInfoPopup() const = 0;


	/**
	 * Sets the callback function called to get a Yes/No decision by
	 * the user.
	 *
	 * @param pfnc pointer to function
	 */
	virtual void setCallbackYesNoPopup( CallbackYesNoPopup pfnc ) = 0;

	/**
	 * Query the callback function called to get a Yes/No decision by
	 * the user.
	 *
	 * @return pointer to function currently called for progress bar events
	 */
	virtual CallbackYesNoPopup getCallbackYesNoPopup() const = 0;

#endif

	/**
	 * With the function setCacheChanges you can turn the caching mode on
	 * and off.  Turning of caching mode will cause all changes done so
	 * far to be committed upto the next modifying function.
	 */
	virtual void setCacheChanges (bool cache) = 0;

	/**
	 * Query the caching mode.
	 */
	virtual bool isCacheChanges () const = 0;

	/**
	 * Commit the current state to the system.  Only useful in caching
	 * mode.
	 */
	virtual int commit() = 0;

	/**
	 * Create backup of current state of all containers
	 *
	 * @param name name under which the backup should be created
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int createBackupState( const string& name ) = 0;

	/**
	 * Restore state to a previously created backup
	 *
	 * @param name name of the backup to restore
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int restoreBackupState( const string& name ) = 0;

	/**
	 * Checks if a backup with a certain name already exists
	 *
	 * @param name name of the backup to check
	 * @return true if the backup exists
	 */
	virtual bool checkBackupState(const string& name) const = 0;

	/**
	 * Compare two backup states
	 *
	 * @param lhs name of backup to compare, empty string means active state
	 * @param rhs name of backup to compare, empty string means active state
	 * @param verbose_log flag if differences should be logged in detail
	 * @return true if states are equal
	 */
	virtual bool equalBackupStates(const string& lhs, const string& rhs,
				       bool verbose_log) const = 0;

	/**
	 * Remove existing backup state
	 *
	 * @param name name of backup to remove, empty string means to remove
	 *     all existing backup states
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeBackupState( const string& name ) = 0;

	/**
	 * Determine if the given device is known and mounted somewhere
	 *
	 * @param device device name to check (checks also all alias names)
	 * @param mp set to current mount point if mounted
	 * @return bool that is true if device is mounted
	 */
	virtual bool checkDeviceMounted( const string& device, string& mp ) = 0;

	/**
	 * Umount the given device and do what is necessary to remove
	 * underlying volume (e.g. do losetup -d if loop is set up)
	 *
	 * The function umounts at once, /etc/fstab is unaffected
	 *
	 * @param device device name to umount
	 * @return bool if umount succeeded
	 */
	virtual bool umountDevice( const string& device ) = 0;

	/**
	 * Mount the given device and do what is necessary to access
	 * volume (e.g. do losetup if loop is set up)
	 *
	 * The function mounts at once, /etc/fstab is unaffected
	 *
	 * @param device device name to mount
	 * @param mp mount point to mount to
	 * @return bool if mount succeeded
	 */
	virtual bool mountDevice( const string& device, const string& mp ) = 0;

	/**
	 * Mount the given device with given options and do what is necessary
	 * to access volume (e.g. do losetup if loop is set up)
	 *
	 * The function mounts at once, /etc/fstab is unaffected
	 *
	 * @param device device name to mount
	 * @param mp mount point to mount to
	 * @param opts options to use for mount
	 * @return bool if mount succeeded
	 */
	virtual bool mountDeviceOpts( const string& device, const string& mp,
	                              const string& opts ) = 0;

	/**
	 * Mount the given device readonly and do what is necessary to access
	 * volume (e.g. do losetup if loop is set up)
	 *
	 * The function mounts at once, /etc/fstab is unaffected
	 *
	 * @param device device name to mount
	 * @param mp mount point to mount to
	 * @param opts options to use for mount
	 * @return bool if mount succeeded
	 */
	virtual bool mountDeviceRo( const string& device, const string& mp,
	                            const string& opts ) = 0;

	/**
	 * Check if there are dm maps to a given device
	 *
	 * @param device device name for which dm maps should be checked
	 * @return bool true if there are map to device
	 */
	virtual bool checkDmMapsTo( const string& device ) = 0;

	/**
	 * Remove all possibly existing dm maps to a given device
	 *
	 * @param device device name for which dm maps should be removed
	 */
	virtual void removeDmTableTo( const string& device ) = 0;

	/**
	 * Detect potentially available free space on a partition
	 *
	 * @param device device to check
	 * @param resize_free free space in kilobytes available for resize
	 * @param df_free free space in kilobytes available in filesystem
	 * @param used used space in kilobytes for filesystem
	 * @param win flag if partition contains a windows installation
	 * @param use_cache function should return cached data if available
	 * @return bool if values could be successfully determined
	 */
	virtual bool getFreeInfo( const string& device,
	                          unsigned long long& resize_free,
	                          unsigned long long& df_free,
	                          unsigned long long& used,
				  bool& win, bool& efi, bool use_cache ) = 0;

	/**
	 * Read fstab and cryptotab, if existent, from a specified directory and
	 * return the volumes found in fstab and cryptotab
	 *
	 * @param dir directory where fstab and cryptotab are read from
	 * @param infos list of records that get filled with volume info
	 */
	virtual bool readFstab( const string& dir, deque<VolumeInfo>& infos) = 0;

	/**
	 * Activate or deactivate higher level devices as MD, LVM, DM
	 *
	 * Multipath is not activate by this function.
	 *
	 * @param val flag if devices should be activated or deactivated
	 * @return bool if values could be successfully determined
	 */
	virtual void activateHld( bool val ) = 0;

	/**
	 * Activate or deactivate multipath
	 *
	 * @param val flag if multipath should be activated or deactivated
	 * @return bool if values could be successfully determined
	 */
	virtual void activateMultipath( bool val ) = 0;

	/**
	 * Rescan all disks.
	 * All currently detected objects are forgotten and a new scan
	 * for all type of objects (disks, LVM, MD) is initiated.
	 * This function makes sense to be called after something outside
	 * of libstorage changed disk layout or created storage objects.
	 * Any changes already cached are lost.
	 */
	virtual void rescanEverything() = 0;

	/**
	 * Dump list of all objects to log file.
	 */
	virtual void dumpObjectList() = 0;

	/**
	 * Split volume device name up into container name and a volume
	 * name. For Containers where this is appropriate (e.g. disks,
	 * MD, loop) also a volume number is provided.
	 *
	 * @param dev device name of volume, e.g. /dev/hda1
	 * @param info record that get filled with split data
	 * @return zero if all is ok, negative number to indicate an error
	 */
	virtual int getContVolInfo( const string& dev, ContVolInfo& info) = 0;

    };


    /**
     * Initializes default logging.
     */
    void initDefaultLogger ();


    /**
     * Factory for creating a concrete StorageInterface object.
     *
     * Throws an exception when locking fails.
     */
    StorageInterface* createDefaultStorageInterface ();


    /**
     * Factory for creating a concrete StorageInterface object.
     *
     * Throws an exception when locking fails.
     */
    StorageInterface* createStorageInterface (bool readonly, bool testmode,
					      bool autodetect);


    /**
     * Factory for creating a concrete StorageInterface object.
     *
     * Returns NULL if locking failed. In that case locker_pid is set to the
     * pid of one process holding a conflicting lock. If the pid cannot be
     * determined it is set to 0. The lock holder may run on another system.
     */
    StorageInterface* createStorageInterfacePid (bool readonly, bool testmode,
						 bool autodetect, int& locker_pid);


    /**
     * Destroy a StorageInterface object.
     */
    void destroyStorageInterface (StorageInterface*);

}


#endif
