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

/* $Id: i_op_ren.c,v 1.31 2007/03/27 12:51:43 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
#include "aufs.h"

enum {SRC, DST};
struct rename_args {
	struct dentry *hidden_dentry[2], *parent[2], *hidden_parent[2];
	struct aufs_nhash whlist;
	aufs_bindex_t btgt, bstart[2];
	struct super_block *sb;

	unsigned int isdir:1;
	unsigned int issamedir:1;
	unsigned int whsrc:1;
	unsigned int whdst:1;
	unsigned int dlgt:1;
};

static int do_rename(struct inode *src_dir, struct dentry *src_dentry,
		     struct inode *dir, struct dentry *dentry,
		     struct rename_args *a)
{
	int err, need_diropq, bycpup, rerr;
	struct rmdir_whtmp_arg *tharg;
	struct dentry *wh_dentry[2], *hidden_dst, *hg_parent;
	struct inode *hidden_dir[2];
	aufs_bindex_t bindex, bend;
	unsigned int flags;
	struct lkup_args lkup = {.dlgt = a->dlgt};

	LKTRTrace("%.*s/%.*s, %.*s/%.*s, "
		  "hd{%p, %p}, hp{%p, %p}, wh %p, btgt %d, bstart{%d, %d}, "
		  "flags{%d, %d, %d, %d}\n",
		  DLNPair(a->parent[SRC]), DLNPair(src_dentry),
		  DLNPair(a->parent[DST]), DLNPair(dentry),
		  a->hidden_dentry[SRC], a->hidden_dentry[DST],
		  a->hidden_parent[SRC], a->hidden_parent[DST],
		  &a->whlist, a->btgt,
		  a->bstart[SRC], a->bstart[DST],
		  a->isdir, a->issamedir, a->whsrc, a->whdst);
	hidden_dir[SRC] = a->hidden_parent[SRC]->d_inode;
	hidden_dir[DST] = a->hidden_parent[DST]->d_inode;
	IMustLock(hidden_dir[SRC]);
	IMustLock(hidden_dir[DST]);

	/* prepare workqueue arg */
	hidden_dst = NULL;
	tharg = NULL;
	if (a->isdir && a->hidden_dentry[DST]->d_inode) {
		err = -ENOMEM;
		tharg = kmalloc(sizeof(*tharg), GFP_KERNEL);
		//tharg = NULL;
		if (unlikely(!tharg))
			goto out;
		hidden_dst = dget(a->hidden_dentry[DST]);
	}

	wh_dentry[SRC] = wh_dentry[DST] = NULL;
	lkup.nfsmnt = au_nfsmnt(a->sb, a->btgt);
	/* create whiteout for src_dentry */
	if (a->whsrc) {
		wh_dentry[SRC] = simple_create_wh(src_dentry, a->btgt,
						  a->hidden_parent[SRC], &lkup);
		//wh_dentry[SRC] = ERR_PTR(-1);
		err = PTR_ERR(wh_dentry[SRC]);
		if (IS_ERR(wh_dentry[SRC]))
			goto out_tharg;
	}

	/* lookup whiteout for dentry */
	if (a->whdst) {
		struct dentry *d;
		d = lkup_wh(a->hidden_parent[DST], &dentry->d_name, &lkup);
		//d = ERR_PTR(-1);
		err = PTR_ERR(d);
		if (IS_ERR(d))
			goto out_whsrc;
		if (!d->d_inode)
			dput(d);
		else
			wh_dentry[DST] = d;
	}

	/* rename dentry to tmpwh */
	if (tharg) {
		err = rename_whtmp(dentry, a->btgt);
		//err = -1;
		if (unlikely(err))
			goto out_whdst;
		set_h_dptr(dentry, a->btgt, NULL);
		err = lkup_neg(dentry, a->btgt);
		//err = -1;
		if (unlikely(err))
			goto out_whtmp;
		a->hidden_dentry[DST] = au_h_dptr_i(dentry, a->btgt);
	}

	/* cpup src */
	if (a->hidden_dentry[DST]->d_inode && a->bstart[SRC] != a->btgt) {
		flags = au_flags_cpup(!CPUP_DTIME, a->parent[SRC]);
		hg_parent = a->hidden_parent[SRC]->d_parent;
		if (!(flags & CPUP_LOCKED_GHDIR)
		    && hg_parent == a->hidden_parent[DST])
			flags |= CPUP_LOCKED_GHDIR;

		hi_lock_child(a->hidden_dentry[SRC]->d_inode);
		err = sio_cpup_simple(src_dentry, a->btgt, -1, flags);
		//err = -1; // untested dir
		i_unlock(a->hidden_dentry[SRC]->d_inode);
		if (unlikely(err))
			goto out_whtmp;
	}

	/* rename by vfs_rename or cpup */
	need_diropq = a->isdir
		&& (wh_dentry[DST]
		    || dbdiropq(dentry) == a->btgt
		    || au_flag_test(a->sb, AuFlag_ALWAYS_DIROPQ));
	bycpup = 0;
	if (dbstart(src_dentry) == a->btgt) {
		if (need_diropq && dbdiropq(src_dentry) == a->btgt)
			need_diropq = 0;
		err = vfsub_rename(hidden_dir[SRC], au_h_dptr(src_dentry),
				   hidden_dir[DST], a->hidden_dentry[DST],
				   a->dlgt);
		//err = -1;
	} else {
		bycpup = 1;
		flags = au_flags_cpup(!CPUP_DTIME, a->parent[DST]);
		hg_parent = a->hidden_parent[DST]->d_parent;
		if (!(flags & CPUP_LOCKED_GHDIR)
		    && hg_parent == a->hidden_parent[SRC])
			flags |= CPUP_LOCKED_GHDIR;

		hi_lock_child(a->hidden_dentry[SRC]->d_inode);
		set_dbstart(src_dentry, a->btgt);
		set_h_dptr(src_dentry, a->btgt, dget(a->hidden_dentry[DST]));
		err = sio_cpup_single(src_dentry, a->btgt, a->bstart[SRC], -1,
				      flags);
		//err = -1; // untested dir
		if (unlikely(err)) {
			set_h_dptr(src_dentry, a->btgt, NULL);
			set_dbstart(src_dentry, a->bstart[SRC]);
		}
		i_unlock(a->hidden_dentry[SRC]->d_inode);
	}
	if (unlikely(err))
		goto out_whtmp;

	/* make dir opaque */
	if (need_diropq) {
		struct dentry *diropq;
		struct inode *h_inode;

		h_inode = au_h_dptr_i(src_dentry, a->btgt)->d_inode;
		hdir_lock(h_inode, src_dentry->d_inode, a->btgt);
		diropq = create_diropq(src_dentry, a->btgt, a->dlgt);
		//diropq = ERR_PTR(-1);
		hdir_unlock(h_inode, src_dentry->d_inode, a->btgt);
		err = PTR_ERR(diropq);
		if (IS_ERR(diropq))
			goto out_rename;
		dput(diropq);
	}

	/* remove whiteout for dentry */
	if (wh_dentry[DST]) {
		err = au_unlink_wh_dentry(hidden_dir[DST], wh_dentry[DST],
					  dentry, a->dlgt);
		//err = -1;
		if (unlikely(err))
			goto out_diropq;
	}

	/* remove whtmp */
	if (tharg) {
		if (au_is_nfs(hidden_dst->d_sb)
		    || !is_longer_wh(&a->whlist, a->btgt,
				     stosi(a->sb)->si_dirwh)) {
			err = rmdir_whtmp(hidden_dst, &a->whlist, a->btgt, dir,
					  dentry->d_inode);
			if (unlikely(err))
				Warn("failed removing whtmp dir %.*s (%d), "
				     "ignored.\n", DLNPair(hidden_dst), err);
		} else {
			kick_rmdir_whtmp(hidden_dst, &a->whlist, a->btgt, dir,
					 dentry->d_inode, tharg);
			dput(hidden_dst);
			tharg = NULL;
		}
	}
	err = 0;
	goto out_success;

