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

/* $Id: inode.h,v 1.21 2007/02/19 03:28:38 sfjro Exp $ */

#ifndef __AUFS_INODE_H__
#define __AUFS_INODE_H__

#include <linux/fs.h>
#include <linux/inotify.h>
#include <linux/version.h>
#include <linux/aufs_type.h>
#include "misc.h"

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
#else
#define i_lock(i)		mutex_lock(&(i)->i_mutex)
#define i_unlock(i)		mutex_unlock(&(i)->i_mutex)
#define IMustLock(i)		DEBUG_ON(!mutex_is_locked(&(i)->i_mutex))
#endif

#define ii_read_lock(i)		rw_read_lock(&itoii(i)->ii_rwsem)
#define ii_read_unlock(i)	rw_read_unlock(&itoii(i)->ii_rwsem)
#define ii_downgrade_lock(i)	rw_dgrade_lock(&itoii(i)->ii_rwsem)
#define ii_write_lock(i)	rw_write_lock(&itoii(i)->ii_rwsem)
#define ii_write_unlock(i)	rw_write_unlock(&itoii(i)->ii_rwsem)
// remove this
#define ii_is_write_lock(i)	rw_is_write_lock(&itoii(i)->ii_rwsem)

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

/* ---------------------------------------------------------------------- */

//inode.c
int refresh_hinode(struct dentry *dentry);
struct inode *aufs_new_inode(struct dentry *dentry);

#if 0 //def CONFIG_AUFS_IPRIV_PATCH
//sec.c
struct aufs_h_ipriv {
	struct inode	*hi_inode;	/* does not iget/iput */
};
#define i_priv(i)	({i->i_state |= I_PRIVATE_TMP;})
#define i_unpriv(i)	({i->i_state &= ~I_PRIVATE_TMP;})
#define IMustPriv(i)	DEBUG_ON(!IS_PRIVATE(i))
void h_ipriv(struct inode *h_inode, struct aufs_h_ipriv *ipriv,
	     struct super_block *sb, int do_lock);
void h_iunpriv(struct aufs_h_ipriv *ipriv, int do_lock);
int hipriv_link(struct dentry *h_src_dentry, struct inode *h_dir,
		struct dentry *h_dentry, struct super_block *sb);
int hipriv_rename(struct inode *h_src_dir, struct dentry *h_src_dentry,
		  struct inode *h_dir, struct dentry *h_dentry,
		  struct super_block *sb);
int hipriv_rmdir(struct inode *h_dir, struct dentry *h_dentry,
		 struct super_block *sb);
int hipriv_unlink(struct inode *h_dir, struct dentry *h_dentry,
		  struct super_block *sb);
#else
struct aufs_h_ipriv {};
#define i_priv(i)		/* */
#define i_unpriv(i)		/* */
#define IMustPriv(i)		/* */
#define h_ipriv(hi,priv,sb,l)	({priv;})
#define h_iunpriv(priv,l)	/* */
#define hipriv_link(h_src_dentry, h_dir, h_dentry, sb) \
	vfs_link(h_src_dentry, h_dir, h_dentry)
#define hipriv_rename(h_src_dir, h_src_dentry, h_dir, h_dentry, sb) \
	vfs_rename(h_src_dir, h_src_dentry, h_dir, h_dentry)
#define hipriv_rmdir(h_dir, h_dentry, sb) \
	vfs_rmdir(h_dir, h_dentry)
#define hipriv_unlink(h_dir, h_dentry, sb) \
	safe_unlink(h_dir, h_dentry)
#endif

static inline void
i_lock_priv(struct inode *h_inode, struct aufs_h_ipriv *ipriv,
	    struct super_block *sb)
{
	i_lock(h_inode);
	h_ipriv(h_inode, ipriv, sb, /*do_lock*/0);
}

static inline void
i_unlock_priv(struct inode *h_inode, struct aufs_h_ipriv *ipriv)
{
	h_iunpriv(ipriv, /*do_lock*/0);
	i_unlock(h_inode);
}

static inline void
i_lock_rename_priv(struct dentry **h_parents, struct aufs_h_ipriv *iprivs,
		   struct super_block *sb)
{
	lock_rename(h_parents[0], h_parents[1]);
	h_ipriv(h_parents[0]->d_inode, iprivs + 0, sb, /*do_lock*/0);
	h_ipriv(h_parents[1]->d_inode, iprivs + 1, sb, /*do_lock*/0);
}

static inline void
i_unlock_rename_priv(struct dentry **h_parents, struct aufs_h_ipriv *iprivs)
{
	h_iunpriv(iprivs + 0, /*do_lock*/0);
	h_iunpriv(iprivs + 1, /*do_lock*/0);
	unlock_rename(h_parents[0], h_parents[1]);
}

#ifdef CONFIG_AUFS_HINOTIFY
// hinotify.c
int alloc_hinotify(struct aufs_hinode *hinode, struct inode *inode,
		   struct inode *h_inode);
void do_free_hinotify(struct aufs_hinode *hinode);
void hdir_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex,
	       struct aufs_h_ipriv *ipriv);
void hdir_unlock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex,
		 struct aufs_h_ipriv *ipriv);
void hdir_lock_rename(struct dentry **h_parents, struct inode **dirs,
		      aufs_bindex_t bindex, int issamedir,
		      struct aufs_h_ipriv *iprivs);
void hdir_unlock_rename(struct dentry **h_parents, struct inode **dirs,
			aufs_bindex_t bindex, int issamedir,
			struct aufs_h_ipriv *iprivs);
void reset_hinotify(struct inode *inode, unsigned int flags);
int __init aufs_inotify_init(void);
void __exit aufs_inotify_exit(void);
#else
#define alloc_hinotify(hi,i,h_i)	-EOPNOTSUPP
#define do_free_hinotify(hi)		/* */
static inline void
hdir_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex,
	  struct aufs_h_ipriv *ipriv)
{
	i_lock_priv(h_dir, ipriv, dir->i_sb);
}
#define hdir_unlock(h_d,d,i,priv)	i_unlock_priv(h_d, priv)
#define hdir_lock_rename(h_p,d,i,same,priv) \
					i_lock_rename_priv(h_p, priv, d[0]->i_sb)
#define hdir_unlock_rename(h_p,d,i,same,priv) \
					i_unlock_rename_priv(h_p, priv)
#define reset_hinotify(i,f)		({i;})
#define aufs_inotify_init()		0
#define aufs_inotify_exit()		/* */
#endif
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
unsigned int hi_flags(struct inode *inode, int isdir);
void set_h_iptr(struct inode *inode, aufs_bindex_t bindex,
		struct inode *h_inode, unsigned int flags);
void update_brange(struct inode *inode, int do_put_zero);

int iinfo_init(struct inode *inode);
void iinfo_fin(struct inode *inode);

#endif /* __KERNEL__ */
#endif /* __AUFS_INODE_H__ */
