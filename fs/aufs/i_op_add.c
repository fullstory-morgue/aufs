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

/* $Id: i_op_add.c,v 1.29 2007/02/19 03:28:07 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
#include "aufs.h"

/*
 * final procedure of adding a new entry, except link(2).
 * remove whiteout, instantiate, copyup the parent dir's times and size
 * and update version.
 * if it failed, re-create the removed whiteout.
 */
static int epilog(struct dentry *wh_dentry, struct dentry *dentry)
{
	int err, rerr;
	aufs_bindex_t bwh;
	struct inode *inode, *dir;
	struct dentry *wh;

	LKTRTrace("wh %p, %.*s\n", wh_dentry, DLNPair(dentry));

	bwh = -1;
	if (wh_dentry) {
		bwh = dbwh(dentry);
		err = unlink_wh_dentry(wh_dentry->d_parent->d_inode, wh_dentry,
				       dentry);
		//err = -1;
		if (unlikely(err))
			goto out;
	}

	inode = aufs_new_inode(dentry);
	//inode = ERR_PTR(-1);
	if (!IS_ERR(inode)) {
		d_instantiate(dentry, inode);
		dir = dentry->d_parent->d_inode;
		/* or always cpup dir mtime? */
		if (ibstart(dir) == dbstart(dentry))
			cpup_attr_timesizes(dir);
		dir->i_version++;
		return 0; /* success */
	}

	err = PTR_ERR(inode);
	if (!wh_dentry)
		goto out;

	/* revert */
	wh = simple_create_wh(dentry, bwh, wh_dentry->d_parent);
	//wh = ERR_PTR(-1);
	rerr = PTR_ERR(wh);
	if (!IS_ERR(wh)) {
		dput(wh);
		goto out;
	}
	IOErr("%.*s reverting whiteout failed(%d, %d)\n",
	      DLNPair(dentry), err, rerr);
	err = -EIO;

 out:
	TraceErr(err);
	return err;
}

/*
 * initial procedure of adding a new entry.
 * prepare writable branch and the parent dir, lock it,
 * lookup whiteout for the new entry.
 */
static struct dentry *lock_hdir_lkup_wh(struct dentry *dentry, struct dtime *dt,
					struct dentry *src_dentry,
					int do_lock_srcdir,
					struct aufs_h_ipriv *ipriv)
{
	struct dentry *wh_dentry, *parent, *hidden_parent;
	int err;
	aufs_bindex_t bstart, bcpup;
	struct inode *dir, *h_dir;

	LKTRTrace("%.*s, src %p\n", DLNPair(dentry), src_dentry);

	parent = dentry->d_parent;
	bstart = dbstart(dentry);
	bcpup = err = wr_dir(dentry, 1, src_dentry, -1, do_lock_srcdir);
	//err = -1;
	wh_dentry = ERR_PTR(err);
	if (unlikely(err < 0))
		goto out;

	dir = parent->d_inode;
	hidden_parent = h_dptr_i(parent, bcpup);
	h_dir = hidden_parent->d_inode;
	hdir_lock(h_dir, dir, bcpup, ipriv);
	if (dt)
		dtime_store(dt, parent, hidden_parent);
	if (/* bcpup != bstart || */ bcpup != dbwh(dentry))
		return NULL; /* success */

	wh_dentry = lkup_wh(hidden_parent, &dentry->d_name,
			    mnt_nfs(parent->d_sb, bcpup));
	//wh_dentry = ERR_PTR(-1);
	if (IS_ERR(wh_dentry))
		hdir_unlock(h_dir, dir, bcpup, ipriv);

 out:
	TraceErrPtr(wh_dentry);
	return wh_dentry;
}

/* ---------------------------------------------------------------------- */

enum {Mknod, Symlink, Creat};
struct simple_arg {
	int type;
	union {
		struct {
			int mode;
			struct nameidata *nd;
		} c;
		struct {
			const char *symname;
		} s;
		struct {
			int mode;
			dev_t dev;
		} m;
	} u;
};

