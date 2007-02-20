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

/* $Id: module.c,v 1.1 2007/02/19 03:32:08 sfjro Exp $ */

//#include <linux/init.h>
#include <linux/module.h>
#include "aufs.h"

/* ---------------------------------------------------------------------- */

/*
 * aufs caches
 */

struct kmem_cache *aufs_cachep[_AufsCacheLast];
static int __init create_cache(void)
{
#define Cache(type) \
	kmem_cache_create(#type, sizeof(struct type), 0, \
			  SLAB_RECLAIM_ACCOUNT, NULL, NULL)

	if ((aufs_cachep[AUFS_CACHE_DINFO] = Cache(aufs_dinfo))
	    && (aufs_cachep[AUFS_CACHE_ICNTNR] = Cache(aufs_icntnr))
	    && (aufs_cachep[AUFS_CACHE_FINFO] = Cache(aufs_finfo))
	    //&& (aufs_cachep[AUFS_CACHE_FINFO] = NULL)
	    && (aufs_cachep[AUFS_CACHE_VDIR] = Cache(aufs_vdir))
	    && (aufs_cachep[AUFS_CACHE_DEHSTR] = Cache(aufs_dehstr))
	    && (aufs_cachep[AUFS_CACHE_HINOTIFY] = Cache(aufs_hinotify)))
		return 0;
	return -ENOMEM;

#undef Cache
}

static void __exit destroy_cache(void)
{
	int i;
	for (i = 0; i < _AufsCacheLast; i++)
		if (aufs_cachep[i])
			kmem_cache_destroy(aufs_cachep[i]);
}

/* ---------------------------------------------------------------------- */

