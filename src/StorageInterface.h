#ifndef STORAGEINTERFACE_H
#define STORAGEINTERFACE_H


#include <string>
#include <list>

using std::string;
using std::list;


namespace storage
{
    enum FsType { UNKNOWN, REISERFS, EXT2, EXT3, VFAT, XFS, JFS, NTFS, SWAP };

    enum PartitionType { PRIMARY, EXTENDED, LOGICAL };

    enum MountByType { MOUNTBY_DEVICE, MOUNTBY_UUID, MOUNTBY_LABEL };

    enum EncryptType { ENC_NONE, ENC_TWOFISH, ENC_TWOFISH_OLD, ENC_UNKNOWN };

    enum MdType { RAID0, RAID1, RAID5, MULTIPATH };


    /**
     * Contains capabilities of a filesystem type.
     */
    struct FsCapabilities
    {
	bool isExtendable;
	bool isExtendableWhileMounted;
	bool isReduceable;
	bool isReduceableWhileMounted;
	bool supportsUuid;
	bool supportsLabel;
	bool labelWhileMounted;
	int labelLength;
    };


    /**
     * Contains info about a partition.
     */
    struct PartitionInfo
    {
	string name;
	unsigned long cylStart;
	unsigned long cylSize;
	PartitionType partitionType;
	FsType fsType;
    };

    /**
     * prelimiary list of error codes, must have negative values
     */
    enum ErrorCodes
    {
	DISK_CREATE_PARTITION_OVERLAPS_EXISTING = -1000,
	DISK_CREATE_PARTITION_EXCEEDS_DISK = -1001,
	DISK_CREATE_PARTITION_EXT_ONLY_ONCE = -1002,
	DISK_CREATE_PARTITION_EXT_IMPOSSIBLE = -1003,
	DISK_CREATE_PARTITION_NO_FREE_NUMBER = -1004,
	DISK_CREATE_PARTITION_INVALID_VOLUME = -1005,
	DISK_CREATE_PARTITION_INVALID_TYPE = -1006,
	DISK_CREATE_PARTITION_PARTED_FAILED = -1007,
	DISK_CREATE_PARTITION_NOT_FOUND = -1008,
	DISK_CREATE_PARTITION_LOGICAL_NO_EXT = -1009,
	DISK_CREATE_PARTITION_LOGICAL_OUTSIDE_EXT = -1010,
	DISK_SET_TYPE_INVALID_VOLUME = -1011,
	DISK_SET_TYPE_PARTED_FAILED = -1012,
	DISK_SET_LABEL_PARTED_FAILED = -1013,
	DISK_REMOVE_PARTITION_NOT_FOUND = -1014,
	DISK_REMOVE_PARTITION_PARTED_FAILED = -1015,
	DISK_REMOVE_PARTITION_INVALID_VOLUME = -1016,
	DISK_REMOVE_PARTITION_LIST_ERASE = -1017,
	DISK_CHANGE_PARTITION_ID_NOT_FOUND = -1018,
	DISK_DESTROY_TABLE_INVALID_LABEL = -1019,
	DISK_CREATE_PARTITION_ZERO_SIZE = -1020,

	STORAGE_DISK_NOT_FOUND = -2000,
	STORAGE_VOLUME_NOT_FOUND = -2001,
	STORAGE_REMOVE_PARTITION_INVALID_CONTAINER = -2002,
	STORAGE_CHANGE_PARTITION_ID_INVALID_CONTAINER = -2003,

	VOLUME_COMMIT_UNKNOWN_STAGE = -3000,

	CONTAINER_INTERNAL_ERROR = -4000,
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
	 * Query all attatched disks.
	 */
	virtual bool getDisks (list<string>& disks) = 0;


	/**
	 * Query partitions on all attatched disks.
	 */
	virtual bool getPartitions (list<PartitionInfo>& partitioninfos) = 0;


	/**
	 * Query partitions on a single disks.
	 */
	virtual bool getPartitions (const string& disk, list<PartitionInfo>& partitioninfos) = 0;