static int add_simple(struct inode *dir, struct dentry *dentry,
		      struct simple_arg *arg)
{
	int err;
	struct dentry *hidden_dentry, *hidden_parent, *wh_dentry, *parent;
	struct inode *hidden_dir;
	struct dtime dt;
	struct aufs_h_ipriv ipriv;

	LKTRTrace("type %d, %.*s\n", arg->type, DLNPair(dentry));
	IMustLock(dir);

	aufs_read_lock(dentry, AUFS_D_WLOCK);
	parent = dentry->d_parent;
	di_write_lock(parent);
	wh_dentry = lock_hdir_lkup_wh(dentry, &dt, /*src_dentry*/NULL,
				      /*do_lock_srcdir*/0, &ipriv);
	//wh_dentry = ERR_PTR(-1);
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	hidden_dentry = h_dptr(dentry);
	hidden_parent = hidden_dentry->d_parent;
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

#if 1 // partial testing
	switch (arg->type) {
	case Creat:
#if 0
		if (arg->u.c.nd) {
			struct nameidata fake_nd;
			fake_nd = *arg->u.c.nd;
			fake_nd.dentry = dget(hidden_parent);
			fake_nd.mnt = sbr_mnt(dentry->d_sb, dbstart(dentry));
			mntget(fake_nd.mnt);
			err = vfs_create(hidden_dir, hidden_dentry,
					 arg->u.c.mode, &fake_nd);
			path_release(&fake_nd);
		} else
#endif
			err = vfs_create(hidden_dir, hidden_dentry,
					 arg->u.c.mode, NULL);
		break;
	case Symlink:
		err = vfs_symlink(hidden_dir, hidden_dentry,
				  arg->u.s.symname, S_IALLUGO);
		break;
	case Mknod:
		err = vfs_mknod(hidden_dir, hidden_dentry,
				arg->u.m.mode, arg->u.m.dev);
		break;
	default:
		BUG();
	}
#else
	err = -1;
#endif
	if (!err)
		err = epilog(wh_dentry, dentry);
	//err = -1;

	/* revert */
	if (err && hidden_dentry->d_inode) {
		int rerr;
		rerr = hipriv_unlink(hidden_dir, hidden_dentry, dir->i_sb);
		//rerr = -1;
		if (rerr) {
			IOErr("%.*s revert failure(%d, %d)\n",
			      DLNPair(dentry), err, rerr);
			err = -EIO;
		}
		dtime_revert(&dt, !CPUP_LOCKED_GHDIR);
		d_drop(dentry);
	}

	hdir_unlock(hidden_dir, dir, dbstart(dentry), &ipriv);
	if (wh_dentry)
		dput(wh_dentry);

 out:
	if (unlikely(err)) {
		update_dbstart(dentry);
		d_drop(dentry);
	}
	di_write_unlock(parent);
	aufs_read_unlock(dentry, AUFS_D_WLOCK);
	TraceErr(err);
	return err;
}

int aufs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	struct simple_arg arg = {
		.type = Mknod,
		.u.m = {.mode = mode, .dev = dev}
	};
	return add_simple(dir, dentry, &arg);
}

int aufs_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
{
	struct simple_arg arg = {
		.type = Symlink,
		.u.s.symname = symname
	};
	return add_simple(dir, dentry, &arg);
}

int aufs_create(struct inode *dir, struct dentry *dentry, int mode,
		struct nameidata *nd)
{
	struct simple_arg arg = {
		.type = Creat,
		.u.c = {.mode = mode, .nd = nd}
	};
	return add_simple(dir, dentry, &arg);
}

/* ---------------------------------------------------------------------- */

struct link_arg {
	aufs_bindex_t bdst, bsrc;
	int issamedir;
	struct dentry *src_parent, *parent, *hidden_dentry;
	struct inode *hidden_dir, *inode;
	struct aufs_h_ipriv ipriv;
};