/*
 * functions for module interface.
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Junjiro Okajima");
MODULE_DESCRIPTION(AUFS_NAME " -- Another unionfs");
MODULE_VERSION(AUFS_VERSION);

unsigned char aufs_nwkq = AUFS_NWKQ_DEF;
MODULE_PARM_DESC(nwkq, "the number of workqueue thread, " AUFS_WKQ_NAME);
module_param_named(nwkq, aufs_nwkq, byte, 0444);
extern struct file_system_type aufs_fs_type;

static int __init aufs_init(void)
{
	int err;

	//sbinfo->si_xino is atomic_long_t
	BUILD_BUG_ON(sizeof(ino_t) != sizeof(long));

#ifdef CONFIG_AUFS_DEBUG
	{
		struct aufs_destr destr;
		destr.len = -1;
		DEBUG_ON(destr.len < NAME_MAX);
	}

#ifdef CONFIG_4KSTACKS
	printk("CONFIG_4KSTACKS is defined.\n");
#endif
#if 0 // verbose debug
	{
		union {
			struct aufs_branch br;
			struct aufs_dinfo di;
			struct aufs_finfo fi;
			struct aufs_iinfo ii;
			struct aufs_hinode hi;
			struct aufs_sbinfo si;
			struct aufs_destr destr;
			struct aufs_de de;
			struct aufs_wh wh;
			struct aufs_vdir vd;
		} u;

		printk("br{"
		       "xino %d, readf %d, writef %d, "
		       "id %d, perm %d, sb %d, mnt %d, count %d, "
		       "wh_sem %d, wh %d, run %d} %d\n",
		       offsetof(typeof(u.br), br_xino),
		       offsetof(typeof(u.br), br_xino_read),
		       offsetof(typeof(u.br), br_xino_write),
		       offsetof(typeof(u.br), br_id),
		       offsetof(typeof(u.br), br_perm),
		       offsetof(typeof(u.br), br_sb),
		       offsetof(typeof(u.br), br_mnt),
		       offsetof(typeof(u.br), br_count),
		       offsetof(typeof(u.br), br_wh_rwsem),
		       offsetof(typeof(u.br), br_wh),
		       offsetof(typeof(u.br), br_wh_running),
		       sizeof(u.br));
		printk("di{gen %d, rwsem %d, bstart %d, bend %d, bwh %d, "
		       "bdiropq %d, dentry %d, reval %d} %d\n",
		       offsetof(typeof(u.di), di_generation),
		       offsetof(typeof(u.di), di_rwsem),
		       offsetof(typeof(u.di), di_bstart),
		       offsetof(typeof(u.di), di_bend),
		       offsetof(typeof(u.di), di_bwh),
		       offsetof(typeof(u.di), di_bdiropq),
		       offsetof(typeof(u.di), di_dentry),
		       offsetof(typeof(u.di), di_reval),
		       sizeof(u.di));
		printk("fi{gen %d, rwsem %d, hfile %d, bstart %d, bend %d, "
		       "mapcnt %d, vm_ops %d, vdir_cach %d} %d\n",
		       offsetof(typeof(u.fi), fi_generation),
		       offsetof(typeof(u.fi), fi_rwsem),
		       offsetof(typeof(u.fi), fi_hfile),
		       offsetof(typeof(u.fi), fi_bstart),
		       offsetof(typeof(u.fi), fi_bend),
		       offsetof(typeof(u.fi), fi_hidden_vm_ops),
		       offsetof(typeof(u.fi), fi_vdir_cache),
		       sizeof(u.fi));
		printk("ii{rwsem %d, bstart %d, bend %d, hinode %d, vdir %d} "
		       "%d\n",
		       offsetof(typeof(u.ii), ii_rwsem),
		       offsetof(typeof(u.ii), ii_bstart),
		       offsetof(typeof(u.ii), ii_bend),
		       offsetof(typeof(u.ii), ii_hinode),
		       offsetof(typeof(u.ii), ii_vdir),
		       sizeof(u.ii));
		printk("hi{inode %d, id %d, notify %d} %d\n",
		       offsetof(typeof(u.hi), hi_inode),
		       offsetof(typeof(u.hi), hi_id),
		       offsetof(typeof(u.hi), hi_notify),
		       sizeof(u.hi));
		printk("si{rwsem %d, gen %d, "
		       "failed_refresh %d, "
		       "bend %d, last id %d, br %d, "
		       "flags %d, "
		       "xino %d, "
		       "rdcache %d, "
		       "dirwh %d, "
		       "pl_lock %d, pl %d, "
		       "kobj %d} %d\n",
		       offsetof(typeof(u.si), si_rwsem),
		       offsetof(typeof(u.si), si_generation),
		       -1,//offsetof(typeof(u.si), si_failed_refresh_dirs),
		       offsetof(typeof(u.si), si_bend),
		       offsetof(typeof(u.si), si_last_br_id),
		       offsetof(typeof(u.si), si_branch),
		       offsetof(typeof(u.si), si_flags),
		       offsetof(typeof(u.si), si_xino),
		       offsetof(typeof(u.si), si_rdcache),
		       offsetof(typeof(u.si), si_dirwh),
		       offsetof(typeof(u.si), si_plink_lock),
		       offsetof(typeof(u.si), si_plink),
		       offsetof(typeof(u.si), si_kobj),
		       sizeof(u.si));
		printk("destr{len %d, name %d} %d\n",
		       offsetof(typeof(u.destr), len),
		       offsetof(typeof(u.destr), name),
		       sizeof(u.destr));
		printk("de{ino %d, type %d, str %d} %d\n",
		       offsetof(typeof(u.de), de_ino),
		       offsetof(typeof(u.de), de_type),
		       offsetof(typeof(u.de), de_str),
		       sizeof(u.de));
		printk("wh{hash %d, bindex %d, str %d} %d\n",
		       offsetof(typeof(u.wh), wh_hash),
		       offsetof(typeof(u.wh), wh_bindex),
		       offsetof(typeof(u.wh), wh_str),
		       sizeof(u.wh));
		printk("vd{deblk %d, nblk %d, last %d, ver %d, jiffy %d} %d\n",
		       offsetof(typeof(u.vd), vd_deblk),
		       offsetof(typeof(u.vd), vd_nblk),
		       offsetof(typeof(u.vd), vd_last),
		       offsetof(typeof(u.vd), vd_version),
		       offsetof(typeof(u.vd), vd_jiffy),
		       sizeof(u.vd));
	}
#endif
#endif

	err = -EINVAL;
	if (aufs_nwkq <= 0)
		goto out;
	err = create_cache();
	if (err)
		goto out;
	err = init_wkq();
	if (err)
		goto out_cache;
	//err = init_kobj();
	if (err)
		goto out_wkq;
	err = aufs_inotify_init();
	if (err)
		goto out_kobj;
	err = register_filesystem(&aufs_fs_type);
	if (err)
		goto out_inotify;
	printk(AUFS_NAME " " AUFS_VERSION "\n");
	return 0; /* success */

 out_inotify:
	aufs_inotify_exit();
 out_kobj:
	//fin_kobj();
 out_wkq:
	fin_wkq();
 out_cache:
	destroy_cache();
 out:
	TraceErr(err);
	return err;
}

static void __exit aufs_exit(void)
{
	unregister_filesystem(&aufs_fs_type);
	aufs_inotify_exit();
	//fin_kobj();
	fin_wkq();
	destroy_cache();
}

module_init(aufs_init);
module_exit(aufs_exit);

// fake Kconfig
#if 1
#ifdef CONFIG_AUFS_HINOTIFY
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#error CONFIG_AUFS_HINOTIFY is supported in linux-2.6.18 and later.
#endif
#ifndef CONFIG_INOTIFY
#error enable CONFIG_INOTIFY to use CONFIG_AUFS_HINOTIFY.
#endif
#endif

#if defined(CONFIG_AUFS_IPRIV_PATCH) && !defined(CONFIG_SECURITY)
#error CONFIG_AUFS_IPRIV_PATCH requires CONFIG_SECURITY.
#endif

#ifdef CONFIG_PROVE_LOCKING
#warning CONFIG_PROVE_LOCKING will produce msgs to innocent locks.
#endif

#ifdef CONFIG_AUFS_COMPAT
#warning CONFIG_AUFS_COMPAT will be removed in the near future.
#endif

#endif