#define RevertFailure(fmt, args...) do { \
		IOErrWhck("revert failure: " fmt " (%d, %d)\n", \
			##args, err, rerr); \
		err = -EIO; \
	} while(0)

 out_diropq:
	if (need_diropq) {
		struct inode *h_inode;

		h_inode = au_h_dptr_i(src_dentry, a->btgt)->d_inode;
		// i_lock simplly since inotify is not set to h_inode.
		hi_lock_parent(h_inode);
		//hdir_lock(h_inode, src_dentry->d_inode, a->btgt);
		rerr = remove_diropq(src_dentry, a->btgt, a->dlgt);
		//rerr = -1;
		//hdir_unlock(h_inode, src_dentry->d_inode, a->btgt);
		i_unlock(h_inode);
		if (rerr)
			RevertFailure("remove diropq %.*s",
				      DLNPair(src_dentry));
	}
 out_rename:
	if (!bycpup) {
		struct dentry *d;
		struct qstr *name = &src_dentry->d_name;
		d = lkup_one(name->name, a->hidden_parent[SRC], name->len,
			     &lkup);
		//d = ERR_PTR(-1);
		rerr = PTR_ERR(d);
		if (IS_ERR(d)) {
			RevertFailure("lkup_one %.*s", DLNPair(src_dentry));
			goto out_whtmp;
		}
		DEBUG_ON(d->d_inode);
		rerr = vfsub_rename
			(hidden_dir[DST], au_h_dptr_i(src_dentry, a->btgt),
			 hidden_dir[SRC], d, a->dlgt);
		//rerr = -1;
		d_drop(d);
		dput(d);
		//set_h_dptr(src_dentry, a->btgt, NULL);
		if (rerr)
			RevertFailure("rename %.*s", DLNPair(src_dentry));
	} else {
		rerr = vfsub_unlink(hidden_dir[DST], a->hidden_dentry[DST],
				    a->dlgt);
		//rerr = -1;
		set_h_dptr(src_dentry, a->btgt, NULL);
		set_dbstart(src_dentry, a->bstart[SRC]);
		if (rerr)
			RevertFailure("unlink %.*s",
				      DLNPair(a->hidden_dentry[DST]));
	}
 out_whtmp:
	if (tharg) {
		struct dentry *d;
		struct qstr *name = &dentry->d_name;
		LKTRLabel(here);
		d = lkup_one(name->name, a->hidden_parent[DST], name->len,
			     &lkup);
		//d = ERR_PTR(-1);
		rerr = PTR_ERR(d);
		if (IS_ERR(d)) {
			RevertFailure("lookup %.*s", LNPair(name));
			goto out_whdst;
		}
		if (d->d_inode) {
			d_drop(d);
			dput(d);
			goto out_whdst;
		}
		DEBUG_ON(d->d_inode);
		rerr = vfsub_rename(hidden_dir[DST], hidden_dst,
				    hidden_dir[DST], d, a->dlgt);
		//rerr = -1;
		d_drop(d);
		dput(d);
		if (rerr) {
			RevertFailure("rename %.*s", DLNPair(hidden_dst));
			goto out_whdst;
		}
		set_h_dptr(dentry, a->btgt, NULL);
		set_h_dptr(dentry, a->btgt, dget(hidden_dst));
	}
 out_whdst:
	if (wh_dentry[DST]) {
		dput(wh_dentry[DST]);
		wh_dentry[DST] = NULL;
	}
 out_whsrc:
	if (wh_dentry[SRC]) {
		LKTRLabel(here);
		rerr = au_unlink_wh_dentry(hidden_dir[SRC], wh_dentry[SRC],
					   src_dentry, a->dlgt);
		//rerr = -1;
		if (rerr)
			RevertFailure("unlink %.*s", DLNPair(wh_dentry[SRC]));
	}
