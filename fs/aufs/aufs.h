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

/* $Id: aufs.h,v 1.17 2007/01/22 04:00:21 sfjro Exp $ */

#ifndef __AUFS_H__
#define __AUFS_H__

#include <linux/version.h>

/* ---------------------------------------------------------------------- */

// limited support before 2.6.16, curretly 2.6.15 only.
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,15)
#define atomic_long_t		atomic_t
#define atomic_long_set		atomic_set
#define timespec_to_ns(ts)	({(long long)(ts)->tv_sec;})
#define D_CHILD			d_child
#else
#define D_CHILD			d_u.d_child
#endif

/* ---------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
#define filldir_ino_t	u64
#define filldir_ino_fmt	"%Lu"
#else
#define filldir_ino_t	ino_t
#define filldir_ino_fmt	"%lu"
#endif

/* ---------------------------------------------------------------------- */

#include "debug.h"

#include "branch.h"
#include "cpup.h"
#include "dentry.h"
#include "dir.h"
#include "file.h"
#include "inode.h"
//#include "kobj.h"
#include "misc.h"
#include "opts.h"
#include "super.h"
#include "whout.h"
#include "wkq.h"
//#include "xattr.h"

#if defined(CONFIG_AUFS_MODULE) && !defined(CONFIG_AUFS_KSIZE_PATCH)
#define ksize(p)	(-1)
#endif

#endif /* __AUFS_H__ */
