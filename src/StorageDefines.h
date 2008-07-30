#ifndef STORAGE_DEFINES_H
#define STORAGE_DEFINES_H


#define PARTEDBIN "/usr/sbin/parted"
#define PARTEDCMD PARTEDBIN " -s "	// blank at end !!

#define MDADMBIN "/sbin/mdadm"

#define PVCREATEBIN "/sbin/pvcreate"

#define LVCREATEBIN "/sbin/lvcreate"
#define LVREMOVEBIN "/sbin/lvremove"

#define VGCREATEBIN "/sbin/vgcreate"
#define VGREMOVEBIN "/sbin/vgremove"
#define VGDISPLAYBIN "/sbin/vgdisplay"


#endif