#undef RevertFailure
	d_drop(src_dentry);
	bend = dbend(src_dentry);
	for (bindex = dbstart(src_dentry); bindex <= bend; bindex++) {
		struct dentry *hd;
		hd = au_h_dptr_i(src_dentry, bindex);
		if (hd)
			d_drop(hd);
	}
	d_drop(dentry);
	bend = dbend(dentry);
	for (bindex = dbstart(dentry); bindex <= bend; bindex++) {
		struct dentry *hd;
		hd = au_h_dptr_i(dentry, bindex);
		if (hd)
			d_drop(hd);
	}
	au_update_dbstart(dentry);
	if (tharg)
		d_drop(hidden_dst);
 out_success:
	if (wh_dentry[SRC])
		dput(wh_dentry[SRC]);
	if (wh_dentry[DST])
		dput(wh_dentry[DST]);
 out_tharg:
	if (tharg) {
		dput(hidden_dst);
		kfree(tharg);
	}
 out:
	TraceErr(err);
	return err;
}

/*
 * test if @dentry dir can be rename destination or not.
 * success means, it is a logically empty dir.
 */
static int may_rename_dstdir(struct dentry *dentry, aufs_bindex_t btgt,
			     struct aufs_nhash *whlist)
{
	LKTRTrace("%.*s\n", DLNPair(dentry));

