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

/* $Id: debug.h,v 1.24 2007/03/19 04:30:52 sfjro Exp $ */

#ifndef __AUFS_DEBUG_H__
#define __AUFS_DEBUG_H__

#include <linux/fs.h>

#ifdef __KERNEL__

#ifdef CONFIG_AUFS_DEBUG
#define DEBUG_ON(a)	BUG_ON(a)
#else
#define DEBUG_ON(a)	/* */
#endif

/* ---------------------------------------------------------------------- */

/* debug print */
#if defined(CONFIG_LKTR) || defined(CONFIG_LKTR_MODULE)
#include <linux/lktr.h>
#ifdef CONFIG_AUFS_DEBUG
#undef LktrCond
extern atomic_t aufs_cond;
#define LktrCond	((lktr_cond && lktr_cond()) || atomic_read(&aufs_cond))
#endif
#else
#define LktrCond			0
#define LKTRDumpVma(pre, vma, suf)	/* */
#define LKTRDumpStack()			/* */
#define LKTRTrace(format, args...)	/* */
#define LKTRLabel(label)		/* */
#endif /* CONFIG_LKTR */

#define TraceErr(e)	do { \
	if (unlikely((e) < 0)) \
		LKTRTrace("err %d\n", (int)(e)); \
	}while(0)
#define TraceErrPtr(p)	do { \
	if (IS_ERR(p)) \
		TraceErr(PTR_ERR(p)); \
	}while(0)
#define TraceEnter()	LKTRLabel(enter)

/* dirty macros for debug print, use with "%.*s" and caution */
#define LNPair(qstr)		(qstr)->len,(qstr)->name
#define DLNPair(d)		LNPair(&(d)->d_name)

/* ---------------------------------------------------------------------- */

