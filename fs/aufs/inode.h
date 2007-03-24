/*
 * Copyright (C) 2005, 2006, 2007 Junjiro Okajima
 *
 * This program, aufs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* $Id: inode.h,v 1.25 2007/03/19 04:31:31 sfjro Exp $ */

#ifndef __AUFS_INODE_H__
#define __AUFS_INODE_H__

#include <linux/fs.h>
#include <linux/inotify.h>
#include <linux/version.h>
#include <linux/aufs_type.h>
#include "misc.h"
//#include "super.h"

#ifdef __KERNEL__

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#else
struct inotify_watch {};
#endif

struct aufs_hinotify {
	struct inotify_watch	hin_watch;
	struct inode		*hin_aufs_inode;	// no get/put
};

struct aufs_hinode {
	struct inode		*hi_inode;
	//unsigned int		hi_id;
	aufs_bindex_t		hi_id;
	struct aufs_hinotify	*hi_notify;
};

struct aufs_vdir;
struct aufs_iinfo {
	struct aufs_rwsem	ii_rwsem;
	aufs_bindex_t		ii_bstart, ii_bend;
	struct aufs_hinode	*ii_hinode;
	struct aufs_vdir	*ii_vdir; // i_mapping and page cache?
};

struct aufs_icntnr {
	struct aufs_iinfo iinfo;
	struct inode vfs_inode;
};

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,15)
#define i_lock(i)		down(&(i)->i_sem)
#define i_unlock(i)		up(&(i)->i_sem)
#define IMustLock(i)		DEBUG_ON(!down_trylock(&(i)->i_sem))
#define i_lock_new(i)		i_lock(i)
#define hi_lock(i, lsc)		i_lock(i)
#else
#define i_lock(i)		mutex_lock(&(i)->i_mutex)
#define i_unlock(i)		mutex_unlock(&(i)->i_mutex)
#define IMustLock(i)		MtxMustLock(&(i)->i_mutex)
#define i_lock_new(i) \
	mutex_lock_nested(&(i)->i_mutex, AUFS_LSC_INODE_NEW)
#define hi_lock(i, lsc)		mutex_lock_nested(&(i)->i_mutex, lsc)
#endif

#define hi_lock_gparent(h_i)	hi_lock(h_i, AUFS_LSC_H_GPARENT)
#define hi_lock_parent(h_i)	hi_lock(h_i, AUFS_LSC_H_PARENT)
#define hi_lock_parent2(h_i)	hi_lock(h_i, AUFS_LSC_H_PARENT2)
#define hi_lock_child(h_i)	hi_lock(h_i, AUFS_LSC_H_CHILD)
#define hi_lock_child2(h_i)	hi_lock(h_i, AUFS_LSC_H_CHILD2)

#define ii_read_lock_child(i) \
	rw_read_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_CHILD)
#define ii_write_lock_child(i) \
	rw_write_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_CHILD)

#define ii_read_lock_child2(i) \
	rw_read_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_CHILD2)
#define ii_write_lock_child2(i) \
	rw_write_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_CHILD2)

#define ii_read_lock_child3(i) \
	rw_read_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_CHILD3)
#define ii_write_lock_child3(i) \
	rw_write_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_CHILD3)

#define ii_read_lock_parent(i) \
	rw_read_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_PARENT)
#define ii_write_lock_parent(i) \
	rw_write_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_PARENT)

#define ii_read_lock_parent2(i) \
	rw_read_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_PARENT2)
#define ii_write_lock_parent2(i) \
	rw_write_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_PARENT2)

#define ii_read_lock_parent3(i) \
	rw_read_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_PARENT3)
#define ii_write_lock_parent3(i) \
	rw_write_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_PARENT3)

#define ii_read_lock_new(i) \
	rw_read_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_NEW)
#define ii_write_lock_new(i) \
	rw_write_lock(&itoii(i)->ii_rwsem, AUFS_LSC_IINFO_NEW)

#define ii_read_unlock(i)	rw_read_unlock(&itoii(i)->ii_rwsem)
#define ii_downgrade_lock(i)	rw_dgrade_lock(&itoii(i)->ii_rwsem)
#define ii_write_unlock(i)	rw_write_unlock(&itoii(i)->ii_rwsem)

/* debug macro. use with caution */
#define IiMustReadLock(i)	do { \
	SiMustAnyLock((i)->i_sb); \
	RwMustReadLock(&itoii(i)->ii_rwsem); \
	} while(0)
#define IiMustWriteLock(i)	do { \
	SiMustAnyLock((i)->i_sb); \
	RwMustWriteLock(&itoii(i)->ii_rwsem); \
	} while(0)
#define IiMustAnyLock(i)	do { \
	SiMustAnyLock((i)->i_sb); \
	RwMustAnyLock(&itoii(i)->ii_rwsem); \
	} while(0)

#define IiMustNoWaiters(i)	RwMustNoWaiters(&itoii(i)->ii_rwsem)