	return test_empty(dentry, whlist);
}

/*
 * test if @dentry dir can be rename source or not.
 * if it can, return 0 and @children is filled.
 * success means,
 * - or, it is a logically empty dir.
 * - or, it exists on writable branch and has no children including whiteouts
 *       on the lower branch.
 */
static int may_rename_srcdir(struct dentry *dentry, aufs_bindex_t btgt)
{
	int err;
	aufs_bindex_t bstart;

	LKTRTrace("%.*s\n", DLNPair(dentry));

	bstart = dbstart(dentry);
	if (bstart != btgt) {
		struct aufs_nhash whlist;

		init_nhash(&whlist);
		err = test_empty(dentry, &whlist);
		free_nhash(&whlist);
		goto out;
	}

	if (bstart == dbtaildir(dentry))
		return 0; /* success */

	err = au_test_empty_lower(dentry);

 out:
	if (/* unlikely */(err == -ENOTEMPTY))
		err = -EXDEV;
	TraceErr(err);
	return err;
}

int aufs_rename(struct inode *src_dir, struct dentry *src_dentry,
		struct inode *dir, struct dentry *dentry)
{
	int err, do_dt_dstdir;
	aufs_bindex_t bend, bindex;
	struct inode *inode, *dirs[2];
	enum {PARENT, CHILD};
	struct dtime dt[2][2];
	struct rename_args a;

	LKTRTrace("i%lu, %.*s, i%lu, %.*s\n",
		  src_dir->i_ino, DLNPair(src_dentry),
		  dir->i_ino, DLNPair(dentry));
	IMustLock(src_dir);
	IMustLock(dir);
	if (dentry->d_inode)
		IMustLock(dentry->d_inode);

	a.sb = src_dentry->d_sb;
	inode = src_dentry->d_inode;
	a.isdir = S_ISDIR(inode->i_mode);
	if (unlikely(a.isdir && dentry->d_inode
		     && !S_ISDIR(dentry->d_inode->i_mode)))
		return -ENOTDIR;

	aufs_read_and_write_lock2(dentry, src_dentry, a.isdir);
	a.dlgt = need_dlgt(a.sb);
	a.parent[SRC] = a.parent[DST] = dentry->d_parent;
	a.issamedir = (src_dir == dir);
	if (a.issamedir)
		di_write_lock_parent(a.parent[DST]);
	else {
		a.parent[SRC] = src_dentry->d_parent;
		di_write_lock2_parent(a.parent[SRC], a.parent[DST], /*isdir*/1);
	}

	/* which branch we process */
	a.bstart[DST] = dbstart(dentry);
	a.btgt = err = wr_dir(dentry, 1, src_dentry, /*force_btgt*/-1,
			      /*do_lock_srcdir*/0);
	if (unlikely(err < 0))
		goto out;

	/* are they available to be renamed */
	err = 0;
	init_nhash(&a.whlist);
	if (a.isdir && dentry->d_inode) {
		set_dbstart(dentry, a.bstart[DST]);
		err = may_rename_dstdir(dentry, a.btgt, &a.whlist);
		set_dbstart(dentry, a.btgt);
	}
	a.hidden_dentry[DST] = au_h_dptr(dentry);
	if (unlikely(err))
		goto out;
	a.bstart[SRC] = dbstart(src_dentry);
	a.hidden_dentry[SRC] = au_h_dptr(src_dentry);
	if (a.isdir) {
		err = may_rename_srcdir(src_dentry, a.btgt);
		if (unlikely(err))
			goto out_children;
	}

	/* prepare the writable parent dir on the same branch */
	a.whsrc = err = wr_dir_need_wh(src_dentry, a.isdir, &a.btgt,
				       a.issamedir ? NULL : a.parent[DST]);
	if (unlikely(err < 0))
		goto out_children;
	a.whdst = (a.bstart[DST] == a.btgt);
	if (!a.whdst) {
		err = cpup_dirs(dentry, a.btgt,
				a.issamedir ? NULL : a.parent[SRC]);
		if (unlikely(err))
			goto out_children;
	}

