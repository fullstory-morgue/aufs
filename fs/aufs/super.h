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

/* $Id: super.h,v 1.33 2007/02/19 03:29:38 sfjro Exp $ */

#ifndef __AUFS_SUPER_H__
#define __AUFS_SUPER_H__

#include <linux/fs.h>
#include <linux/version.h>
#include <linux/aufs_type.h>
#include "misc.h"

#define AUFS_ROOT_INO		2
#define AUFS_FIRST_INO		11

#ifdef __KERNEL__
#include "branch.h"

struct aufs_sbinfo {
	struct aufs_rwsem	si_rwsem;

	/* branch management */
	int			si_generation;

	/*
	 * set true when refresh_dirs() at remount time failed.
	 * then try refreshing dirs at access time again.
	 * if it is false, refreshing dirs at access time is unnecesary
	 */
	unsigned int		si_failed_refresh_dirs:1;
	aufs_bindex_t		si_bend;
	//unsigned int		si_last_br_id;
	aufs_bindex_t		si_last_br_id;
	struct aufs_branch	**si_branch;

	/* mount flags */
	unsigned int		si_flags;

	/* external inode number table */
	atomic_long_t		si_xino;	// time bomb
	//struct file		*si_xino_bmap;

	/* readdir cache time, max, in HZ */
	unsigned long		si_rdcache;

	/*
	 * If the number of whiteouts are larger than si_dirwh, leave all of
	 * them after rename_whtmp to reduce the cost of rmdir(2).
	 * future fsck.aufs or kernel thread will remove them later.
	 * Otherwise, remove all whiteouts and the dir in rmdir(2).
	 */
	unsigned int		si_dirwh;

	/* pseudo_link list */ // dirty
	spinlock_t		si_plink_lock;
	struct list_head	si_plink;

#if 0 //defined(CONFIG_EXPORTFS) || defined(CONFIG_EXPORTFS_MODULE)
	int			si_tested_generation;
#endif

#if 0 //def CONFIG_AUFS_NEST
	/* locked vma list for mmap() */ // very dirty
	spinlock_t		si_lvma_lock;
	struct list_head	si_lvma;
#endif
	/* sysfs */
	struct kobject		si_kobj;
};

#define sbtype(sb)	({(sb)->s_type->name;})
#define SB_NFS(sb)	(!strcmp(sbtype(sb), "nfs"))
#if !defined(CONFIG_NFS_FS) && !defined(CONFIG_NFS_FS_MODULE)
#undef SB_NFS
#define SB_NFS(sb)	0
#endif
#define SB_AUFS(sb)	(!strcmp(sbtype(sb), AUFS_FSTYPE))
#define SB_REMOTE(sb)	SB_NFS(sb)

#if 0 //defined(CONFIG_EXPORTFS) || defined(CONFIG_EXPORTFS_MODULE)
extern struct export_operations aufs_export_op;
#define init_export_op(sb)	({(sb)->s_export_op = &aufs_export_op;})
#else
#define init_export_op(sb)	/* */
#endif

#if 0 //def CONFIG_AUFS_NEST
static inline void init_lvma(struct aufs_sbinfo *sbinfo)
{
	spin_lock_init(&sbinfo->si_lvma_lock);
	INIT_LIST_HEAD(&sbinfo->si_lvma);
}
#else
#define init_lvma(sbinfo)	/* */
#endif

/* see linux/include/linux/jiffies.h */
#define AufsGenYounger(a,b)	((b) - (a) < 0)
#define AufsGenOlder(a,b)	AufsGenYounger(b,a)

/* flags for aufs_read_lock()/di_read_lock() */
#define AUFS_D_WLOCK		1
#define AUFS_I_RLOCK		2
#define AUFS_I_WLOCK		4

