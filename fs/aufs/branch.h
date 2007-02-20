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

/* $Id: branch.h,v 1.22 2007/02/19 03:26:18 sfjro Exp $ */

#ifndef __AUFS_BRANCH_H__
#define __AUFS_BRANCH_H__

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/aufs_type.h>
#include "misc.h"

#ifdef __KERNEL__

/* protected by superblock rwsem */
struct aufs_branch {
	struct file		*br_xino;
	readf_t			br_xino_read;
	writef_t		br_xino_write;

	//unsigned int		br_id;
	aufs_bindex_t		br_id;

	unsigned int		br_perm;
	struct vfsmount		*br_mnt;
	atomic_t		br_count;

	/* whiteout base */
	struct aufs_rwsem	br_wh_rwsem;
	struct dentry		*br_wh;
	atomic_t 		br_wh_running;

	/* pseudo-link dir */
	struct dentry		*br_plink;
};

#define br_count(br)		atomic_read(&(br)->br_count)
#define br_get(br)		atomic_inc(&(br)->br_count)
#define br_put(br)		atomic_dec(&(br)->br_count)

#define br_wh_read_lock(br)	rw_read_lock(&(br)->br_wh_rwsem)
#define br_wh_read_unlock(br)	rw_read_unlock(&(br)->br_wh_rwsem)
#define br_wh_write_lock(br)	rw_write_lock(&(br)->br_wh_rwsem)
#define br_wh_write_unlock(br)	rw_write_unlock(&(br)->br_wh_rwsem)

/* debug macro. use with caution */
#define BrWhMustReadLock(br)	do { \
	/* SiMustAnyLock(sb); */ \
	RwMustReadLock(&(br)->br_wh_rwsem); \
	} while(0)
#define BrWhMustWriteLock(br)	do { \
	/* SiMustAnyLock(sb); */ \
	RwMustWriteLock(&(br)->br_wh_rwsem); \
	} while(0)
#define BrWhMustAnyLock(br)	do { \
	/* SiMustAnyLock(sb); */ \
	RwMustAnyLock(&(br)->br_wh_rwsem); \
	} while(0)

/* ---------------------------------------------------------------------- */
/* branch permission flags */
/*
 * WH: whiteout may exist on readonly branch.
 * RR: natively readonly branch, isofs, squashfs, etc.
 */
#define AufsBrpermBase		MAY_APPEND /* last defined in linux/fs.h */
#define AUFS_MAY_WH		(AufsBrpermBase << 1)
#define AUFS_MAY_RR		(AufsBrpermBase << 2)

#define AUFS_BRPERM_MASK	(MAY_WRITE | MAY_READ | AUFS_MAY_WH | AUFS_MAY_RR)

/* ---------------------------------------------------------------------- */

#define _AufsNoNfsBranchMsg "NFS branch is not supported"
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,15)
#define AufsNoNfsBranch
#define AufsNoNfsBranchMsg _AufsNoNfsBranchMsg
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19) \
	&& !defined(CONFIG_AUFS_LHASH_PATCH)
#define AufsNoNfsBranch
#define AufsNoNfsBranchMsg _AufsNoNfsBranchMsg \
	", try lhash.patch and CONFIG_AUFS_LHASH_PATCH"
#endif

/* ---------------------------------------------------------------------- */

struct aufs_sbinfo;
void free_branches(struct aufs_sbinfo *sinfo);
int br_rdonly(struct aufs_branch *br);
int find_brindex(struct super_block *sb, unsigned int id);
int find_rw_br(struct super_block *sb, aufs_bindex_t bend);
int find_rw_parent_br(struct dentry *dentry, aufs_bindex_t bend);
struct opt_add;
int br_add(struct super_block *sb, struct opt_add *add, int remount);
struct opt_del;
int br_del(struct super_block *sb, struct opt_del *del, int remount);
struct opt_mod;
int br_mod(struct super_block *sb, struct opt_mod *mod, int remount);

#endif /* __KERNEL__ */
#endif /* __AUFS_BRANCH_H__ */