	a.hidden_parent[SRC] = au_h_dptr_i(a.parent[SRC], a.btgt);
	a.hidden_parent[DST] = au_h_dptr_i(a.parent[DST], a.btgt);
	dirs[0] = src_dir;
	dirs[1] = dir;
	hdir_lock_rename(a.hidden_parent, dirs, a.btgt, a.issamedir);

	/* store timestamps to be revertible */
	dtime_store(dt[PARENT] + SRC, a.parent[SRC], a.hidden_parent[SRC]);
	if (!a.issamedir)
		dtime_store(dt[PARENT] + DST, a.parent[DST],
			    a.hidden_parent[DST]);
	do_dt_dstdir = 0;
	if (a.isdir) {
		dtime_store(dt[CHILD] + SRC, src_dentry,
			    a.hidden_dentry[SRC]);
		if (a.hidden_dentry[DST]->d_inode) {
			do_dt_dstdir = 1;
			dtime_store(dt[CHILD] + DST, dentry,
				    a.hidden_dentry[DST]);
		}
	}

	err = do_rename(src_dir, src_dentry, dir, dentry, &a);
	if (unlikely(err))
		goto out_dt;
	hdir_unlock_rename(a.hidden_parent, dirs, a.btgt, a.issamedir);

	/* update dir attributes */
	dir->i_version++;
	if (a.isdir)
		au_cpup_attr_nlink(dir);
	if (ibstart(dir) == a.btgt)
		au_cpup_attr_timesizes(dir);

	if (!a.issamedir) {
		src_dir->i_version++;
		if (a.isdir)
			au_cpup_attr_nlink(src_dir);
		if (ibstart(src_dir) == a.btgt)
			au_cpup_attr_timesizes(src_dir);
	}

	// is this updating defined in POSIX?
	if (unlikely(a.isdir)) {
		//i_lock(inode);
		au_cpup_attr_timesizes(inode);
		//i_unlock(inode);
	}

	/* dput/iput all lower dentries */
	set_dbwh(src_dentry, -1);
	bend = dbend(src_dentry);
	for (bindex = a.btgt + 1; bindex <= bend; bindex++) {
		struct dentry *hd;
		hd = au_h_dptr_i(src_dentry, bindex);
		if (hd)
			set_h_dptr(src_dentry, bindex, NULL);
	}
	set_dbend(src_dentry, a.btgt);

	bend = ibend(inode);
	for (bindex = a.btgt + 1; bindex <= bend; bindex++) {
		struct inode *hi;
		hi = au_h_iptr_i(inode, bindex);
		if (hi)
			set_h_iptr(inode, bindex, NULL, 0);
	}
	set_ibend(inode, a.btgt);
	goto out_children; /* success */

 out_dt:
	dtime_revert(dt[PARENT] + SRC,
		     a.hidden_parent[SRC]->d_parent == a.hidden_parent[DST]);
	if (!a.issamedir)
		dtime_revert(dt[PARENT] + DST,
			     a.hidden_parent[DST]->d_parent
			     == a.hidden_parent[SRC]);
	if (a.isdir && err != -EIO) {
		struct dentry *hd;

		hd = dt[CHILD][SRC].dt_h_dentry;
		hi_lock_child(hd->d_inode);
		dtime_revert(dt[CHILD] + SRC, 1);
		i_unlock(hd->d_inode);
		if (do_dt_dstdir) {
			hd = dt[CHILD][DST].dt_h_dentry;
			hi_lock_child(hd->d_inode);
			dtime_revert(dt[CHILD] + DST, 1);
			i_unlock(hd->d_inode);
		}
	}
	hdir_unlock_rename(a.hidden_parent, dirs, a.btgt, a.issamedir);
 out_children:
	free_nhash(&a.whlist);
 out:
	//if (unlikely(err /* && a.isdir */)) {
	if (unlikely(err && a.isdir)) {
		au_update_dbstart(dentry);
		d_drop(dentry);
	}
	if (a.issamedir)
		di_write_unlock(a.parent[DST]);
	else
		di_write_unlock2(a.parent[SRC], a.parent[DST]);
	aufs_read_and_write_unlock2(dentry, src_dentry);
	TraceErr(err);
	return err;
}
