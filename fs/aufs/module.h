/*
 * Copyright (C) 2007 Junjiro Okajima
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

/* $Id: module.h,v 1.5 2007/03/27 12:47:23 sfjro Exp $ */

#ifndef __AUFS_MODULE_H__
#define __AUFS_MODULE_H__

#ifdef __KERNEL__

#include <linux/slab.h>

/* ---------------------------------------------------------------------- */

// module.c
extern char au_esc_chars[];
extern short aufs_nwkq;
extern int sysaufs_brs, au_dir_roflags;

// wkq.c
extern struct au_wkq *au_wkq;

/* kmem cache */
enum {AuCache_DINFO, AuCache_ICNTNR, AuCache_FINFO, AuCache_VDIR,
      AuCache_DEHSTR, AuCache_HINOTIFY, AuCache_Last};
extern struct kmem_cache *aufs_cachep[];

static inline void *aufs_cache_alloc(int i)
{
	return kmem_cache_alloc(aufs_cachep[i], GFP_KERNEL);
}

#define cache_alloc_dinfo() 	aufs_cache_alloc(AuCache_DINFO)
#define cache_alloc_icntnr() 	aufs_cache_alloc(AuCache_ICNTNR)
#define cache_alloc_finfo() 	aufs_cache_alloc(AuCache_FINFO)
#define cache_alloc_vdir() 	aufs_cache_alloc(AuCache_VDIR)
#define cache_alloc_dehstr() 	aufs_cache_alloc(AuCache_DEHSTR)
#define cache_alloc_hinotify() 	aufs_cache_alloc(AuCache_HINOTIFY)

static inline void aufs_cache_free(int i, void *p)
{
	kmem_cache_free(aufs_cachep[i], p);
}

#define cache_free_dinfo(p)	aufs_cache_free(AuCache_DINFO, p)
#define cache_free_icntnr(p)	aufs_cache_free(AuCache_ICNTNR, p)
#define cache_free_finfo(p)	aufs_cache_free(AuCache_FINFO, p)
#define cache_free_vdir(p)	aufs_cache_free(AuCache_VDIR, p)
#define cache_free_dehstr(p)	aufs_cache_free(AuCache_DEHSTR, p)
#define cache_free_hinotify(p)	aufs_cache_free(AuCache_HINOTIFY, p)

//sysaufs.c
#ifdef CONFIG_AUFS_SYSAUFS
struct aufs_sbinfo;
void add_sbilist(struct aufs_sbinfo *sbinfo);
void del_sbilist(struct aufs_sbinfo *sbinfo);
int __init sysaufs_init(void);
void sysaufs_fin(void);
void sysaufs_notify_remount(void);
#else
static inline void add_sbilist(struct aufs_sbinfo *sbinfo)
{
	/* nothing */
}

static inline void del_sbilist(struct aufs_sbinfo *sbinfo)
{
	/* nothing */
}

#define sysaufs_init()		0
#define sysaufs_fin()		/* */
#define sysaufs_notify_remount()	/* */
#endif

#endif /* __KERNEL__ */
#endif /* __AUFS_MODULE_H__ */
