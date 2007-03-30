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

/* $Id: aufs_type.h,v 1.47 2007/03/28 01:56:42 sfjro Exp $ */

#ifndef __AUFS_TYPE_H__
#define __AUFS_TYPE_H__

#define AUFS_VERSION	"20070328"

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_BRANCH_MAX_127
typedef char aufs_bindex_t;
#define AUFS_BRANCH_MAX 127
#else
typedef short aufs_bindex_t;
#ifdef CONFIG_AUFS_BRANCH_MAX_511
#define AUFS_BRANCH_MAX 511
#elif defined(CONFIG_AUFS_BRANCH_MAX_1023)
#define AUFS_BRANCH_MAX 1023
#elif defined(CONFIG_AUFS_BRANCH_MAX_32767)
#define AUFS_BRANCH_MAX 32767
#else
#error unknown CONFIG_AUFS_BRANCH_MAX value
#endif
#endif

#define AUFS_NAME		"aufs"
#define AUFS_WH_PFX		".wh."
#define AUFS_WH_LEN		(sizeof(AUFS_WH_PFX)-1)
#define AUFS_XINO_FNAME		"." AUFS_NAME ".xino"
#define AUFS_XINO_DEFPATH	"/tmp/" AUFS_XINO_FNAME
#define AUFS_DIRWH_DEF		3
#define AUFS_RDCACHE_DEF	10 // seconds
#define AUFS_WKQ_NAME		AUFS_NAME "d"
#define AUFS_NWKQ_DEF		4

#define AUFS_FSTYPE		AUFS_NAME
#ifdef CONFIG_AUFS_COMPAT
#define AUFS_DIROPQ_NAME	"__dir_opaque"
#else
#define AUFS_DIROPQ_NAME	AUFS_WH_PFX ".opq" // whiteouted doubly
#endif

#define AUFS_WH_BASENAME	AUFS_WH_PFX AUFS_NAME // will be whiteouted doubly
#define AUFS_WH_DIROPQ		AUFS_WH_PFX AUFS_DIROPQ_NAME
#define AUFS_WH_PLINKDIR	AUFS_WH_PFX "plink" // will be whiteouted doubly

#endif /* __AUFS_TYPE_H__ */