/* lock superblock. mainly for entry point functions */
#define si_read_lock(sb)	rw_read_lock(&stosi(sb)->si_rwsem)
#define si_read_unlock(sb)	rw_read_unlock(&stosi(sb)->si_rwsem)
#define si_write_lock(sb)	rw_write_lock(&stosi(sb)->si_rwsem)
#define si_write_unlock(sb)	rw_write_unlock(&stosi(sb)->si_rwsem)
#define SiMustReadLock(sb)	RwMustReadLock(&stosi(sb)->si_rwsem)
#define SiMustWriteLock(sb)	RwMustWriteLock(&stosi(sb)->si_rwsem)
#define SiMustAnyLock(sb)	RwMustAnyLock(&stosi(sb)->si_rwsem)

/* Mount time flags */
#define MS_XINO			1
#define MS_ZXINO		(1 << 1)
#define MS_PLINK		(1 << 2)
#define MS_UDBA_NONE		(1 << 3)
#define MS_UDBA_REVAL		(1 << 4)
#define MS_UDBA_INOTIFY		(1 << 5)
#define MS_WARN_PERM		(1 << 6)
#define MS_COO_NONE		(1 << 7)
#define MS_COO_LEAF		(1 << 8)
#define MS_COO_ALL		(1 << 9)
#define MS_ALWAYS_DIROPQ	(1 << 10)
#define MS_IPRIVATE		(1 << 11)
#define MS_SET(sb,flg)		({stosi(sb)->si_flags |= flg;})
#define MS_CLR(sb,flg)		({stosi(sb)->si_flags &= ~(flg);})
#define IS_MS(sb,flg)		({stosi(sb)->si_flags & flg;})

#ifdef CONFIG_AUFS_COMPAT
#define MS_DEF_DIROPQ		MS_ALWAYS_DIROPQ
#else
#define MS_DEF_DIROPQ		0
#endif

#ifdef ForceHinotify
#define MS_UDBA_DEF		MS_UDBA_INOTIFY
#else
#define MS_UDBA_DEF		MS_UDBA_REVAL
#endif
#define MS_UDBA_MASK		(MS_UDBA_NONE | MS_UDBA_REVAL | MS_UDBA_INOTIFY)
#define MS_UDBA(sb)		IS_MS(sb, MS_UDBA_MASK)
#define MS_COO_DEF		MS_COO_NONE
#define MS_COO_MASK		(MS_COO_NONE | MS_COO_LEAF | MS_COO_ALL)
#define MS_COO(sb)		IS_MS(sb, MS_COO_MASK)
#ifdef ForceIPrivate
#define MS_IPRIV_DEF		MS_IPRIVATE
#else
#define MS_IPRIV_DEF		0
#endif

#define COMM_DEF		(MS_XINO | MS_UDBA_DEF | MS_WARN_PERM \
					| MS_COO_DEF | MS_DEF_DIROPQ | MS_IPRIV_DEF)
#if LINUX_VERSION_CODE != KERNEL_VERSION(2,6,15)
#define MS_DEF	(COMM_DEF | MS_PLINK)
#else
#define MS_DEF	COMM_DEF
#endif

/* ---------------------------------------------------------------------- */

/* Superblock to branch */
#define sbr_mnt(sb, bindex)	({stobr(sb, bindex)->br_mnt;})
#define sbr_sb(sb, bindex)	({sbr_mnt(sb, bindex)->mnt_sb;})
#define sbr_count(sb, bindex)	br_count(stobr(sb, bindex))
#define sbr_get(sb, bindex)	br_get(stobr(sb, bindex))
#define sbr_put(sb, bindex)	br_put(stobr(sb, bindex))
#define sbr_perm(sb, bindex) 	({stobr(sb, bindex)->br_perm;})
#define sbr_perm_str(sb, bindex, str, len) \
				br_perm_str(str, len, stobr(sb, bindex)->br_perm)

/* ---------------------------------------------------------------------- */

/* kmem cache */
enum {AUFS_CACHE_DINFO, AUFS_CACHE_ICNTNR, AUFS_CACHE_FINFO, AUFS_CACHE_VDIR,
      AUFS_CACHE_DEHSTR, AUFS_CACHE_HINOTIFY, _AufsCacheLast};
