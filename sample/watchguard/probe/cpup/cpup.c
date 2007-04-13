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

/* $Id: cpup.c,v 1.1 2007/04/09 02:49:01 sfjro Exp $ */

#include <linux/module.h>

#include "fs/aufs/aufs.h"
#include "probe.h"

static int cpup_entry_hook(struct dentry *dentry, aufs_bindex_t bdst,
			   aufs_bindex_t bsrc, loff_t len, unsigned int flags)
{
	Dbg("%.*s, dst %d, src %d\n", DLNPair(dentry), bdst, bsrc);
	jprobe_return();
	return -1;
}

RpFuncInt(cpup_entry);

/* ---------------------------------------------------------------------- */

static struct hook hooks[] = {
	Hook(cpup_entry),
	{.funcname = NULL}
};

static struct rhook rhooks[] = {
	RHook(cpup_entry),
	{.funcname = NULL}
};

static int __init cpup_init(void)
{
	return probe_init(hooks, rhooks);
}

static void __exit cpup_exit(void)
{
	probe_exit(hooks, rhooks);
}

module_init(cpup_init);
module_exit(cpup_exit);
MODULE_LICENSE("GPL");
