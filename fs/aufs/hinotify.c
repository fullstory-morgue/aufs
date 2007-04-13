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

/* $Id: hinotify.c,v 1.14 2007/04/09 02:47:10 sfjro Exp $ */

#include "aufs.h"

static struct inotify_handle *in_handle;
static const __u32 in_mask = (IN_MOVE | IN_DELETE | IN_CREATE /* | IN_ACCESS */
			      | IN_MODIFY | IN_ATTRIB
			      | IN_DELETE_SELF | IN_MOVE_SELF);

int alloc_hinotify(struct aufs_hinode *hinode, struct inode *inode,
		   struct inode *hidden_inode)
{
	int err;
	struct aufs_hinotify *hin;
	s32 wd;

	LKTRTrace("i%lu, hi%lu\n", inode->i_ino, hidden_inode->i_ino);

	err = -ENOMEM;
	hin = cache_alloc_hinotify();
	if (hin) {
		DEBUG_ON(hinode->hi_notify);
		hinode->hi_notify = hin;
		hin->hin_aufs_inode = inode;
		inotify_init_watch(&hin->hin_watch);
		wd = inotify_add_watch(in_handle, &hin->hin_watch, hidden_inode,
				       in_mask);
		if (wd >= 0)
			return 0; /* success */

		err = wd;
		put_inotify_watch(&hin->hin_watch);
		cache_free_hinotify(hin);
		hinode->hi_notify = NULL;
	}

	TraceErr(err);
	return err;
}

void do_free_hinotify(struct aufs_hinode *hinode)
{
	int err;
	struct aufs_hinotify *hin;

	TraceEnter();

	hin = hinode->hi_notify;
	if (hin) {
		err = 0;
		if (atomic_read(&hin->hin_watch.count))
			err = inotify_rm_watch(in_handle, &hin->hin_watch);

		if (!err) {
			cache_free_hinotify(hin);
			hinode->hi_notify = NULL;
		} else
			IOErr1("failed inotify_rm_watch() %d\n", err);
	}
}

/* ---------------------------------------------------------------------- */

static void ctl_hinotify(struct aufs_hinode *hinode, const __u32 mask)
{
	struct inode *hi;
	struct inotify_watch *watch;

	hi = hinode->hi_inode;
	LKTRTrace("hi%lu, sb %p, %u\n", hi->i_ino, hi->i_sb, mask);
	if (0 && !strcmp(current->comm, "link"))
		dump_stack();
	IMustLock(hi);
	if (!hinode->hi_notify)
		return;

	watch = &hinode->hi_notify->hin_watch;
#if 0
	{
		u32 wd;
		wd = inotify_find_update_watch(in_handle, hi, mask);
		TraceErr(wd);
		// ignore an err;
	}
#else
	watch->mask = mask;
	smp_mb();
#endif
	LKTRTrace("watch %p, mask %u\n", watch, watch->mask);
}

#define suspend_hinotify(hi)	ctl_hinotify(hi, 0)
#define resume_hinotify(hi)	ctl_hinotify(hi, in_mask)

void do_hdir_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex,
		  unsigned int lsc)
{
	struct aufs_hinode *hinode;

	LKTRTrace("i%lu, b%d, lsc %d\n", dir->i_ino, bindex, lsc);
	DEBUG_ON(!S_ISDIR(dir->i_mode));
	hinode = itoii(dir)->ii_hinode + bindex;
	DEBUG_ON(h_dir != hinode->hi_inode);

	hi_lock(h_dir, lsc);
	if (1 /* unlikely(au_flag_test(dir->i_sb, AuFlag_UDBA_HINOTIFY) */)
		suspend_hinotify(hinode);
}

void hdir_unlock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex)
{
	struct aufs_hinode *hinode;

	LKTRTrace("i%lu, b%d\n", dir->i_ino, bindex);
	DEBUG_ON(!S_ISDIR(dir->i_mode));
	hinode = itoii(dir)->ii_hinode + bindex;
	DEBUG_ON(h_dir != hinode->hi_inode);

	if (1 /* unlikely(au_flag_test(dir->i_sb, AuFlag_UDBA_HINOTIFY) */)
	    resume_hinotify(hinode);
	i_unlock(h_dir);
}