extern struct kmem_cache *aufs_cachep[];
#define aufs_cache_alloc(i)	kmem_cache_alloc(aufs_cachep[i], GFP_KERNEL)
#define cache_alloc_dinfo() 	aufs_cache_alloc(AUFS_CACHE_DINFO)
#define cache_alloc_icntnr() 	aufs_cache_alloc(AUFS_CACHE_ICNTNR)
#define cache_alloc_finfo() 	aufs_cache_alloc(AUFS_CACHE_FINFO)
#define cache_alloc_vdir() 	aufs_cache_alloc(AUFS_CACHE_VDIR)
#define cache_alloc_dehstr() 	aufs_cache_alloc(AUFS_CACHE_DEHSTR)
#define cache_alloc_hinotify() 	aufs_cache_alloc(AUFS_CACHE_HINOTIFY)
#define aufs_cache_free(i, p)	kmem_cache_free(aufs_cachep[i], p)
#define cache_free_dinfo(p)	aufs_cache_free(AUFS_CACHE_DINFO, p)
#define cache_free_icntnr(p)	aufs_cache_free(AUFS_CACHE_ICNTNR, p)
#define cache_free_finfo(p)	aufs_cache_free(AUFS_CACHE_FINFO, p)
#define cache_free_vdir(p)	aufs_cache_free(AUFS_CACHE_VDIR, p)
#define cache_free_dehstr(p)	aufs_cache_free(AUFS_CACHE_DEHSTR, p)
#define cache_free_hinotify(p)	aufs_cache_free(AUFS_CACHE_HINOTIFY, p)

/* ---------------------------------------------------------------------- */

// module parameter
extern unsigned char aufs_nwkq;

//xino.c
struct file *xino_create(struct super_block *sb, char *fname, int silent,
			 struct dentry *parent);
int xino_write(struct super_block *sb, aufs_bindex_t bindex, ino_t h_ino,
	       ino_t ino);
int xino_read(struct super_block *sb, aufs_bindex_t bindex, ino_t h_ino,
	      ino_t *ino, int force);
int xino_init(struct super_block *sb, aufs_bindex_t bindex,
	      struct file *base_file, int do_test);
struct opt_xino;
int xino_set(struct super_block *sb, struct opt_xino *xino, int remount);
int xino_clr(struct super_block *sb);
struct file *xino_def(struct super_block *sb);

//sbinfo.c
struct aufs_sbinfo *stosi(struct super_block *sb);
#if 0 // debug
void MS_SET(struct super_block *sb, unsigned int flg);
void MS_CLR(struct super_block *sb, unsigned int flg);
unsigned int IS_MS(struct super_block *sb, unsigned int flg);
#endif
aufs_bindex_t sbend(struct super_block *sb);
struct aufs_branch *stobr(struct super_block *sb, aufs_bindex_t bindex);
int sigen(struct super_block *sb);
int sigen_inc(struct super_block *sb);
int find_bindex(struct super_block *sb, struct aufs_branch *br);

void aufs_read_lock(struct dentry *dentry, int flags);
void aufs_read_unlock(struct dentry *dentry, int flags);
void aufs_write_lock(struct dentry *dentry);
void aufs_write_unlock(struct dentry *dentry);
void aufs_read_and_write_lock2(struct dentry *d1, struct dentry *d2, int isdir);
void aufs_read_and_write_unlock2(struct dentry *d1, struct dentry *d2);

#ifdef CONFIG_AUFS_DEBUG
void list_plink(struct super_block *sb);
#else
#define list_plink(sb)	/* */
#endif
int is_plinked(struct super_block *sb, struct inode *inode);
void append_plink(struct super_block *sb, struct inode *inode,
		  struct dentry *h_dentry, aufs_bindex_t bindex);
void put_plink(struct super_block *sb);
void half_refresh_plink(struct super_block *sb, unsigned int id);

unsigned int new_br_id(struct super_block *sb);

#endif /* __KERNEL__ */
#endif /* __AUFS_SUPER_H__ */
