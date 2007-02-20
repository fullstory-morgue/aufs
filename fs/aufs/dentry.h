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

/* $Id: dentry.h,v 1.18 2007/02/05 01:45:22 sfjro Exp $ */

#ifndef __AUFS_DENTRY_H__
#define __AUFS_DENTRY_H__

#include <linux/fs.h>
#include <linux/aufs_type.h>
#include "misc.h"

#ifdef __KERNEL__

struct aufs_hdentry {
	struct dentry	*hd_dentry;
};

struct aufs_dinfo {
	atomic_t		di_generation;

	struct aufs_rwsem	di_rwsem;
	aufs_bindex_t		di_bstart, di_bend, di_bwh, di_bdiropq;
	struct aufs_hdentry	*di_hdentry;

	atomic_t		di_reval;
};

/* debug macro. use with caution */
#define DiMustReadLock(d)	do { \
	SiMustAnyLock((d)->d_sb); \
	RwMustReadLock(&dtodi(d)->di_rwsem); \
	} while(0)
#define DiMustWriteLock(d)	do { \
	SiMustAnyLock((d)->d_sb); \
	RwMustWriteLock(&dtodi(d)->di_rwsem); \
	} while(0)
#define DiMustAnyLock(d)	do { \
	SiMustAnyLock((d)->d_sb); \
	RwMustAnyLock(&dtodi(d)->di_rwsem); \
	} while(0)

#define digen(d)	atomic_read(&dtodi(d)->di_generation)

#ifdef CONFIG_AUFS_HINOTIFY
#define direval_test(d)	atomic_read(&dtodi(d)->di_reval)
#define direval_inc(d)	do { \
	atomic_inc(&dtodi(d)->di_reval); \
	/* Dbg("inc i%lu\n",d->d_inode?d->d_inode->i_ino:0); */ \
	/* dump_stack(); */ \
	} while(0)
#define direval_dec(d)	do { \
	int v = atomic_dec_return(&dtodi(d)->di_reval); \
	DEBUG_ON(v < 0); \
	/* Dbg("dec i%lu\n",d->d_inode?d->d_inode->i_ino:0); */ \
	v++; \
	} while(0)
#else
#define direval_test(d)	0
#define direval_inc(d)	/* */
#define direval_dec(d)	({d;})
#endif

/* ---------------------------------------------------------------------- */

extern struct dentry_operations aufs_dop;
#ifdef CONFIG_AUFS_LHASH_PATCH
struct vfsmount *mnt_nfs(struct super_block *sb, aufs_bindex_t bindex);
struct dentry *lkup_one(const char *name, struct dentry *parent, int len,
			struct vfsmount *mnt);
#else
#define mnt_nfs(sb,i)		NULL
#define lkup_one(n,p,l,m)	lookup_one_len(n,p,l)
#endif
struct dentry *sio_lkup_one(const char *name, struct dentry *parent, int len,
			    struct vfsmount *mnt);
int lkup_dentry(struct dentry *dentry, aufs_bindex_t bstart, mode_t type);
int lkup_neg(struct dentry *dentry, aufs_bindex_t bindex);
int refresh_hdentry(struct dentry *dentry, mode_t type);
int reval_dpath(struct dentry *dentry, int sgen);

//dinfo.c
int alloc_dinfo(struct dentry *dentry);
struct aufs_dinfo *dtodi(struct dentry *dentry);

void di_read_lock(struct dentry *d, int flags);
void di_read_unlock(struct dentry *d, int flags);
void di_downgrade_lock(struct dentry *d, int flags);
void di_write_lock(struct dentry *d);
void di_write_unlock(struct dentry *d);
void di_write_lock2(struct dentry *d1, struct dentry *d2, int isdir);
void di_write_unlock2(struct dentry *d1, struct dentry *d2);

aufs_bindex_t dbstart(struct dentry *dentry);
aufs_bindex_t dbend(struct dentry *dentry);
aufs_bindex_t dbwh(struct dentry *dentry);
aufs_bindex_t dbdiropq(struct dentry *dentry);
struct dentry *h_dptr_i(struct dentry *dentry, aufs_bindex_t bindex);
struct dentry *h_dptr(struct dentry *dentry);

aufs_bindex_t dbtail(struct dentry *dentry);
aufs_bindex_t dbtaildir(struct dentry *dentry);
aufs_bindex_t dbtail_generic(struct dentry *dentry);

void set_dbstart(struct dentry *dentry, aufs_bindex_t bindex);
void set_dbend(struct dentry *dentry, aufs_bindex_t bindex);
void set_dbwh(struct dentry *dentry, aufs_bindex_t bindex);
void set_dbdiropq(struct dentry *dentry, aufs_bindex_t bindex);
void hdput(struct aufs_hdentry *hdentry);
void set_h_dptr(struct dentry *dentry, aufs_bindex_t bindex,
		struct dentry *h_dentry);

void update_digen(struct dentry *dentry);
void update_dbstart(struct dentry *dentry);
int find_dbindex(struct dentry *dentry, struct dentry *h_dentry);

#endif /* __KERNEL__ */
#endif /* __AUFS_DENTRY_H__ */
