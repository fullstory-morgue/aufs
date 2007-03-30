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

/* $Id: misc.h,v 1.21 2007/03/28 01:46:16 sfjro Exp $ */

#ifndef __AUFS_MISC_H__
#define __AUFS_MISC_H__

#include <linux/fs.h>
#include <linux/aufs_type.h>

#ifdef __KERNEL__

#include <linux/namei.h>
#include <linux/sched.h>

struct aufs_rwsem {
	struct rw_semaphore	rwsem;
#ifdef CONFIG_AUFS_DEBUG
	atomic_t		rcnt;
#define rcnt_init(rw)		atomic_set(&(rw)->rcnt, 0)
#define rcnt_inc(rw)		atomic_inc(&(rw)->rcnt)
#define rcnt_dec(rw)		atomic_dec(&(rw)->rcnt)
#else
#define rcnt_init(rw)		/* */
#define rcnt_inc(rw)		/* */
#define rcnt_dec(rw)		/* */
#endif
};

static inline void rw_init_nolock(struct aufs_rwsem *rw)
{
	init_rwsem(&rw->rwsem);
	rcnt_init(rw);
}

static inline void rw_init_wlock(struct aufs_rwsem *rw)
{
	rw_init_nolock(rw);
	down_write(&rw->rwsem);
}

static inline void rw_init_wlock_nested(struct aufs_rwsem *rw, unsigned int lsc)
{
	rw_init_nolock(rw);
	down_write_nested(&rw->rwsem, lsc);
}

static inline void rw_read_lock(struct aufs_rwsem *rw)
{
	down_read(&rw->rwsem);
	rcnt_inc(rw);
}

static inline void rw_read_lock_nested(struct aufs_rwsem *rw, unsigned int lsc)
{
	down_read_nested(&rw->rwsem, lsc);
	rcnt_inc(rw);
}

static inline void rw_read_unlock(struct aufs_rwsem *rw)
{
	rcnt_dec(rw);
	up_read(&rw->rwsem);
}

static inline void rw_dgrade_lock(struct aufs_rwsem *rw)
{
	rcnt_inc(rw);
	downgrade_write(&rw->rwsem);
}

static inline void rw_write_lock(struct aufs_rwsem *rw)
{
	down_write(&rw->rwsem);
}

static inline void rw_write_lock_nested(struct aufs_rwsem *rw, unsigned int lsc)
{
	down_write_nested(&rw->rwsem, lsc);
}

static inline void rw_write_unlock(struct aufs_rwsem *rw)
{
	up_write(&rw->rwsem);
}

static inline void RwMustAnyLock(struct aufs_rwsem *rw)
{
	DEBUG_ON(down_write_trylock(&rw->rwsem));
}

static inline void RwMustReadLock(struct aufs_rwsem *rw)
{
	RwMustAnyLock(rw);
#ifdef CONFIG_AUFS_DEBUG
	DEBUG_ON(!atomic_read(&rw->rcnt));
#endif
}

static inline void RwMustWriteLock(struct aufs_rwsem *rw)
{
	RwMustAnyLock(rw);
#ifdef CONFIG_AUFS_DEBUG
	DEBUG_ON(atomic_read(&rw->rcnt));
#endif
}

static inline void RwMustNoWaiters(struct aufs_rwsem *rw)
{
	DEBUG_ON(!list_empty(&rw->rwsem.wait_list));
}

#undef rcnt_init
#undef rcnt_inc
#undef rcnt_dec

/* ---------------------------------------------------------------------- */

typedef ssize_t (*readf_t)(struct file*, char __user*, size_t, loff_t*);
typedef ssize_t (*writef_t)(struct file*, const char __user*, size_t, loff_t*);

void *au_kzrealloc(void *p, int nused, int new_sz);
struct nameidata *fake_dm(struct nameidata *fake_nd, struct nameidata *nd,
			  struct super_block *sb, aufs_bindex_t bindex);
void fake_dm_release(struct nameidata *fake_nd);
int au_copy_file(struct file *dst, struct file *src, loff_t len,
		 struct super_block *sb, int *sparse);
int test_ro(struct super_block *sb, aufs_bindex_t bindex, struct inode *inode);
int au_test_perm(struct inode *h_inode, int mask, int dlgt);

/* ---------------------------------------------------------------------- */

static inline int au_is_kthread(struct task_struct *tsk)
{
	return !tsk->mm;
}

#endif /* __KERNEL__ */
#endif /* __AUFS_MISC_H__ */