static int cpup_before_link(struct dentry *src_dentry, struct inode *dir,
			    struct link_arg *a)
{
	int err;
	unsigned int flags;
	struct inode *hi, *hdir = NULL, *src_dir;

	TraceEnter();

	err = 0;
	flags = flags_cpup(CPUP_DTIME, a->parent);
	src_dir = a->src_parent->d_inode;
	if (!a->issamedir) {
		di_read_lock(a->src_parent, AUFS_I_RLOCK);
		// this temporary unlock/lock is safe
		hdir_unlock(a->hidden_dir, dir, a->bdst, &a->ipriv);
		err = test_and_cpup_dirs(src_dentry, a->bdst, a->parent);
		//err = -1;
		if (!err) {
			hdir = h_iptr_i(src_dir, a->bdst);
			hdir_lock(hdir, src_dir, a->bdst, &a->ipriv);
			flags = flags_cpup(CPUP_DTIME, a->src_parent);
		}
	}

	if (!err) {
		struct aufs_h_ipriv ipriv;
		hi = h_dptr(src_dentry)->d_inode;
		i_lock_priv(hi, &ipriv, src_dentry->d_sb);
		err = sio_cpup_simple(src_dentry, a->bdst, -1, flags);
		//err = -1;
		i_unlock_priv(hi, &ipriv);
	}

	if (!a->issamedir) {
		if (hdir)
			hdir_unlock(hdir, src_dir, a->bdst, &a->ipriv);
		hdir_lock(a->hidden_dir, dir, a->bdst, &a->ipriv);
		di_read_unlock(a->src_parent, AUFS_I_RLOCK);
	}

	TraceErr(err);
	return err;
}

static int cpup_or_link(struct dentry *src_dentry, struct link_arg *a)
{
	int err;
	struct inode *inode, *h_inode, *h_dst_inode;
	struct dentry *h_dentry;
	aufs_bindex_t bstart;
	struct super_block *sb;
	struct aufs_h_ipriv ipriv;

	TraceEnter();

	sb = src_dentry->d_sb;
	inode = src_dentry->d_inode;
	h_dentry = h_dptr(src_dentry);
	h_inode = h_dentry->d_inode;
	bstart = ibstart(inode);
	h_dst_inode = NULL;
	if (bstart <= a->bdst)
		h_dst_inode = h_iptr_i(inode, a->bdst);

	if (!h_dst_inode) {
		/* copyup src_dentry as the name of dentry. */
		set_dbstart(src_dentry, a->bdst);
		set_h_dptr(src_dentry, a->bdst, dget(a->hidden_dentry));
		i_lock_priv(h_inode, &ipriv, sb);
		err = sio_cpup_single(src_dentry, a->bdst, a->bsrc, -1,
				      flags_cpup(!CPUP_DTIME, a->parent));
		//err = -1;
		i_unlock_priv(h_inode, &ipriv);
		set_h_dptr(src_dentry, a->bdst, NULL);
		set_dbstart(src_dentry, a->bsrc);
	} else {
		/* the inode of src_dentry already exists on a.bdst branch */
		h_dentry = d_find_alias(h_dst_inode);
		if (h_dentry) {
			err = hipriv_link(h_dentry, a->hidden_dir,
					  a->hidden_dentry, sb);
			dput(h_dentry);
		} else {
			IOErr("no dentry found for i%lu on b%d\n",
			      h_dst_inode->i_ino, a->bdst);
			err = -EIO;
		}
	}

	if (!err)
		append_plink(sb, a->inode, a->hidden_dentry, a->bdst);

	TraceErr(err);
	return err;
}

int aufs_link(struct dentry *src_dentry, struct inode *dir,
	      struct dentry *dentry)
{
	int err, rerr;
	struct dentry *hidden_parent, *wh_dentry, *hidden_src_dentry;
	struct dtime dt;
	struct link_arg a;
	struct super_block *sb;