void hdir_lock_rename(struct dentry **h_parents, struct inode **dirs,
		      aufs_bindex_t bindex, int issamedir)
{
	struct aufs_hinode *hinode;

	LKTRTrace("%.*s, %.*s\n", DLNPair(h_parents[0]), DLNPair(h_parents[1]));

	vfsub_lock_rename(h_parents[0], h_parents[1]);
	hinode = itoii(dirs[0])->ii_hinode + bindex;
	DEBUG_ON(h_parents[0]->d_inode != hinode->hi_inode);
	suspend_hinotify(hinode);
	if (issamedir)
		return;
	hinode = itoii(dirs[1])->ii_hinode + bindex;
	DEBUG_ON(h_parents[1]->d_inode != hinode->hi_inode);
	suspend_hinotify(hinode);
}

void hdir_unlock_rename(struct dentry **h_parents, struct inode **dirs,
			aufs_bindex_t bindex, int issamedir)
{
	struct aufs_hinode *hinode;

	LKTRTrace("%.*s, %.*s\n", DLNPair(h_parents[0]), DLNPair(h_parents[1]));

	hinode = itoii(dirs[0])->ii_hinode + bindex;
	DEBUG_ON(h_parents[0]->d_inode != hinode->hi_inode);
	resume_hinotify(hinode);
	if (!issamedir) {
		hinode = itoii(dirs[1])->ii_hinode + bindex;
		DEBUG_ON(h_parents[1]->d_inode != hinode->hi_inode);
		resume_hinotify(hinode);
	}
	vfsub_unlock_rename(h_parents[0], h_parents[1]);
}

void au_reset_hinotify(struct inode *inode, unsigned int flags)
{
	aufs_bindex_t bindex, bend;
	struct inode *hi;

	LKTRTrace("i%lu, 0x%x\n", inode->i_ino, flags);

	bend = ibend(inode);
	for (bindex = ibstart(inode); bindex <= bend; bindex++) {
		hi = au_h_iptr_i(inode, bindex);
		if (hi) {
			//hi_lock(hi, AUFS_LSC_H_CHILD);
			igrab(hi);
			set_h_iptr(inode, bindex, NULL, 0);
			set_h_iptr(inode, bindex, igrab(hi), flags);
			iput(hi);
			//i_unlock(hi);
		}
	}
}

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_DEBUG
static char *in_name(u32 mask)
{
#define test_ret(flag)	if (mask & flag) return #flag;
	test_ret(IN_ACCESS);
	test_ret(IN_MODIFY);
	test_ret(IN_ATTRIB);
	test_ret(IN_CLOSE_WRITE);
	test_ret(IN_CLOSE_NOWRITE);
	test_ret(IN_OPEN);
	test_ret(IN_MOVED_FROM);
	test_ret(IN_MOVED_TO);
	test_ret(IN_CREATE);
	test_ret(IN_DELETE);
	test_ret(IN_DELETE_SELF);
	test_ret(IN_MOVE_SELF);
	test_ret(IN_UNMOUNT);
	test_ret(IN_Q_OVERFLOW);
	test_ret(IN_IGNORED);
	return "";
#undef test_ret
}
#else
#define in_name(m) "??"
#endif

static void do_drop(struct dentry *dentry)
{
	spin_lock(&dentry->d_lock);
 	__d_drop(dentry);
	spin_unlock(&dentry->d_lock);
}

static void reval_alias(struct inode *inode)
{
	struct dentry *alias;

	list_for_each_entry(alias, &inode->i_dentry, d_alias) {
		//Dbg("%.*s\n", DLNPair(alias));
#if 1
		do_drop(alias);
#else
		au_direval_inc(alias);
#endif
	}
}

