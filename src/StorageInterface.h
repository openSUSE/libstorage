#ifndef STORAGEINTERFACE_H
#define STORAGEINTERFACE_H


#include <string>
#include <list>

using std::string;
using std::list;


/**
 * \brief Abstract class defining the interface for libstorage.
 */
class StorageInterface
{
public:


    StorageInterface () {};
    virtual ~StorageInterface () {};


    enum FsType { UNKNOWN, REISERFS, EXT2, EXT3, VFAT, XFS, JFS, NTFS, SWAP };


    /**
     * Contains capabilities of a filesystem type.
     */
    struct FsCapabilities
    {
	bool extend;
	bool mount_extend;
	bool shrink;
	bool mount_shrink;
	bool uuid;
	bool label;
	int label_length;
    };


    enum PartitionType { PRIMARY, EXTENDED, LOGICAL };


    /**
     * Contains info about a partition.
     */
    struct PartitionInfo
    {
	string name;
	long size;
	FsType fstype;
    };


    /**
     * Query all attatched disks.
     */
    virtual bool getDisks (list<string>& disks) = 0;

#if 0

    /**
     * Query partitions on all attatched disks.
     */
    virtual bool getPartitions (list<PartitionInfo>& partitioninfos) = 0;


    /**
     * Query partitions on a single disks.
     */
    virtual bool getPartitions (string disk, list<PartitionInfo>& partitioninfos) = 0;


    /**
     * Quert capabilities of a filesystem type.
     */
    virtual bool getFsCapabilities (FsType fstype, FsCapabilities& fscapabilities);


    /**
     * Create a new partition.
     *
     * @param disk name of disk, e.g. hda
     * @param type type of partition to create, e.g. primary or extended
     * @param size size of partition
     * @param number gets assigned to the number of the new partition
     */
    virtual bool createPartition (string disk, PartitionType type, long size,
				  int& number) = 0;

    /**
     *
     */
    virtual bool removePartition (string name) = 0;

    /**
     *
     */
    virtual bool resizePartition (string name, long size) = 0;

    /**
     *  Destroys the partition table of a disk.
     */
    virtual bool destroyPartitionTable (string disk) = 0;

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

#endif

};


#endif
