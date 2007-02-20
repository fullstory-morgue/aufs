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

/* $Id: hinotify.c,v 1.7 2007/02/19 03:27:45 sfjro Exp $ */

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
		wd = inotify_add_watch(in_handle, &hin->hin_watch, hidden_inode,
				       in_mask);
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
		if (atomic_read(&hinode->hi_notify->hin_watch.count))
			err = inotify_rm_watch(in_handle,
					       &hinode->hi_notify->hin_watch);
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

void hdir_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex,
	       	struct aufs_h_ipriv *ipriv)
{
	struct aufs_hinode *hinode;

	LKTRTrace("i%lu, b%d\n", dir->i_ino, bindex);
	DEBUG_ON(!S_ISDIR(dir->i_mode));

	hinode = itoii(dir)->ii_hinode + bindex;
	DEBUG_ON(h_dir != hinode->hi_inode);
	i_lock_priv(hinode->hi_inode, ipriv, dir->i_sb);
	if (1 /* unlikely(IS_MS(dir->i_sb, MS_UDBA_HINOTIFY) */)
		suspend_hinotify(hinode);
}

void hdir_unlock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex,
		 struct aufs_h_ipriv *ipriv)
{
	struct aufs_hinode *hinode;

	LKTRTrace("i%lu, b%d\n", dir->i_ino, bindex);
	DEBUG_ON(!S_ISDIR(dir->i_mode));

	hinode = itoii(dir)->ii_hinode + bindex;
	DEBUG_ON(h_dir != hinode->hi_inode);
	if (1 /* unlikely(IS_MS(dir->i_sb, MS_UDBA_HINOTIFY) */)
	    resume_hinotify(hinode);
	i_unlock_priv(hinode->hi_inode, ipriv);
}

void hdir_lock_rename(struct dentry **h_parents, struct inode **dirs,
		      aufs_bindex_t bindex, int issamedir,
		      struct aufs_h_ipriv *iprivs)
{
	struct aufs_hinode *hinode;

	LKTRTrace("%.*s, %.*s\n", DLNPair(h_parents[0]), DLNPair(h_parents[1]));

	i_lock_rename_priv(h_parents, iprivs, dirs[0]->i_sb);
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
			aufs_bindex_t bindex, int issamedir,
			struct aufs_h_ipriv *iprivs)
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
	i_unlock_rename_priv(h_parents, iprivs);
}

void reset_hinotify(struct inode *inode, unsigned int flags)
{
	aufs_bindex_t bindex, bend;
	struct inode *hi;

	LKTRTrace("i%lu, 0x%x\n", inode->i_ino, flags);

	bend = ibend(inode);
	for (bindex = 0; bindex <= bend; bindex++) {
		hi = h_iptr_i(inode, bindex);
		if (hi) {
			igrab(hi);
			set_h_iptr(inode, bindex, NULL, 0);
			set_h_iptr(inode, bindex, igrab(hi), flags);
			iput(hi);
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
		direval_inc(alias);
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
				direval_inc(child);
			} else
				reval_alias(child->d_inode);

			// todo: the i_nlink of newly created name by link(2)
			// should be updated
			// todo: some nfs dentry doesn't notifyed at deleteing
			break;
		}
	}

	TraceErr(err);
	return err;
}

static void aufs_inotify(struct inotify_watch *watch, u32 wd, u32 mask,
			 u32 cookie, const char *name,
			 struct inode *hidden_inode)
{
	struct aufs_hinotify *hinotify;
	struct inode *inode;
	struct dentry *parent;

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
	Dbg("wd %d, mask 0x%x %s, cookie 0x%x, name %s, hi%lu\n",
		  wd, mask, in_name(mask), cookie,
		  name, hidden_inode ? hidden_inode->i_ino : 0);
	//WARN_ON(1);
#endif

	// todo: try parent = d_find_alias(inode);
	spin_lock(&dcache_lock);
	parent = list_entry(inode->i_dentry.next, struct dentry, d_alias);
	//Dbg("%.*s\n", DLNPair(parent));
	direval_inc(parent);
	if (name && *name)
		reval_child(name, parent);
	spin_unlock(&dcache_lock);

	/*
	 * special handling root directory,
	 * sine d_revalidate may not be called later.
	 * main purpose is maintaining i_nlink.
	 */
	if (unlikely(IS_ROOT(parent))) {
		int locked;
		//Dbg("here\n");
		//WARN_ON(1);
		// todo: this is a bug. fix it.
		locked = ii_is_write_lock(inode);
		if (!locked) {
			ii_write_unlock(inode);
			aufs_read_lock(parent, AUFS_D_WLOCK);
			//ii_read_lock(inode);
		}
		atomic_set(&dtodi(parent)->di_reval, 0);
		cpup_attr_all(inode);
		if (!locked)
			//ii_read_unlock(inode);
			aufs_read_unlock(parent, AUFS_D_WLOCK);
	}

#if 0
	if (dentry) {
		struct aufs_vdir *vdir;
		aufs_read_lock(dentry, AUFS_D_WLOCK);
		vdir = ivdir(inode);
		if (vdir)
			vdir->vd_jiffy = 0;
		aufs_read_unlock(dentry, AUFS_D_WLOCK);
	}
#endif
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

int __init aufs_inotify_init(void)
{
	in_handle = inotify_init(&aufs_inotify_ops);
	if (!IS_ERR(in_handle))
		return 0;
	TraceErrPtr(in_handle);
	return PTR_ERR(in_handle);
}

void __exit aufs_inotify_exit(void)
{
	inotify_destroy(in_handle);
}