#define Dpri(lvl, fmt, arg...) \
	printk(lvl AUFS_NAME " %s:%d:%s[%d]: " fmt, \
	       __func__, __LINE__, current->comm, current->pid, ##arg)
#define Dbg(fmt, arg...)	Dpri(KERN_DEBUG, fmt, ##arg)
#define Warn(fmt, arg...)	Dpri(KERN_WARNING, fmt, ##arg)
#define Warn1(fmt, arg...)	do { \
	static unsigned char c; \
	if (!c++) Warn(fmt, ##arg); \
	} while (0)
#define Err(fmt, arg...)	Dpri(KERN_ERR, fmt, ##arg)
#define Err1(fmt, arg...)	do { \
	static unsigned char c; \
	if (!c++) Err(fmt, ##arg); \
	} while (0)
#define IOErr(fmt, arg...)	Err("I/O Error, " fmt, ##arg)
#define IOErr1(fmt, arg...)	do { \
	static unsigned char c; \
	if (!c++) IOErr(fmt, ##arg); \
	} while (0)
#define IOErrWhck(fmt, arg...)	Err("I/O Error, try whck. " fmt, ##arg)

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_DEBUG
struct aufs_nhash;
void au_dpri_whlist(struct aufs_nhash *whlist);
struct aufs_vdir;
void au_dpri_vdir(struct aufs_vdir *vdir);
void au_dpri_inode(struct inode *inode);
void au_dpri_dentry(struct dentry *dentry);
void au_dpri_file(struct file *filp);
void au_dpri_sb(struct super_block *sb);
#define DbgWhlist(w)		do{LKTRTrace(#w "\n"); au_dpri_whlist(w);}while(0)
#define DbgVdir(v)		do{LKTRTrace(#v "\n"); au_dpri_vdir(v);}while(0)
#define DbgInode(i)		do{LKTRTrace(#i "\n"); au_dpri_inode(i);}while(0)
#define DbgDentry(d)		do{LKTRTrace(#d "\n"); au_dpri_dentry(d);}while(0)
#define DbgFile(f)		do{LKTRTrace(#f "\n"); au_dpri_file(f);}while(0)
#define DbgSb(sb)		do{LKTRTrace(#sb "\n"); au_dpri_sb(sb);}while(0)
#else
#define DbgWhlist(w)		/* */
#define DbgVdir(v)		/* */
#define DbgInode(i)		/* */
#define DbgDentry(d)		/* */
#define DbgFile(f)		/* */
#define DbgSb(sb)		/* */
#endif

/* ---------------------------------------------------------------------- */

#ifndef MAX_LOCKDEP_SUBCLASSES
#define MAX_LOCKDEP_SUBCLASSES 8
#define RemoveMe
#endif

#if MAX_LOCKDEP_SUBCLASSES == 32
/* lock subclass. or you may think it represents aufs locking order */
enum {
	AufsLscBegin = I_MUTEX_QUOTA,	// last defined in inode mutex
	AUFS_LSC_SBILIST_SYSFS,		// access to sysfs
	AUFS_LSC_SBINFO_NFS,		// access from nfsd
	AUFS_LSC_SBINFO_HINOTIFY,	// udba=inotify
	AUFS_LSC_SBINFO,
	AUFS_LSC_FINFO,
	AUFS_LSC_DINFO_CHILD,	// child first
	AUFS_LSC_IINFO_CHILD,
	AUFS_LSC_DINFO_CHILD2,	// rename(2), link(2), and cpup at hinotify
	AUFS_LSC_IINFO_CHILD2,
	AUFS_LSC_DINFO_CHILD3,	// copyup dirs
	AUFS_LSC_IINFO_CHILD3,
	AUFS_LSC_DINFO_PARENT,
	AUFS_LSC_IINFO_PARENT,
	AUFS_LSC_DINFO_PARENT2,	// rename(2), link(2), and cpup at hinotify
	AUFS_LSC_IINFO_PARENT2,
	AUFS_LSC_DINFO_PARENT3,	// copyup dirs
	AUFS_LSC_IINFO_PARENT3,
	AUFS_LSC_H_GPARENT,	// setattr with inotify
	AUFS_LSC_H_PARENT,	// hidden inode, parent first
	//AUFS_LSC_INODE_NEW,
	AUFS_LSC_IINFO_NEW,
	AUFS_LSC_H_CHILD,
	AUFS_LSC_H_PARENT2,
	AUFS_LSC_H_CHILD2,
	AUFS_LSC_BR_WH,
	AUFS_LSC_PLINK,
	AUFS_LSC_SBILIST,
	AufsLscEnd
};
#else
enum {
	AUFS_LSC_SBILIST_SYSFS,
	AUFS_LSC_SBILIST
};
enum {
	AUFS_LSC_SBINFO_NFS,
	AUFS_LSC_SBINFO_HINOTIFY,
	AUFS_LSC_SBINFO
};
enum {
	AUFS_LSC_FINFO
};
enum {
	AUFS_LSC_DINFO_CHILD,
	AUFS_LSC_DINFO_CHILD2,
	AUFS_LSC_DINFO_CHILD3,
	AUFS_LSC_DINFO_PARENT,
	AUFS_LSC_DINFO_PARENT2,
	AUFS_LSC_DINFO_PARENT3
};
enum {
	AUFS_LSC_IINFO_CHILD,
	AUFS_LSC_IINFO_CHILD2,
	AUFS_LSC_IINFO_CHILD3,
	AUFS_LSC_IINFO_PARENT,
	AUFS_LSC_IINFO_PARENT2,
	AUFS_LSC_IINFO_PARENT3,
	AUFS_LSC_IINFO_NEW
};
// default MAX_LOCKDEP_SUBCLASSES(8) is not enough
enum {
	AufsLscBegin = I_MUTEX_QUOTA,
	AUFS_LSC_H_GPARENT,
	AUFS_LSC_H_PARENT,
	//AUFS_LSC_INODE_NEW,
	AUFS_LSC_H_CHILD,
	AUFS_LSC_H_PARENT2,
	AUFS_LSC_H_CHILD2,
	AufsLscEnd
};
enum {
	AUFS_LSC_BR_WH
};
enum {
	AUFS_LSC_PLINK
};
#endif

#ifdef RemoveMe
#undef MAX_LOCKDEP_SUBCLASSES
#endif

#endif /* __KERNEL__ */
#endif /* __AUFS_DEBUG_H__ */