	LKTRTrace("src %.*s, i%lu, dst %.*s\n",
		  DLNPair(src_dentry), dir->i_ino, DLNPair(dentry));
	IMustLock(dir);
	IMustLock(src_dentry->d_inode);

	aufs_read_and_write_lock2(dentry, src_dentry, /*isdir*/0);
	a.src_parent = src_dentry->d_parent;
	a.parent = dentry->d_parent;
	a.issamedir = (a.src_parent == a.parent);
	di_write_lock(a.parent);
	wh_dentry = lock_hdir_lkup_wh(dentry, &dt, src_dentry, !a.issamedir,
				      &a.ipriv);
	//wh_dentry = ERR_PTR(-1);
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	a.inode = src_dentry->d_inode;
	a.hidden_dentry = h_dptr(dentry);
	hidden_parent = a.hidden_dentry->d_parent;
	a.hidden_dir = hidden_parent->d_inode;
	IMustLock(a.hidden_dir);

	err = 0;
	sb = dentry->d_sb;
	a.bsrc = dbstart(src_dentry);
	a.bdst = dbstart(dentry);
	if (unlikely(!IS_MS(sb, MS_PLINK))) {
		/*
		 * copyup src_dentry to the branch we process,
		 * and then link(2) to it.
		 * gave up 'pseudo link by cpup' approach,
		 * since nlink may be one and some applications will not work.
		 */
		if (a.bdst < a.bsrc)
			err = cpup_before_link(src_dentry, dir, &a);
		if (!err) {
			hidden_src_dentry = h_dptr(src_dentry);
			err = hipriv_link(hidden_src_dentry, a.hidden_dir,
					  a.hidden_dentry, sb);
			//err = -1;
		}
	} else {
		if (a.bdst < a.bsrc)
			err = cpup_or_link(src_dentry, &a);
		else {
			hidden_src_dentry = h_dptr(src_dentry);
			err = hipriv_link(hidden_src_dentry, a.hidden_dir,
					  a.hidden_dentry, sb);
			//err = -1;
		}
	}
	if (unlikely(err))
		goto out_unlock;
	if (wh_dentry) {
		err = unlink_wh_dentry(a.hidden_dir, wh_dentry, dentry);
		//err = -1;
		if (unlikely(err))
			goto out_revert;
	}

	dir->i_version++;
	if (ibstart(dir) == dbstart(dentry))
		cpup_attr_timesizes(dir);
	if (!d_unhashed(a.hidden_dentry)
	    /* || hidden_old_inode->i_nlink <= nlink */
	    /* || SB_NFS(hidden_src_dentry->d_sb) */) {
		dentry->d_inode = igrab(a.inode);
		d_instantiate(dentry, a.inode);
		a.inode->i_nlink++;
		a.inode->i_ctime = dir->i_ctime;
	} else
		/* nfs case (< 2.6.15) */
		d_drop(dentry);
	goto out_unlock; /* success */

 out_revert:
#if 0 // remove
	if (d_unhashed(a.hidden_dentry)) {
		/* hardlink on nfs (< 2.6.15) */
		struct dentry *d;
		const struct qstr *name = &a.hidden_dentry->d_name;
		DEBUG_ON(a.hidden_dentry->d_parent->d_inode != a.hidden_dir);
		// do not superio.
		d = lkup_one(name->name, a.hidden_dentry->d_parent, name->len,
			     mnt_nfs(sb, a.bdst)??);
		rerr = PTR_ERR(d);
		if (IS_ERR(d))
			goto out_rerr;
		dput(a.hidden_dentry);
		a.hidden_dentry = d;
		DEBUG_ON(!d->d_inode);
	}
#endif
	rerr = hipriv_unlink(a.hidden_dir, a.hidden_dentry, sb);
	//rerr = -1;
	if (!rerr)
		goto out_dt;
// out_rerr:
	IOErr("%.*s reverting failed(%d, %d)\n", DLNPair(dentry), err, rerr);
	err = -EIO;
 out_dt:
	d_drop(dentry);
	dtime_revert(&dt, !CPUP_LOCKED_GHDIR);
 out_unlock:
	hdir_unlock(a.hidden_dir, dir, a.bdst, &a.ipriv);
	if (wh_dentry)
		dput(wh_dentry);
 out:
	if (unlikely(err)) {
		update_dbstart(dentry);
		d_drop(dentry);
	}
	di_write_unlock(a.parent);
	aufs_read_and_write_unlock2(dentry, src_dentry);
	TraceErr(err);
	return err;
}

