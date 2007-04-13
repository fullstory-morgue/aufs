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

/* $Id: open.c,v 1.1 2007/04/09 02:49:02 sfjro Exp $ */

#include <linux/module.h>

#include "probe.h"

static struct file *do_filp_open_hook(int dfd, const char *filename, int flags,
				      int mode)
{
	Dbg("%s, 0x%x, 0%o\n", filename, flags, mode);
	jprobe_return();
	return NULL;
}

/* ---------------------------------------------------------------------- */

static struct hook hooks[] = {
	Hook(do_filp_open),
	{.funcname = NULL}
};

static struct rhook rhooks[] = {
	//RHook(do_filp_open),
	{.funcname = NULL}
};

static int __init open_init(void)
{
	return probe_init(hooks, rhooks);
}

static void __exit open_exit(void)
{
	probe_exit(hooks, rhooks);
}

module_init(open_init);
module_exit(open_exit);
MODULE_LICENSE("GPL");