/* ---------------------------------------------------------------------- */

//inode.c
int au_refresh_hinode(struct dentry *dentry);
struct inode *aufs_new_inode(struct dentry *dentry);

#ifdef CONFIG_AUFS_HINOTIFY
// hinotify.c
int alloc_hinotify(struct aufs_hinode *hinode, struct inode *inode,
		   struct inode *h_inode);
void do_free_hinotify(struct aufs_hinode *hinode);
void do_hdir_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex,
		  int lsc);
#define hgdir_lock(h_dir, dir, bindex) \
	do_hdir_lock(h_dir, dir, bindex, AUFS_LSC_H_GPARENT)
#define hdir_lock(h_dir, dir, bindex) \
	do_hdir_lock(h_dir, dir, bindex, AUFS_LSC_H_PARENT)
#define hdir2_lock(h_dir, dir, bindex) \
	do_hdir_lock(h_dir, dir, bindex, AUFS_LSC_H_PARENT2)
void hdir_unlock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex);
void hdir_lock_rename(struct dentry **h_parents, struct inode **dirs,
		      aufs_bindex_t bindex, int issamedir);
void hdir_unlock_rename(struct dentry **h_parents, struct inode **dirs,
			aufs_bindex_t bindex, int issamedir);
void au_reset_hinotify(struct inode *inode, unsigned int flags);
int __init aufs_inotify_init(void);
void __exit aufs_inotify_exit(void);
#else
#define alloc_hinotify(hi, i, h_i)	-EOPNOTSUPP
#define do_free_hinotify(hi)		/* */
static inline void
hgdir_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex)
{
	hi_lock_gparent(h_dir);
}
static inline void
hdir_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex)
{
	hi_lock_parent(h_dir);
}
static inline void
hdir2_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex)
{
	hi_lock_parent2(h_dir);
}
#define hdir_unlock(h_d, d, i)		i_unlock(h_d)
#define hdir_lock_rename(h_p, d, i, s)	vfsub_lock_rename(h_p[0], h_p[1])
#define hdir_unlock_rename(h_p, d, i, s) vfsub_unlock_rename(h_p[0], h_p[1])
#define au_reset_hinotify(i, f)		({i;})
#define aufs_inotify_init()		0
#define aufs_inotify_exit()		/* */
#endif /* CONFIG_AUFS_HINOTIFY */

#define free_hinotify(inode, bindex) \
	do_free_hinotify(itoii(inode)->ii_hinode + bindex)

//i_op.c
extern struct inode_operations aufs_iop, aufs_symlink_iop, aufs_dir_iop;
int wr_dir(struct dentry *dentry, int negative, struct dentry *src_dentry,
	   aufs_bindex_t force_btgt, int do_lock_srcdir);

//i_op_add.c
int aufs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev);
int aufs_symlink(struct inode *dir, struct dentry *dentry, const char *symname);
int aufs_create(struct inode *dir, struct dentry *dentry, int mode,
		struct nameidata *nd);
int aufs_link(struct dentry *src_dentry, struct inode *dir,
	      struct dentry *dentry);
int aufs_mkdir(struct inode *dir, struct dentry *dentry, int mode);

//i_op_del.c
int wr_dir_need_wh(struct dentry *dentry, int isdir, aufs_bindex_t *bcpup,
		   struct dentry *locked);
int aufs_unlink(struct inode *dir, struct dentry *dentry);
int aufs_rmdir(struct inode *dir, struct dentry *dentry);

// i_op_ren.c
int aufs_rename(struct inode *src_dir, struct dentry *src_dentry,
		struct inode *dir, struct dentry *dentry);

//iinfo.c
struct aufs_iinfo *itoii(struct inode *inode);
aufs_bindex_t ibstart(struct inode *inode);
aufs_bindex_t ibend(struct inode *inode);
struct aufs_vdir *ivdir(struct inode *inode);
struct inode *h_iptr_i(struct inode *inode, aufs_bindex_t bindex);
struct inode *h_iptr(struct inode *inode);
unsigned int itoid_index(struct inode *inode, aufs_bindex_t bindex);

void set_ibstart(struct inode *inode, aufs_bindex_t bindex);
void set_ibend(struct inode *inode, aufs_bindex_t bindex);
void set_ivdir(struct inode *inode, struct aufs_vdir *vdir);
void aufs_hiput(struct aufs_hinode *hinode);
#define AUFS_HI_XINO	1
#define AUFS_HI_NOTIFY	2
unsigned int au_hi_flags(struct inode *inode, int isdir);
void set_h_iptr(struct inode *inode, aufs_bindex_t bindex,
		struct inode *h_inode, unsigned int flags);
void au_update_brange(struct inode *inode, int do_put_zero);

int au_iinfo_init(struct inode *inode);
void au_iinfo_fin(struct inode *inode);

#endif /* __KERNEL__ */
#endif /* __AUFS_INODE_H__ */
