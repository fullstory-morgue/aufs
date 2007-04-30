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

/* $Id: sysaufs.h,v 1.1 2007/04/30 05:43:57 sfjro Exp $ */

#ifndef __SYSAUFS_H__
#define __SYSAUFS_H__

#ifdef __KERNEL__

#include <linux/seq_file.h>
#include <linux/sysfs.h>

struct sysaufs_args {
	int index;
	struct mutex *mtx;
	struct super_block *sb;
};

typedef int (*sysaufs_op)(struct seq_file *seq, struct sysaufs_args *args,
			  int *do_size);

struct sysaufs_entry {
	struct bin_attribute attr;
	int allocated;	/* zero minus means pages */
	int err;
	sysaufs_op *ops;
};

/* ---------------------------------------------------------------------- */

struct aufs_sbinfo;
#ifdef CONFIG_AUFS_SYSAUFS
void sysaufs_add(struct aufs_sbinfo *sbinfo);
void sysaufs_del(struct aufs_sbinfo *sbinfo);
int __init sysaufs_init(void);
void sysaufs_fin(void);
void sysaufs_notify_remount(void);
#else
static inline void sysaufs_add(struct aufs_sbinfo *sbinfo)
{
	/* nothing */
}

static inline void sysaufs_del(struct aufs_sbinfo *sbinfo)
{
	/* nothing */
}
#define sysaufs_init()			0
#define sysaufs_fin()			/* */
#define sysaufs_notify_remount()	/* */
#endif

#endif /* __KERNEL__ */
#endif /* __SYSAUFS_H__ */