static int reval_child(const char *name, struct dentry *parent)
{
	int err;
	struct dentry *child;
	struct inode *inode;
	struct qstr *child_name;
	const int len = strlen(name);

	err = 0;
	spin_lock(&dcache_lock);
	list_for_each_entry(child, &parent->d_subdirs, d_u.d_child) {
		inode = child->d_inode;
		child_name = &child->d_name;
		if (len == child_name->len
		    && !memcmp(child_name->name, name, len)) {
			if (!inode || S_ISDIR(inode->i_mode)) {
				//Dbg("%.*s\n", LNPair(child_name));
#if 1
				do_drop(child);
#else
				au_direval_inc(child);
#endif
			} else
				reval_alias(child->d_inode);

			// todo: the i_nlink of newly created name by link(2)
			// should be updated
			// todo: some nfs dentry doesn't notified at deleteing
			break;
		}
	}
	spin_unlock(&dcache_lock);

	TraceErr(err);
	return err;
}

struct postproc_args {
	struct inode *h_dir, *dir;
	struct dentry *dentry;
	u32 mask;
	int self;
};

static void postproc(void *args)
{
	struct postproc_args *a = args;
	struct super_block *sb;
	aufs_bindex_t bindex, bstart, bend, bfound;
	ino_t ino, h_ino;
	int err;

	//atomic_inc(&aufs_cond);
	TraceEnter();
	DEBUG_ON(!a->dir);
#ifdef ForceInotify
	if (a->dentry) {
		if (strncmp(a->dentry->d_name.name, "test_fsuidsig",
			    strlen("test_fsuidsig")))
			Dbg("UDBA %.*s\n", DLNPair(a->dentry));
	} else
		Dbg("UDBA i%lu\n", a->dir->i_ino);
#endif

	i_lock(a->dir);
	sb = a->dir->i_sb;
	if (a->dentry) {
		aufs_read_lock(a->dentry, AUFS_D_WLOCK);
	} else {
		si_read_lock(sb);
		ii_write_lock_parent(a->dir);
	}

	/* make dir entries obsolete */
	if (!a->self) {
		struct aufs_vdir *vdir;
		vdir = ivdir(a->dir);
		if (vdir)
			vdir->vd_jiffy = 0;
	}

	/*
	 * special handling root directory,
	 * sine d_revalidate may not be called later.
	 * main purpose is maintaining i_nlink.
	 */
	if (unlikely(a->dir->i_ino == AUFS_ROOT_INO)) {
		au_cpup_attr_all(a->dir);
		atomic_set(&dtodi(sb->s_root)->di_reval, 0);
		goto out;
	}

	/* reset xino for hidden directories */
	//if (!(a->mask & (IN_MOVE_SELF | IN_DELETE_SELF))
	if (!a->self
	    || ibstart(a->dir) == ibend(a->dir)) {
		goto out;
	}
	DEBUG_ON(!a->h_dir);

	bfound = -1;
	bstart = ibstart(a->dir);
	bend = ibend(a->dir);
	for (bindex = bstart; bfound == -1 && bindex <= bend; bindex++)
		if (au_h_iptr_i(a->dir, bindex) == a->h_dir)
			bfound = bindex;
	DEBUG_ON(bfound == -1);

	if (au_flag_test(sb, AuFlag_XINO) && a->dir->i_ino != AUFS_ROOT_INO) {
		h_ino = a->h_dir->i_ino;
		set_h_iptr(a->dir, bfound, NULL, /*flags*/0);
		err = xino_read(sb, bfound, h_ino, &ino, /*force*/0);
		if (!err && ino == h_ino)
			xino_write(sb, bfound, h_ino, 0);
		/* ignore an error */

		if (bfound == bstart) {
			for (bindex = bstart + 1; bindex <= bend; bindex++)
				if (au_h_iptr_i(a->dir, bindex)) {
					set_ibstart(a->dir, bindex);
					break;
			}
			DEBUG_ON(!au_h_iptr(a->dir));
		} else if (bfound == bend) {
			for (bindex = bend - 1; bindex >= bstart; bindex--)
				if (au_h_iptr_i(a->dir, bindex)) {
					set_ibend(a->dir, bindex);
					break;
				}
			DEBUG_ON(!au_h_iptr_i(a->dir, ibend(a->dir)));
		}
	}

#if 0
	if (!a->dentry)
		goto out;
	bstart = dbstart(a->dentry);
	bend = dbend(a->dentry);
	set_h_dptr(a->dentry, bfound, NULL);
	if (bfound == bstart) {
		for (bindex = bstart + 1; bindex <= bend; bindex++)
			if (au_h_dptr_i(a->dentry, bindex)) {
				set_dbstart(a->dentry, bindex);
				break;
			}
		DEBUG_ON(!au_h_dptr(a->dentry));
	} else if (bfound == bend) {
		for (bindex = bend - 1; bindex >= bstart; bindex--)
			if (au_h_dptr_i(a->dentry, bindex)) {
				set_dbend(a->dentry, bindex);
				break;
			}
		DEBUG_ON(!au_h_dptr_i(a->dentry, dbend(a->dentry)));
	}
#endif

 out:
	if (a->dentry) {
		if (a->mask & (IN_MOVE_SELF | IN_DELETE_SELF))
			d_drop(a->dentry);
		aufs_read_unlock(a->dentry, AUFS_D_WLOCK);
		dput(a->dentry);
	} else {
		ii_write_unlock(a->dir);
		si_read_unlock(sb);
	}
	i_unlock(a->dir);

	iput(a->h_dir);
	iput(a->dir);
	kfree(a);
	//atomic_dec(&aufs_cond);
}