int aufs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	int err, rerr, diropq;
	struct dentry *hidden_dentry, *hidden_parent, *wh_dentry, *parent,
		*opq_dentry;
	struct inode *hidden_dir, *hidden_inode;
	struct dtime dt;
	aufs_bindex_t bindex;
	struct aufs_h_ipriv ipriv;
	struct super_block *sb;

	LKTRTrace("i%lu, %.*s, mode 0%o\n", dir->i_ino, DLNPair(dentry), mode);
	IMustLock(dir);

	aufs_read_lock(dentry, AUFS_D_WLOCK);
	parent = dentry->d_parent;
	di_write_lock(parent);
	wh_dentry = lock_hdir_lkup_wh(dentry, &dt, /*src_dentry*/NULL,
				      /*do_lock_srcdir*/0, &ipriv);
	//wh_dentry = ERR_PTR(-1);
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	sb = dentry->d_sb;
	bindex = dbstart(dentry);
	hidden_dentry = h_dptr(dentry);
	hidden_parent = hidden_dentry->d_parent;
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

	err = vfs_mkdir(hidden_dir, hidden_dentry, mode);
	//err = -1;
	if (unlikely(err))
		goto out_unlock;
	hidden_inode = hidden_dentry->d_inode;

	/* make the dir opaque */
	diropq = 0;
	if (unlikely(wh_dentry || IS_MS(sb, MS_ALWAYS_DIROPQ))) {
		struct aufs_h_ipriv ipriv;

		i_lock_priv(hidden_inode, &ipriv, sb);
		opq_dentry = create_diropq(dentry, bindex);
		//opq_dentry = ERR_PTR(-1);
		i_unlock_priv(hidden_inode, &ipriv);
		err = PTR_ERR(opq_dentry);
		if (IS_ERR(opq_dentry))
			goto out_dir;
		dput(opq_dentry);
		diropq = 1;
	}

	err = epilog(wh_dentry, dentry);
	//err = -1;
	if (!err) {
		dir->i_nlink++;
		goto out_unlock; /* success */
	}

	/* revert */
	if (unlikely(diropq)) {
		struct aufs_h_ipriv ipriv;
		LKTRLabel(revert opq);
		i_lock_priv(hidden_inode, &ipriv, sb);
		rerr = remove_diropq(dentry, bindex);
		//rerr = -1;
		i_unlock_priv(hidden_inode, &ipriv);
		if (rerr) {
			IOErr("%.*s reverting diropq failed(%d, %d)\n",
			      DLNPair(dentry), err, rerr);
			err = -EIO;
		}
	}

 out_dir:
	LKTRLabel(revert dir);
	rerr = hipriv_rmdir(hidden_dir, hidden_dentry, dir->i_sb);
	//rerr = -1;
	if (rerr) {
		IOErr("%.*s reverting dir failed(%d, %d)\n",
		      DLNPair(dentry), err, rerr);
		err = -EIO;
	}
	d_drop(dentry);
	dtime_revert(&dt, /*fake flag*/CPUP_LOCKED_GHDIR);
 out_unlock:
	hdir_unlock(hidden_dir, dir, bindex, &ipriv);
	if (wh_dentry)
		dput(wh_dentry);
 out:
	if (unlikely(err)) {
		update_dbstart(dentry);
		d_drop(dentry);
	}
	di_write_unlock(parent);
	aufs_read_unlock(dentry, AUFS_D_WLOCK);
	TraceErr(err);
	return err;
}
