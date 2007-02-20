# AUFS Makefile for the Linux 2.6.16 and later
# $Id: Makefile,v 1.23 2007/02/19 03:25:37 sfjro Exp $

obj-$(CONFIG_AUFS) += aufs.o
aufs-y := module.o super.o sbinfo.o xino.o branch.o opts.o \
	cpup.o whout.o wkq.o \
	dentry.o dinfo.o \
	file.o f_op.o finfo.o \
	dir.o vdir.o \
	inode.o i_op.o i_op_add.o i_op_del.o i_op_ren.o iinfo.o \
	misc.o
#xattr.o
#aufs=$(CONFIG_SYSFS) += kobj.o
aufs-$(CONFIG_AUFS_HINOTIFY) += hinotify.o
#aufs-$(CONFIG_AUFS_EXPORT) += export.o
#aufs-$(CONFIG_AUFS_IPRIV_PATCH) += hipriv.o
#aufs-$(CONFIG_DEBUGFS) += dbgfs.o
aufs-$(CONFIG_AUFS_DEBUG) += debug.o
#aufs-y += $(aufs-m)
