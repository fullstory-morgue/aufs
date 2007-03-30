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

/* $Id: hinotify.c,v 1.12 2007/03/27 12:49:17 sfjro Exp $ */

#include <linux/kthread.h>
#include "aufs.h"

static struct inotify_handle *in_handle;
static const __u32 in_mask = (IN_MOVE | IN_DELETE | IN_CREATE /* | IN_ACCESS */
			      | IN_MODIFY | IN_ATTRIB);

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
		hin->hin_aufs_inode = inode;
		inotify_init_watch(&hin->hin_watch);
		/* inotify doesn't support lockdep */
		/* but this curcular lock may be a problem */
		lockdep_off();
		wd = inotify_add_watch(in_handle, &hin->hin_watch, hidden_inode,
				       in_mask);
		lockdep_on();
		if (wd >= 0) {
			hinode->hi_notify = hin;
			return 0; /* success */
		}

		err = wd;
		put_inotify_watch(&hin->hin_watch);
		cache_free_hinotify(hin);
	}

	TraceErr(err);
	return err;
}

void do_free_hinotify(struct aufs_hinode *hinode)
{
	int err;

	TraceEnter();

	if (hinode->hi_notify) {
		err = 0;
		if (atomic_read(&hinode->hi_notify->hin_watch.count)) {
			/* inotify doesn't support lockdep */
			/* but this curcular lock may be a problem */
			lockdep_off();
			err = inotify_rm_watch(in_handle,
					       &hinode->hi_notify->hin_watch);
			lockdep_on();
		}
		if (!err) {
			cache_free_hinotify(hinode->hi_notify);
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

static void reval_alias(struct inode *inode)
{
	struct dentry *alias;

	list_for_each_entry(alias, &inode->i_dentry, d_alias) {
		//Dbg("%.*s\n", DLNPair(alias));
		au_direval_inc(alias);
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
	list_for_each_entry(child, &parent->d_subdirs, d_u.d_child) {
		inode = child->d_inode;
		child_name = &child->d_name;
		if (len == child_name->len
		    && !memcmp(child_name->name, name, len)) {
			if (!inode || S_ISDIR(inode->i_mode)) {
				//Dbg("%.*s\n", LNPair(child_name));
				au_direval_inc(child);
			} else
				reval_alias(child->d_inode);

			// todo: the i_nlink of newly created name by link(2)
			// should be updated
			// todo: some nfs dentry doesn't notified at deleteing
			break;
		}
	}

	TraceErr(err);
	return err;
}

struct reset_dirino_args {
	struct dentry *parent;
	char *name;
	struct inode *h_inode;
};

static int reset_dirino(void *args)
{
	int err;
	struct reset_dirino_args *a = args;
	struct inode *inode, *dir = a->parent->d_inode;
	struct dentry *child;
	aufs_bindex_t bindex, bend, new_bstart;
	struct super_block *sb = a->parent->d_sb;
	struct lkup_args lkup = {
		.nfsmnt	= NULL,
		.dlgt	= need_dlgt(sb)
	};

	LKTRTrace("%s\n", a->name);

	i_lock(dir);
	child = lkup_one(a->name, a->parent, strlen(a->name), &lkup);
	i_unlock(dir);
	err = PTR_ERR(child);
	if (unlikely(!child || IS_ERR(child)))
		goto out;

	/* reset ino */
	err = 0;
	aufs_read_lock(child, AUFS_I_WLOCK);
	inode = child->d_inode;
	if (unlikely(!inode))
		goto out_drop;

	new_bstart = -1;
	bend = ibend(inode);
	for (bindex = ibstart(inode); bindex <= bend; bindex++) {
		struct inode *h_inode;
		h_inode = au_h_iptr_i(inode, bindex);
		if (unlikely(!h_inode))
			continue;
		if (h_inode != a->h_inode) {
			ino_t ino, h_ino = h_inode->i_ino;
			set_h_iptr(inode, bindex, NULL, /*flags*/0);
			err = xino_read(sb, bindex, h_ino, &ino, /*force*/0);
			if (!err && ino == h_ino)
				xino_write(sb, bindex, h_ino, 0);
			/* ignore an error */
		} else
			new_bstart = bindex;
	}
	if (new_bstart != -1) {
		set_ibend(inode, new_bstart);
		set_ibstart(inode, new_bstart);
	}

 out_drop:
	d_drop(child);
	aufs_read_unlock(child, AUFS_I_WLOCK);
	dput(child);
 out:
	iput(a->h_inode);
	kfree(a->name);
	dput(a->parent);
	kfree(a);
	if (unlikely(err))
		IOErr("err %d\n", err);
	do_exit(err);
	return err;
}

static void aufs_inotify(struct inotify_watch *watch, u32 wd, u32 mask,
			 u32 cookie, const char *name,
			 struct inode *hidden_inode)
{
	struct aufs_hinotify *hinotify;
	struct inode *inode;
	struct dentry *parent;
	struct super_block *sb;
	struct aufs_vdir *vdir;

	LKTRTrace("wd %d, mask 0x%x %s, cookie 0x%x, name %s, hi%lu\n",
		  wd, mask, in_name(mask), cookie,
		  name, hidden_inode ? hidden_inode->i_ino : 0);
	//IMustLock(hidden_inode);
	hinotify = container_of(watch, struct aufs_hinotify, hin_watch);
	DEBUG_ON(!hinotify);
	inode = hinotify->hin_aufs_inode;
	DEBUG_ON(!inode || !S_ISDIR(inode->i_mode));

	if (mask & IN_IGNORED) {
		put_inotify_watch(watch);
		return;
	}

#ifdef DbgInotify
	{
		static pid_t last;
		if (1 || last != current->pid) {
			Dbg("wd %d, mask 0x%x %s, cookie 0x%x, name %s, hi%lu\n",
			    wd, mask, in_name(mask), cookie, name,
			    hidden_inode ? hidden_inode->i_ino : 0);
			WARN_ON(1);
			last = current->pid;
		}
	}
#endif

	// todo: try parent = d_find_alias(inode);
	spin_lock(&dcache_lock);
	parent = list_entry(inode->i_dentry.next, struct dentry, d_alias);
	//Dbg("%.*s\n", DLNPair(parent));
	au_direval_inc(parent);
	if (name && *name)
		reval_child(name, parent);
	spin_unlock(&dcache_lock);

	sb = inode->i_sb;
#if 1 //def DbgInotify
	// todo: fix it
	lockdep_off();
#endif
	aufs_read_lock(parent, AUFS_I_WLOCK);
// todo: stop unnecessary update
	vdir = ivdir(inode);
	if (vdir)
		vdir->vd_jiffy = 0;

	/*
	 * special handling root directory,
	 * sine d_revalidate may not be called later.
	 * main purpose is maintaining i_nlink.
	 */
	if (unlikely(IS_ROOT(parent))) {
		atomic_set(&dtodi(parent)->di_reval, 0);
		/* this condition is not correct. but its ok for a while */
		if (hidden_inode && sbr_sb(sb, 0) == hidden_inode->i_sb)
			au_cpup_attr_all(inode);
	}
	aufs_read_unlock(parent, AUFS_I_WLOCK);
#if 1 //def DbgInotify
	lockdep_on();
#endif

	/* reset xino for hidden directories */
	if (unlikely(hidden_inode
		     && S_ISDIR(hidden_inode->i_mode)
		     && (mask & IN_MOVED_FROM)
		     && au_flag_test(sb, AuFlag_XINO))) {
		int err;
		static DECLARE_WAIT_QUEUE_HEAD(wq);
		struct task_struct *tsk;
		struct reset_dirino_args *args;

		err = -ENOMEM;
		args = kmalloc(sizeof(*args), GFP_KERNEL);
		if (unlikely(!args))
			goto out;
		args->name = kstrdup(name, GFP_KERNEL);
		if (unlikely(!args->name)) {
			kfree(args);
			goto out;
		}

		args->parent = dget(parent);
		args->h_inode = igrab(hidden_inode);
		smp_mb();
		wait_event(wq, !IS_ERR(tsk = kthread_run
				       (reset_dirino, args,
					AUFS_NAME "_reset_dirino")));
		return;
	out:
		Err("failed resetting dir ino(%d)\n", err);
	}
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
