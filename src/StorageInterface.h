#ifndef STORAGEINTERFACE_H
#define STORAGEINTERFACE_H


#include <string>
#include <ostream>
#include <list>

using std::string;
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
 * \section Example
 *
 * Here is a simple example to demonstrate the usage of libstorage:
 *
 * \code
 *
 * #include <y2storage/StorageInterface.h>
 *
 * using namespace std;
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
    enum FsType { FSUNKNOWN, REISERFS, EXT2, EXT3, VFAT, XFS, JFS, NTFS, SWAP };

    enum PartitionType { PRIMARY, EXTENDED, LOGICAL };

    enum MountByType { MOUNTBY_DEVICE, MOUNTBY_UUID, MOUNTBY_LABEL };

    enum EncryptType { ENC_NONE, ENC_TWOFISH, ENC_TWOFISH_OLD, ENC_TWOFISH256_OLD, ENC_UNKNOWN };

    enum MdType { RAID0, RAID1, RAID5, MULTIPATH };

    /**
     *  typedef for a pointer to a function that is called on progress bar events
     */
    typedef void (*CallbackProgressBar)( const string& id, unsigned cur, unsigned max );

    /**
     *  typedef for a pointer to a function that is called with strings telling the user what is
        currently going on
     */
    typedef void (*CallbackShowInstallInfo)( const string& id );


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
	unsigned labelLength;
	unsigned long long minimalFsSizeK;
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
	DISK_CHANGE_READONLY = -1021,
	DISK_RESIZE_PARTITION_INVALID_VOLUME = -1022,
	DISK_RESIZE_PARTITION_PARTED_FAILED = -1023,
	DISK_RESIZE_PARTITION_NOT_FOUND = -1024,
	DISK_RESIZE_NO_SPACE = -1025,
	DISK_CHECK_PARTITION_INVALID_VOLUME = -1026,

	STORAGE_DISK_NOT_FOUND = -2000,
	STORAGE_VOLUME_NOT_FOUND = -2001,
	STORAGE_REMOVE_PARTITION_INVALID_CONTAINER = -2002,
	STORAGE_CHANGE_PARTITION_ID_INVALID_CONTAINER = -2003,
	STORAGE_CHANGE_READONLY = -2004,

	VOLUME_COMMIT_UNKNOWN_STAGE = -3000,
	VOLUME_FSTAB_EMPTY_MOUNT = -3001,
	VOLUME_UMOUNT_FAILED = -3002,
	VOLUME_MOUNT_FAILED = -3003,
	VOLUME_FORMAT_DD_FAILED = -3004,
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
	VOLUME_MOUNT_POINT_INAVLID = -3019,
	VOLUME_MOUNTBY_NOT_ENCRYPTED = -3020,
	VOLUME_MOUNTBY_UNSUPPORTED_BY_FS = -3021,
	VOLUME_LABEL_NOT_SUPPORTED = -3022,
	VOLUME_LABEL_TOO_LONG = -3023,
	VOLUME_LABEL_WHILE_MOUNTED = -3024,
	VOLUME_RESIZE_UNSUPPORTED_BY_FS = -3025,
	VOLUME_RESIZE_UNSUPPORTED_BY_VOLUME = -3026,
	VOLUME_RESIZE_FAILED = -3027,

	CONTAINER_INTERNAL_ERROR = -4000,

	FSTAB_ENTRY_NOT_FOUND = -5000,
	FSTAB_CHANGE_PREFIX_IMPOSSIBLE = -5001,
	FSTAB_REMOVE_ENTRY_NOT_FOUND = -5002,
	FSTAB_UPDATE_ENTRY_NOT_FOUND = -5003,
	FSTAB_ADD_ENTRY_FOUND = -5004

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
	 * Query capabilities of a filesystem type.
	 */
	virtual bool getFsCapabilities (FsType fstype, FsCapabilities& fscapabilities) = 0;

	/**
	 * Print detected entities on a stream. 
	 * Exact output format may change between releses.
	 * Function mainly meant for debugging purposes.
	 *
	 * @param str stream to print data to
	 */
	virtual void printInfo( std::ostream& str ) = 0;

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
	 * @return zero if all is ok, a negative number to indicate an error
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
	 * @return zero if all is ok, a negative number to indicate an error
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
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removePartition (const string& partition) = 0;


	/**
	 * Change partition id of a partition
	 *
	 * @param partition name of partition, e.g. /dev/hda1
	 * @param new partition id (e.g. 0x82 swap, 0x8e for lvm, ...)
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changePartitionId (const string& partition, unsigned id) = 0;

	/**
	 *  Destroys the partition table of a disk. An empty disk label
	 *  of the given type without any partition is created.
	 *
	 * @param disk device name of disk, e.g. /dev/hda
	 * @param label disk label to create on disk , e.g. msdos, gpt, ...
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int destroyPartitionTable (const string& disk, const string& label) = 0;

	/**
	 *  Returns the default disk label of the architecture of the
	 *  machine (e.g. msdos for ix86, gpt for ia64, ...)
	 *
	 */
	virtual string defaultDiskLabel() = 0;

	/**
	 *  sets or unsets the format flag for the given volume.
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param format flag if format is set on or off
	 * @param fs type of filesystem to create if fromat is true
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeFormatVolume( const string& device, bool format, FsType fs ) = 0;

	/**
	 *  changes the mount point of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param mount new mount point of the volume (e.g. /home).
	 *    it is valid to set an empty mount point
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeMountPoint( const string& device, const string& mount ) = 0;

	/**
	 *  get the mount point of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param mount will be set to the mount point of the volume (e.g. /home).
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getMountPoint( const string& device, string& mount ) = 0;

	/**
	 *  changes mount by value in fstab of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param options new mount by value of the volume.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeMountBy( const string& device, MountByType mby ) = 0;

	/**
	 *  get mount by value in fstab of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param options will be set to the mount by value of the volume.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getMountBy( const string& device, MountByType& mby ) = 0;

	/**
	 *  changes the fstab options of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param options new fstab options of the volume (e.g. noauto,user,sync).
	 *    Multiple options are separated by ",".
	 *    It is valid to set an empty fstab option.
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int changeFstabOptions( const string& device, const string& options ) = 0;

	/**
	 *  get the fstab options of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param options will be set to the fstab options of the volume (e.g. noauto,user,sync).
	 *    Multiple options are separated by ",".
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getFstabOptions( const string& device, string& options ) = 0;

	/**
	 *  adds to the fstab options of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param options fstab options to add to already exiting options of the volume (e.g. noauto,user,sync).
	 *    Multiple options are separated by ",".
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int addFstabOptions( const string& device, const string& options ) = 0;

	/**
	 *  remove from the fstab options of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param options fstab options to remove from already existing options of the volume (e.g. noauto).
	 *    Multiple options are separated by ",".
	 *    It is possible to specify wildcards, so "uid=.*" matches every option starting with the string "uid=".
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int removeFstabOptions( const string& device, const string& options ) = 0;

	/**
	 *  set crypt password of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param pwd crypt password fro this volume
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int setCryptPassword( const string& device, const string& pwd ) = 0;

	/**
	 *  set crypt password of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param val flag if encryption should be activated
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int setCrypt( const string& device, bool val ) = 0;

	/**
	 *  get crypt password of a volume
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param val will be set if encryption is activated
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int getCrypt( const string& device, bool& val ) = 0;

	/**
	 *  resizes a volume while keeping the data on the filesystem
	 *
	 * @param device name of volume, e.g. /dev/hda1
	 * @param newSizeMb new size desired volume in Megabyte
	 * @return zero if all is ok, a negative number to indicate an error
	 */
	virtual int resizeVolume( const string& device, unsigned long long newSizeMb ) = 0;

	/**
	 *  gets a list of string describing the actions to be executed
	 *  after next call to commit()
	 *
	 * @param mark_destructive if true use <red> around </red> destructive
	 *    actions (like e.g. deletion, formatting, ...)
	 * @return list of strings presentable to the user
	 */
	virtual list<string> getCommitActions( bool mark_destructive ) = 0;

	/**
	 *  sets the callback function called on progress bar events
	 *
	 * @param pfnc pointer to funtcion
	 */
	virtual void setCallbackProgressBar( CallbackProgressBar pfnc ) = 0;

	/**
	 *  query the callback function called on progress bar events
	 *
	 * @return pointer to function currently called for progress bar events
	 */
	virtual CallbackProgressBar getCallbackProgressBar() = 0;

	/**
	 *  sets the callback function called to display install info
	 *
	 * @param pfnc pointer to funtcion
	 */
	virtual void setCallbackShowInstallInfo( CallbackShowInstallInfo pfnc ) = 0;

	/**
	 *  query the callback function called to display install info
	 *
	 * @return pointer to function currently called for progress bar events
	 */
	virtual CallbackShowInstallInfo getCallbackShowInstallInfo() = 0;
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
	 * With the function setCacheChanges you can turn the caching mode on
	 * and off.  Turning of caching mode will cause all changes done so
	 * far to be commited upto the next modifying function.
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

    };


    /**
     * Factory for creating a concrete StorageInterface object.
     */
    StorageInterface* createStorageInterface (bool ronly = false,
					      bool testmode = false,
					      bool autodetect = true);


};


#endif