static void aufs_inotify(struct inotify_watch *watch, u32 wd, u32 mask,
			 u32 cookie, const char *name, struct inode *h_inode)
{
	struct aufs_hinotify *hinotify;
	struct inode *dir;
	struct dentry *parent;
	struct postproc_args *args;
	static DECLARE_WAIT_QUEUE_HEAD(wq);

	LKTRTrace("wd %d, mask 0x%x %s, cookie 0x%x, name %s, hi%lu\n",
		  wd, mask, in_name(mask), cookie,
		  name ? name : "", h_inode ? h_inode->i_ino : 0);
	//IMustLock(h_dir);

	if (mask & IN_IGNORED) {
		put_inotify_watch(watch);
		return;
	}

	switch (mask & IN_ALL_EVENTS) {
	case IN_MODIFY:
	case IN_ATTRIB:
		if (name)
			return;
		break;

	case IN_MOVED_FROM:
	case IN_MOVED_TO:
	case IN_CREATE:
	case IN_DELETE:
		DEBUG_ON(!name);
		break;

	case IN_DELETE_SELF:
	case IN_MOVE_SELF:
		DEBUG_ON(name);
		break;

	case IN_ACCESS:
	default:
		BUG();
	}

#ifdef DbgInotify
	{
		static pid_t last;
		if (1 || last != current->pid) {
			Dbg("wd %d, mask 0x%x %s, cookie 0x%x, name %s, hi%lu\n",
			    wd, mask, in_name(mask), cookie,
			    name ? name : "", h_inode ? h_inode->i_ino : 0);
			WARN_ON(1);
			last = current->pid;
		}
	}
#endif

	/* iput() and dput() will be called in postproc() */
	hinotify = container_of(watch, struct aufs_hinotify, hin_watch);
	DEBUG_ON(!hinotify);
	dir = igrab(hinotify->hin_aufs_inode);
	if (!dir)
		return;

	/* force re-lookup in next d_revalidate() */
	parent = d_find_alias(dir);
	if (parent) {
		au_direval_inc(parent);
		if (name)
			reval_child(name, parent);
#if 0
		atomic_inc(&aufs_cond);
		DbgDentry(parent);
		atomic_dec(&aufs_cond);
#endif
	}

	/* post process in another context */
	wait_event(wq, (args = kmalloc(sizeof(*args), GFP_KERNEL)));
	args->h_dir = igrab(watch->inode);
	args->dir = dir;
	args->dentry = parent;
	args->mask = mask;
	args->self = !!name;
	wkq_nowait(postproc, args, /*dlgt*/0);
}

static void aufs_inotify_destroy(struct inotify_watch *watch)
{
	return;
}

static struct inotify_operations aufs_inotify_ops = {
	.handle_event	= aufs_inotify,
	.destroy_watch	= aufs_inotify_destroy
};

/* ---------------------------------------------------------------------- */

int __init au_inotify_init(void)
{
	in_handle = inotify_init(&aufs_inotify_ops);
	if (!IS_ERR(in_handle))
		return 0;
	TraceErrPtr(in_handle);
	return PTR_ERR(in_handle);
}

void au_inotify_exit(void)
{
	inotify_destroy(in_handle);
}
