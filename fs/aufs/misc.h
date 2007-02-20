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

/* $Id: misc.h,v 1.16 2007/02/19 03:28:59 sfjro Exp $ */

#ifndef __AUFS_MISC_H__
#define __AUFS_MISC_H__

#include <linux/fs.h>
#include <linux/aufs_type.h>

#ifdef __KERNEL__

#include <linux/namei.h>

struct aufs_rwsem {
#ifdef CONFIG_AUFS_DEBUG_RWSEM
	struct mutex		mutex;
#else
	struct rw_semaphore	rwsem;
#ifdef CONFIG_AUFS_DEBUG
	atomic_t		rcnt;
#endif
#endif
};

/* use with caution */
#ifdef CONFIG_AUFS_DEBUG_RWSEM
static inline void rw_init_wlock(struct aufs_rwsem *rw)
{
	mutex_init(&(rw)->mutex);
	mutex_lock(&(rw)->mutex);
}
#define rw_init_nolock(rw)	mutex_init(&(rw)->mutex)
#define rw_destroy(rw)		mutex_destroy(&(rw)->mutex)
#define rw_read_lock(rw)	mutex_lock(&(rw)->mutex)
#define rw_read_unlock(rw)	mutex_unlock(&(rw)->mutex)
#define rw_dgrade_lock(rw)	/* */
#define rw_write_lock(rw)	rw_read_lock(rw)
#define rw_write_unlock(rw)	rw_read_unlock(rw)
// remove this
#define rw_is_write_lock(rw)	mutex_is_locked(&(rw)->mutex)
#define RwMustAnyLock(rw)	DEBUG_ON(!mutex_is_locked(&(rw)->mutex))
#define RwMustReadLock(rw)	RwMustAnyLock(rw)
#define RwMustWriteLock(rw)	RwMustAnyLock(rw)
#else /* CONFIG_AUFS_DEBUG_RWSEM */

#ifdef CONFIG_AUFS_DEBUG
static inline void rw_init_wlock(struct aufs_rwsem *rw)
{
	init_rwsem(&(rw)->rwsem);
	down_write(&(rw)->rwsem);
	atomic_set(&(rw)->rcnt, 0);
}
static inline void rw_init_nolock(struct aufs_rwsem *rw)
{
	init_rwsem(&(rw)->rwsem);
	atomic_set(&(rw)->rcnt, 0);
}
static inline void rw_read_lock(struct aufs_rwsem *rw)
{
	down_read(&(rw)->rwsem);
	atomic_inc(&(rw)->rcnt);
}
static inline void rw_read_unlock(struct aufs_rwsem *rw)
{
	atomic_dec(&(rw)->rcnt);
	up_read(&(rw)->rwsem);
}
static inline void rw_dgrade_lock(struct aufs_rwsem *rw)
{
	atomic_inc(&(rw)->rcnt);
	downgrade_write(&(rw)->rwsem);
}
#else
static inline void rw_init_wlock(struct aufs_rwsem *rw)
{
	init_rwsem(&(rw)->rwsem);
	down_write(&(rw)->rwsem);
}
#define rw_init_nolock(rw)	init_rwsem(&(rw)->rwsem)
#define rw_read_lock(rw)	down_read(&(rw)->rwsem)
#define rw_read_unlock(rw)	up_read(&(rw)->rwsem)
#define rw_dgrade_lock(rw)	downgrade_write(&(rw)->rwsem)
#endif /* CONFIG_AUFS_DEBUG */

#define rw_destroy(rw)		/* */
#define rw_write_lock(rw)	down_write(&(rw)->rwsem)
#define rw_write_unlock(rw)	up_write(&(rw)->rwsem)
// remove this
#define rw_is_write_lock(rw)	(!down_write_trylock(&(rw)->rwsem))
#define RwMustAnyLock(rw)	DEBUG_ON(down_write_trylock(&(rw)->rwsem))

/* use with caution */
#define RwMustReadLock(rw)	do { \
	RwMustAnyLock(rw); \
	DEBUG_ON(!atomic_read(&(rw)->rcnt)); \
	} while(0)
#define RwMustWriteLock(rw)	do { \
	RwMustAnyLock(rw); \
	DEBUG_ON(atomic_read(&(rw)->rcnt)); \
	} while(0)

#endif /* CONFIG_AUFS_DEBUG_RWSEM */

/* ---------------------------------------------------------------------- */

typedef ssize_t (*readf_t)(struct file*, char __user*, size_t, loff_t*);
typedef ssize_t (*writef_t)(struct file*, const char __user*, size_t, loff_t*);

int safe_unlink(struct inode *h_dir, struct dentry *h_dentry);
int hidden_notify_change(struct dentry *h_dentry, struct iattr *ia,
			 struct dentry *dentry);
void *kzrealloc(void *p, int nused, int new_sz);
struct nameidata *fake_dm(struct nameidata *fake_nd, struct nameidata *nd,
			  struct super_block *sb, aufs_bindex_t bindex);
void fake_dm_release(struct nameidata *fake_nd);
int copy_file(struct file *dst, struct file *src, loff_t len);
int test_ro(struct super_block *sb, aufs_bindex_t bindex, struct inode *inode);
int test_perm(struct inode *h_inode, int mask);

#endif /* __KERNEL__ */
#endif /* __AUFS_MISC_H__ */