	/**
	 * Quert capabilities of a filesystem type.
	 */
	virtual bool getFsCapabilities (FsType fstype, FsCapabilities& fscapabilities) = 0;


	/**
	 * Create a new partition. Units given in disk cylinders.
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param type type of partition to create, e.g. primary or extended
	 * @param start cylinder number of partition start (cylinders are numbered starting with 1)
	 * @param sizeCyl size of partition in disk cylinders
	 * @param device gets assigned to the device name of the new partition
	 * The name is returned instead of the number since creating the name from the
	 * number is not straight-forward.
	 */
	virtual int createPartition( const string& disk, PartitionType type,
				     unsigned long start,
	                             unsigned long sizeCyl,
				     string& device ) = 0;

	/**
	 * Create a new partition. Units given in Megabytes.
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param type type of partition to create, e.g. primary or extended
	 * @param start offset in kilobytes from start of disk
	 * @param size  size of partition in kilobytes
	 * @param device gets assigned to the device name of the new partition
	 * The name is returned instead of the number since creating the name from the
	 * number is not straight-forward.
	 */
	virtual int createPartitionKb( const string& disk, PartitionType type,
				       unsigned long long start,
				       unsigned long long size,
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
	 */
	virtual int removePartition (const string& partition) = 0;


	/**
	 * Change partition id of a partition
	 *
	 * @param partition name of partition, e.g. /dev/hda1
	 * @param new partition id (e.g. 0x82 swap, 0x8e for lvm, ...)
	 */
	virtual int changePartitionId (const string& partition, unsigned id) = 0;

	/**
	 *  Destroys the partition table of a disk. An empty disk label
	 *  of the given type without any partition is created.
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param label disk label to create on disk , e.g. msdos, gpt, ...
	 */
	virtual int destroyPartitionTable (const string& disk, const string& label) = 0;

	/**
	 *  Returns the default disk label of the architecture of the
	 *  machine (e.g. msdos for ix86, gpt for ia64, ...)
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param label disk label to create on disk , e.g. msdos, gpt, ...
	 */
	virtual string defaultDiskLabel() = 0;

#if 0
	/**
	 *
	 */
	virtual bool resizePartition (string partition, long size) = 0;


	virtual bool createMd (...) = 0;
	virtual bool removeMd (...) = 0;

	virtual bool createLvmVg (...) = 0;
	virtual bool removeLvmVg (...) = 0;
	virtual bool extendLvmVg (...) = 0;
	virtual bool reduceLvmVg (...) = 0;

	virtual bool createLvmLv (...) = 0;
	virtual bool removeLvmLv (...) = 0;
	virtual bool resizeLvmLv (...) = 0;

	virtual bool createLoopFile (...) = 0;
	virtual bool removeLoopFile (...) = 0;

	virtual bool createEvmsContainer (...) = 0;
	virtual bool reomveEvmsContainer (...) = 0;
	virtual bool extendEvmsContainer (...) = 0;
	virtual bool reduceEvmsContainer (...) = 0;

	virtual bool createEvmsVolume (...) = 0;
	virtual bool removeEvmsVolume (...) = 0;
	virtual bool resizeEvmsVolume (...) = 0;

	virtual bool formatVolume (...) = 0;

	virtual bool changeMountPoint (...) = 0;

	virtual bool changeFstabOptions (...) = 0;

	/**
	 * Create a backup of the current state.
	 */
	virtual bool createBackupState (string state) = 0;

	/**
	 * Restore a backup of the current state.
	 */
	virtual bool restoreBackupState (string state) = 0;

	/**
	 * Delete a backup of the current state.
	 */
	virtual bool removeBackupState (string state) = 0;

#endif

	/**
	 * Commit the current state to the system.
	 */
	virtual int commit() = 0;

    };


    StorageInterface* createStorageInterface (bool ronly = false,
					      bool testmode = false,
					      bool autodetect = true);


};


#endif
