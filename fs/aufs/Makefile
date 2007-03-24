# AUFS Makefile for the Linux 2.6.16 and later
# $Id: Makefile,v 1.26 2007/03/19 04:29:32 sfjro Exp $

obj-$(CONFIG_AUFS) += aufs.o
aufs-y := module.o super.o sbinfo.o xino.o \
	branch.o vfsub.o cpup.o whout.o wkq.o \
	opts.o \
	dentry.o dinfo.o \
	file.o f_op.o finfo.o \
	dir.o vdir.o \
	inode.o i_op.o i_op_add.o i_op_del.o i_op_ren.o iinfo.o \
	misc.o
#xattr.o
aufs-$(CONFIG_AUFS_SYSAUFS) += sysaufs.o
aufs-$(CONFIG_AUFS_HINOTIFY) += hinotify.o
aufs-$(CONFIG_AUFS_EXPORT) += export.o
#aufs-$(CONFIG_DEBUGFS) += dbgfs.o
aufs-$(CONFIG_AUFS_DEBUG) += debug.o
