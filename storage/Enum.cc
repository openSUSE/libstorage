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


#include "storage/Enum.h"
#include "storage/StorageTmpl.h"


namespace storage
{

    // strings must match /etc/fstab
    static const string fs_type_names[] = {
	"unknown", "reiserfs", "ext2", "ext3", "ext4", "btrfs", "vfat", "xfs", "jfs", "hfs",
	"ntfs-3g", "swap", "hfsplus", "nfs", "nfs4", "tmpfs", "none"
    };

    const vector<string> EnumInfo<FsType>::names(fs_type_names, fs_type_names +
						 lengthof(fs_type_names));


    static const string partition_type_names[] = {
	"primary", "extended", "logical", "any"
    };

    const vector<string> EnumInfo<PartitionType>::names(partition_type_names, partition_type_names +
							lengthof(partition_type_names));


    static const string mount_by_names[] = {
	"device", "uuid", "label", "id", "path"
    };

    const vector<string> EnumInfo<MountByType>::names(mount_by_names, mount_by_names +
						      lengthof(mount_by_names));


    static const string encrypt_names[] = {
	"none", "twofish256", "twofish", "twofishSL92", "luks", "unknown"
    };

    const vector<string> EnumInfo<EncryptType>::names(encrypt_names, encrypt_names +
						      lengthof(encrypt_names));


    // strings must match /proc/mdstat
    static const string md_type_names[] = {
	"unknown", "raid0", "raid1", "raid5", "raid6", "raid10", "multipath"
    };

    const vector<string> EnumInfo<MdType>::names(md_type_names, md_type_names +
						 lengthof(md_type_names));


    // strings must match "mdadm --parity" option
    static const string md_parity_names[] = {
	"default", "left-asymmetric", "left-symmetric", "right-asymmetric", "right-symmetric",
	"parity-first", "parity-last",
	"left-asymmetric-6", "left-symmetric-6", "right-asymmetric-6", "right-symmetric-6",
	"parity-first-6",
	"n2", "o2", "f2", "n3", "o3", "f3"
    };

    const vector<string> EnumInfo<MdParity>::names(md_parity_names, md_parity_names +
						   lengthof(md_parity_names));


    // strings must match /sys/block/md*/md/array_state
    static const string md_array_state_names[] = {
	"unknown", "clear", "inactive", "suspended", "readonly", "read-auto", "clean", "active",
	"write-pending", "active-idle"
    };

    const vector<string> EnumInfo<MdArrayState>::names(md_array_state_names, md_array_state_names +
						       lengthof(md_array_state_names));


    static const string used_by_type_names[] = {
	"NONE", "LVM", "MD", "MDPART", "DM", "DMRAID", "DMMULTIPATH", "BTRFS"
    };

    const vector<string> EnumInfo<UsedByType>::names(used_by_type_names, used_by_type_names +
						     lengthof(used_by_type_names));


    static const string c_type_names[] = {
	"UNKNOWN", "DISK", "MD", "LOOP", "LVM", "DM", "DMRAID", "NFS", "DMMULTIPATH", "MDPART", "BTRFS", "TMPFS"
    };

    const vector<string> EnumInfo<CType>::names(c_type_names, c_type_names +
						lengthof(c_type_names));


    static const string transport_names[] = {
	"UNKNOWN", "SBP", "ATA", "FC", "iSCSI", "SAS", "SATA", "SPI", "USB"
    };

    const vector<string> EnumInfo<Transport>::names(transport_names, transport_names +
						    lengthof(transport_names));


    static const string imsm_driver_names[] = {
	"UNDECIDED", "DMRAID", "MDADM"
    };

    const vector<string> EnumInfo<ImsmDriver>::names(imsm_driver_names, imsm_driver_names +
						     lengthof(imsm_driver_names));


    // strings must match "parted --align" option
    static const string part_align_names[] = {
	"optimal", "cylinder"
    };

    const vector<string> EnumInfo<PartAlign>::names(part_align_names, part_align_names +
						    lengthof(part_align_names));

}
