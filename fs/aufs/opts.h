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

/* $Id: opts.h,v 1.9 2007/02/26 02:28:28 sfjro Exp $ */

#ifndef __AUFS_OPTS_H__
#define __AUFS_OPTS_H__

#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/aufs_type.h>

#ifdef __KERNEL__

struct opt_add {
	aufs_bindex_t		bindex;
	char			*path;
	unsigned int		perm;
	struct nameidata	nd;
};

struct opt_del {
	char		*path;
	struct dentry	*h_root;
};

struct opt_mod {
	char		*path;
	unsigned int	perm;
	struct dentry	*h_root;
};

struct opt_xino {
	char		*path;
	struct file	*file;
};

struct opt {
	int type;
	union {
		struct opt_add	add;
		struct opt_del	del;
		struct opt_mod	mod;
		int		dirwh;
		int		rdcache;
		int		deblk;
		int		nhash;
		int		udba;
		int		coo;
	};
};

struct opts {
	struct opt_xino	xino;
	struct opt	*opt;
	int		max_opt;
};

/* ---------------------------------------------------------------------- */

int br_perm_str(char *p, int len, unsigned int perm);
char *udba_str(int udba);
//char *coo_str(int coo);
void free_opts(struct opts *opts);
int parse_opts(struct super_block *sb, char *str, struct opts *opts,
	       int remount);
int do_opts(struct super_block *sb, struct opts *opts, int remount);

#endif /* __KERNEL__ */
#endif /* __AUFS_OPTS_H__ */
