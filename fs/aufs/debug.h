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

/* $Id: debug.h,v 1.20 2007/01/22 03:59:00 sfjro Exp $ */

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

#define TraceErr(e)	do {if(unlikely((e)<0))LKTRTrace("err %d\n",(int)(e));}while(0)
#define TraceErrPtr(p)	do {if(IS_ERR(p))TraceErr(PTR_ERR(p));}while(0)
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
#define IOErr(fmt, arg...)	Err("I/O Error, " fmt, ##arg)
#define IOErr1(fmt, arg...)	do { \
	static unsigned char c; \
	if (!c++) IOErr(fmt, ##arg); \
	} while (0)
#define IOErrWhck(fmt, arg...)	Err("I/O Error, try whck. " fmt, ##arg)

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_DEBUG
struct aufs_nhash;
void dpri_whlist(struct aufs_nhash *whlist);
struct aufs_vdir;
void dpri_vdir(struct aufs_vdir *vdir);
void dpri_inode(struct inode *inode);
void dpri_dentry(struct dentry *dentry);
void dpri_file(struct file *filp);
void dpri_sb(struct super_block *sb);
#define DbgWhlist(w)		do{LKTRTrace(#w "\n"); dpri_whlist(w);}while(0)
#define DbgVdir(v)		do{LKTRTrace(#v "\n"); dpri_vdir(v);}while(0)
#define DbgInode(i)		do{LKTRTrace(#i "\n"); dpri_inode(i);}while(0)
#define DbgDentry(d)		do{LKTRTrace(#d "\n"); dpri_dentry(d);}while(0)
#define DbgFile(f)		do{LKTRTrace(#f "\n"); dpri_file(f);}while(0)
#define DbgSb(sb)		do{LKTRTrace(#sb "\n"); dpri_sb(sb);}while(0)
#else
#define DbgWhlist(w)		/* */
#define DbgVdir(v)		/* */
#define DbgInode(i)		/* */
#define DbgDentry(d)		/* */
#define DbgFile(f)		/* */
#define DbgSb(sb)		/* */
#endif

/* ---------------------------------------------------------------------- */

/* lock subclass. or you may think it represents aufs locking order */
#if 0 // just a note
enum {
	AufsLsBegin = I_MUTEX_QUOTA,	// last defined in inode mutex
	AUFS_LS_SBINFO,
	AUFS_LS_FINFO,
	AUFS_LS_DINFO_CHILD,	// child first
	AUFS_LS_IINFO_CHILD,
	AUFS_LS_DINFO_PARENT,
	AUFS_LS_IINFO_PARENT,
	AUFS_LS_H_PARENT,	// hidden inode, parent first
	AUFS_LS_H_CHILD,
	AUFS_LS_H_PARENT2,
	AUFS_LS_H_CHILD2,
	AUFS_LS_BR_WH,
	AUFS_LS_PLINK,
	AufsLsEnd
};
#endif

#endif /* __KERNEL__ */
#endif /* __AUFS_DEBUG_H__ */
