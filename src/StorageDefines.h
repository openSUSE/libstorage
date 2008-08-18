#ifndef STORAGE_DEFINES_H
#define STORAGE_DEFINES_H


#define PARTEDBIN "/usr/sbin/parted"
#define PARTEDCMD PARTEDBIN " -s "	// blank at end !!

#define FDISKBIN "/sbin/fdisk"

#define MDADMBIN "/sbin/mdadm"

#define PVCREATEBIN "/sbin/pvcreate"

#define LVCREATEBIN "/sbin/lvcreate"
#define LVREMOVEBIN "/sbin/lvremove"
#define LVEXTENDBIN "/sbin/lvextend"
#define LVREDUCEBIN "/sbin/lvreduce"
#define LVSBIN "/sbin/lvs"

#define VGCREATEBIN "/sbin/vgcreate"
#define VGREMOVEBIN "/sbin/vgremove"
#define VGEXTENDBIN "/sbin/vgextend"
#define VGREDUCEBIN "/sbin/vgreduce"
#define VGDISPLAYBIN "/sbin/vgdisplay"
#define VGSCANBIN "/sbin/vgscan"
#define VGCHANGEBIN "/sbin/vgchange"

#define CRYPTSETUPBIN "/sbin/cryptsetup"
#define LOSETUPBIN "/sbin/losetup"
#define MULTIPATHBIN "/sbin/multipath"
#define DMSETUPBIN "/sbin/dmsetup"
#define KPARTXBIN "/sbin/kpartx"
#define DMRAIDBIN "/sbin/dmraid"

#define MOUNTBIN "/bin/mount"
#define UMOUNTBIN "/bin/umount"
#define SWAPONBIN "/sbin/swapon"
#define SWAPOFFBIN "/sbin/swapoff"

#define DDBIN "/bin/dd"

#define DASDFMTBIN "/sbin/dasdfmt"
#define DASDVIEWBIN "/sbin/dasdview"
#define FDASDBIN "/sbin/fdasd"

#define UDEVSETTLEBIN "/sbin/udevsettle"


#endif
