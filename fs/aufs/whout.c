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

/* $Id: whout.c,v 1.4 2007/02/19 03:29:57 sfjro Exp $ */

#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/namei.h>
#include <linux/random.h>
#include <linux/security.h>
#include "aufs.h"

#define WH_MASK			S_IRUGO

/* If a directory contains this file, then it is opaque.  We start with the
 * .wh. flag so that it is blocked by lookup.
 */
static struct qstr diropq_name = {
	.name = AUFS_WH_DIROPQ,
	.len = sizeof(AUFS_WH_DIROPQ) - 1
};

/*
 * generate whiteout name, which is NOT terminated by NULL.
 * @name: original d_name.name
 * @len: original d_name.len
 * @wh: whiteout qstr
 * returns zero when succeeds, otherwise error.
 * succeeded value as wh->name should be freed by free_whname().
 */
int alloc_whname(const char *name, int len, struct qstr *wh)
{
	char *p;

	DEBUG_ON(!name || !len || !wh);

	if (unlikely(len > PATH_MAX - AUFS_WH_LEN))
		return -ENAMETOOLONG;

	wh->len = len + AUFS_WH_LEN;
	wh->name = p = kmalloc(wh->len, GFP_KERNEL);
	//if (LktrCond) {kfree(p); wh->name = p = NULL;}
	if (p) {
		memcpy(p, AUFS_WH_PFX, AUFS_WH_LEN);
		memcpy(p + AUFS_WH_LEN, name, len);
		//smp_mb();
		return 0;
	}
	return -ENOMEM;
}

void free_whname(struct qstr *wh)
{
	DEBUG_ON(!wh || !wh->name);
	kfree(wh->name);
#ifdef CONFIG_AUFS_DEBUG
	wh->name = NULL;
#endif
}

/* ---------------------------------------------------------------------- */

/*
 * test if the @wh_name exists under @hidden_parent.
 * @try_sio specifies the neccesary of super-io.
 */
int is_wh(struct dentry *hidden_parent, struct qstr *wh_name, int try_sio,
	  struct vfsmount *h_mnt)
{
	int err;
	struct dentry *wh_dentry;
	struct inode *hidden_dir;

	LKTRTrace("%.*s/%.*s, mnt %p\n", DLNPair(hidden_parent),
		  wh_name->len, wh_name->name, h_mnt);
	hidden_dir = hidden_parent->d_inode;
	DEBUG_ON(!S_ISDIR(hidden_dir->i_mode));
	IMustLock(hidden_dir);

	if (!try_sio)
		wh_dentry = lkup_one(wh_name->name, hidden_parent,
				     wh_name->len, h_mnt);
	else
		wh_dentry = sio_lkup_one(wh_name->name, hidden_parent,
					 wh_name->len, h_mnt);
	//if (LktrCond) {dput(wh_dentry); wh_dentry = ERR_PTR(-1);}
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	err = 0;
	if (!wh_dentry->d_inode)
		goto out_wh; /* success */

	err = 1;
	if (S_ISREG(wh_dentry->d_inode->i_mode))
		goto out_wh; /* success */

	err = -EIO;
	IOErr("%.*s Invalid whiteout entry type 0%o.\n",
	      DLNPair(wh_dentry), wh_dentry->d_inode->i_mode);

 out_wh:
	dput(wh_dentry);
 out:
	TraceErr(err);
	return err;
}

/*
 * test if the @hidden_dentry sets opaque or not.
 */
int is_diropq(struct dentry *hidden_dentry, struct vfsmount *h_mnt)
{
	int err;
	struct inode *hidden_dir;

	LKTRTrace("dentry %.*s\n", DLNPair(hidden_dentry));
	hidden_dir = hidden_dentry->d_inode;
	DEBUG_ON(!S_ISDIR(hidden_dir->i_mode));
	IMustLock(hidden_dir);

	err = is_wh(hidden_dentry, &diropq_name, /*try_sio*/1, h_mnt);
	TraceErr(err);
	return err;
}

/*
 * returns a negative dentry whose name is unique and temporary.
 */
struct dentry *lkup_whtmp(struct dentry *hidden_parent, struct qstr *prefix,
			  struct vfsmount *h_mnt)
{
#define HEX_LEN 4
	struct dentry *dentry;
	int len, i;
	char defname[AUFS_WH_LEN * 2 + DNAME_INLINE_LEN_MIN + 1 + HEX_LEN + 1],
		*name, *p;
	static unsigned char cnt;

	LKTRTrace("hp %.*s, prefix %.*s\n",
		  DLNPair(hidden_parent), prefix->len, prefix->name);
	DEBUG_ON(!hidden_parent->d_inode);
	IMustLock(hidden_parent->d_inode);

	name = defname;
	len = sizeof(defname) - DNAME_INLINE_LEN_MIN + prefix->len - 1;
	if (unlikely(prefix->len > DNAME_INLINE_LEN_MIN)) {
		dentry = ERR_PTR(-ENAMETOOLONG);
		if (unlikely(len >= PATH_MAX))
			goto out;
		dentry = ERR_PTR(-ENOMEM);
		name = kmalloc(len + 1, GFP_KERNEL);
		//if (LktrCond) {kfree(name); name = NULL;}
		if (unlikely(!name))
			goto out;
	}

	// doubly whiteout-ed
	memcpy(name, AUFS_WH_PFX AUFS_WH_PFX, AUFS_WH_LEN * 2);
	p = name + AUFS_WH_LEN * 2;
	memcpy(p, prefix->name, prefix->len);
	p += prefix->len;
	*p++ = '.';
	DEBUG_ON(name + len + 1 - p <= HEX_LEN);

	for (i = 0; i < 3; i++) {
		sprintf(p, "%.*d", HEX_LEN, cnt++);
		dentry = sio_lkup_one(name, hidden_parent, len, h_mnt);
		//if (LktrCond) {dput(dentry); dentry = ERR_PTR(-1);}
		if (unlikely(IS_ERR(dentry) || !dentry->d_inode))
			goto out_name;
		dput(dentry);
	}
	//Warn("could not get random name\n");
	dentry = ERR_PTR(-EEXIST);
	Dbg("%.*s\n", len, name);
	BUG();

 out_name:
	if (unlikely(name != defname))
		kfree(name);
 out:
	TraceErrPtr(dentry);
	return dentry;
#undef HEX_LEN
}

/*
 * rename the @dentry of @bindex to the whiteouted temporary name.
 */
int rename_whtmp(struct dentry *dentry, aufs_bindex_t bindex)
{
	int err;
	struct inode *hidden_dir;
	struct dentry *hidden_dentry, *hidden_parent, *tmp_dentry;
	struct super_block *sb;

	LKTRTrace("%.*s, b%d\n", DLNPair(dentry), bindex);
	hidden_dentry = h_dptr_i(dentry, bindex);
	DEBUG_ON(!hidden_dentry || !hidden_dentry->d_inode);
	hidden_parent = hidden_dentry->d_parent;
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

	sb = dentry->d_sb;
	tmp_dentry = lkup_whtmp(hidden_parent, &hidden_dentry->d_name,
				mnt_nfs(sb, bindex));
	//if (LktrCond) {dput(tmp_dentry); tmp_dentry = ERR_PTR(-1);}
	err = PTR_ERR(tmp_dentry);
	if (!IS_ERR(tmp_dentry)) {
		/* under the same dir, no need to lock_rename() */
		err = hipriv_rename(hidden_dir, hidden_dentry, hidden_dir,
				    tmp_dentry, sb);
		//if (LktrCond) err = -1; //unavailable
		TraceErr(err);
		dput(tmp_dentry);
	}

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

int unlink_wh_dentry(struct inode *hidden_dir, struct dentry *wh_dentry,
		     struct dentry *dentry)
{
	int err;

	LKTRTrace("hi%lu, wh %.*s, d %p\n", hidden_dir->i_ino,
		  DLNPair(wh_dentry), dentry);
	DEBUG_ON((dentry && dbwh(dentry) == -1)
		 || !wh_dentry->d_inode
		 || !S_ISREG(wh_dentry->d_inode->i_mode));
	IMustLock(hidden_dir);

	err = safe_unlink(hidden_dir, wh_dentry);
	//if (LktrCond) err = -1; // unavailable
	if (!err && dentry)
		set_dbwh(dentry, -1);

	TraceErr(err);
	return err;
}

static int unlink_wh_name(struct dentry *hidden_parent, struct qstr *wh,
			  struct vfsmount *h_mnt)
{
	int err;
	struct inode *hidden_dir;
	struct dentry *hidden_dentry;

	LKTRTrace("%.*s/%.*s\n", DLNPair(hidden_parent), LNPair(wh));
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

	// test_perm() is already done
	hidden_dentry = lkup_one(wh->name, hidden_parent, wh->len, h_mnt);
	//if (LktrCond) {dput(hidden_dentry); hidden_dentry = ERR_PTR(-1);}
	if (!IS_ERR(hidden_dentry)) {
		err = 0;
		if (hidden_dentry->d_inode)
			err = safe_unlink(hidden_dir, hidden_dentry);
		dput(hidden_dentry);
	} else
		err = PTR_ERR(hidden_dentry);

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * initialize the whiteout base file for @br.
 */
int init_wh(struct dentry *hidden_root, struct aufs_branch *br,
	    struct vfsmount *h_mnt)
{
	int err;
	struct dentry *wh, *plink;
	struct inode *hidden_dir;
	static struct qstr base_name[] = {
		{.name = AUFS_WH_BASENAME, .len = sizeof(AUFS_WH_BASENAME) - 1},
		{.name = AUFS_WH_PLINKDIR, .len = sizeof(AUFS_WH_PLINKDIR) - 1}
	};

	LKTRTrace("h_mnt %p\n", h_mnt);

	hidden_dir = hidden_root->d_inode;
	IMustLock(hidden_dir);
	DEBUG_ON(!hidden_dir->i_op || !hidden_dir->i_op->link);
	BrWhMustWriteLock(br);

	// doubly whiteouted
	wh = lkup_wh(hidden_root, base_name + 0, h_mnt);
	//if (LktrCond) {dput(wh); wh = ERR_PTR(-1);}
	err = PTR_ERR(wh);
	if (IS_ERR(wh))
		goto out;
	plink = br->br_plink;
	if (!br->br_plink) {
		plink = lkup_wh(hidden_root, base_name + 1, h_mnt);
		err = PTR_ERR(plink);
		if (IS_ERR(plink))
			goto out_wh;
	}

	err = -EEXIST;
	if (wh->d_inode) {
		if (S_ISREG(wh->d_inode->i_mode))
			err = 0;
	} else
		err = vfs_create(hidden_dir, wh, WH_MASK, NULL);
		//if (LktrCond) {vfs_unlink(hidden_dir, wh); err = -1;}
	if (unlikely(err))
		goto out_plink;
	i_priv(wh->d_inode);

	/* do not set PRIVATE to this dir */
	err = -EEXIST;
	if (plink->d_inode) {
		if (S_ISDIR(plink->d_inode->i_mode))
			err = 0;
	} else {
		int mode = S_IRWXU;
		if (unlikely(SB_NFS(plink->d_sb)))
			mode |= S_IXUGO;
		err = vfs_mkdir(hidden_dir, plink, mode);
	}
	if (unlikely(err))
		goto out_plink;

	br->br_wh = wh;
	br->br_plink = plink;
	return 0; /* success */

 out_plink:
	dput(plink);
 out_wh:
	dput(wh);
 out:
	TraceErr(err);
	return err;
}

struct reinit_br_wh {
	struct super_block *sb;
	struct aufs_branch *br;
};

static int reinit_br_wh(void *arg)
{
	int err;
	struct reinit_br_wh *a = arg;
	struct inode *hidden_dir, *dir;
	struct dentry *hidden_root;
	aufs_bindex_t bindex;
	struct aufs_h_ipriv ipriv;

	TraceEnter();
	DEBUG_ON(!a->br->br_wh || !a->br->br_wh->d_inode || current->fsuid);

	err = 0;
	si_read_lock(a->sb);
	if (unlikely(!(a->br->br_perm & MAY_WRITE)))
		goto out;
	bindex = find_brindex(a->sb, a->br->br_id);
	if (unlikely(bindex < 0))
		goto out;

	dir = a->sb->s_root->d_inode;
	hidden_root = a->br->br_wh->d_parent;
	hidden_dir = hidden_root->d_inode;
	DEBUG_ON(!hidden_dir->i_op || !hidden_dir->i_op->link);
	hdir_lock(hidden_dir, dir, bindex, &ipriv);
	br_wh_write_lock(a->br);
	err = safe_unlink(hidden_dir, a->br->br_wh);
	//if (LktrCond) err = -1;
	dput(a->br->br_wh);
	a->br->br_wh = NULL;
	if (!err)
		err = init_wh(hidden_root, a->br, a->br->br_mnt);
	br_wh_write_unlock(a->br);
	hdir_unlock(hidden_dir, dir, bindex, &ipriv);

 out:
	atomic_dec(&a->br->br_wh_running);
	br_put(a->br);
	si_read_unlock(a->sb);
	kfree(arg);
	if (unlikely(err))
		IOErr("err %d\n", err);
	do_exit(err);
	return err;
}

static void kick_reinit_br_wh(struct super_block *sb, struct aufs_branch *br)
{
	int do_dec;
	struct reinit_br_wh *arg;
	static DECLARE_WAIT_QUEUE_HEAD(wq);
	struct task_struct *tsk;

	do_dec = 1;
	if (atomic_inc_return(&br->br_wh_running) != 1)
		goto out;

	// ignore ENOMEM
	arg = kmalloc(sizeof(*arg), GFP_KERNEL);
	if (arg) {
		// dec(wh_running), kfree(arg) and br_put() in reinit function
		arg->sb = sb;
		arg->br = br;
		br_get(br);
		smp_mb();
		wait_event(wq, !IS_ERR(tsk = kthread_run
				       (reinit_br_wh, arg,
					AUFS_NAME "_reinit_br_wh")));
		do_dec = 0;
	}

 out:
	if (do_dec)
		atomic_dec(&br->br_wh_running);
}

/*
 * create the whiteoute @wh.
 */
static int link_or_create_wh(struct dentry *wh, struct super_block *sb,
			     aufs_bindex_t bindex)
{
	int err;
	struct aufs_branch *br;
	struct dentry *hidden_parent;
	struct inode *hidden_dir;

	LKTRTrace("%.*s\n", DLNPair(wh));
	SiMustReadLock(sb);
	hidden_parent = wh->d_parent;
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

	br = stobr(sb, bindex);
	br_wh_read_lock(br);
	if (br->br_wh) {
		IMustPriv(br->br_wh->d_inode);
		err = vfs_link(br->br_wh, hidden_dir, wh);
		if (!err || err != -EMLINK)
			goto out;

		// link count full. re-initialize br_wh.
		kick_reinit_br_wh(sb, br);
	}

	// return this error in this context
	err = vfs_create(hidden_dir, wh, WH_MASK, NULL);
	if (!err)
		i_priv(wh->d_inode);

 out:
	br_wh_read_unlock(br);
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * create or remove the diropq.
 */
static struct dentry *do_diropq(struct dentry *dentry, aufs_bindex_t bindex,
				int do_create)
{
	struct dentry *opq_dentry, *hidden_dentry;
	struct inode *hidden_dir;
	int err;
	struct super_block *sb;

	LKTRTrace("%.*s, bindex %d, do_create %d\n", DLNPair(dentry),
		  bindex, do_create);
	hidden_dentry = h_dptr_i(dentry, bindex);
	DEBUG_ON(!hidden_dentry);
	hidden_dir = hidden_dentry->d_inode;
	DEBUG_ON(!hidden_dir || !S_ISDIR(hidden_dir->i_mode));
	IMustLock(hidden_dir);

	// already checked by test_perm().
	sb = dentry->d_sb;
	opq_dentry = lkup_one(diropq_name.name, hidden_dentry, diropq_name.len,
			      mnt_nfs(sb, bindex));
	//if (LktrCond) {dput(opq_dentry); opq_dentry = ERR_PTR(-1);}
	if (IS_ERR(opq_dentry))
		goto out;

	if (do_create) {
		DEBUG_ON(opq_dentry->d_inode);
		err = link_or_create_wh(opq_dentry, sb, bindex);
		//if (LktrCond) {vfs_unlink(hidden_dir, opq_dentry); err = -1;}
		if (!err) {
			set_dbdiropq(dentry, bindex);
			goto out; /* success */
		}
	} else {
		DEBUG_ON(/* !S_ISDIR(dentry->d_inode->i_mode)
			  * ||  */!opq_dentry->d_inode);
		err = safe_unlink(hidden_dir, opq_dentry);
		//if (LktrCond) err = -1;
		if (!err)
			set_dbdiropq(dentry, -1);
	}
	dput(opq_dentry);
	opq_dentry = ERR_PTR(err);

 out:
	TraceErrPtr(opq_dentry);
	return opq_dentry;
}

struct do_diropq_args {
	struct dentry **errp;
	struct dentry *dentry;
	aufs_bindex_t bindex;
	int do_create;
};

static void call_do_diropq(void *args)
{
	struct do_diropq_args *a = args;
	*a->errp = do_diropq(a->dentry, a->bindex, a->do_create);
}

struct dentry *sio_diropq(struct dentry *dentry, aufs_bindex_t bindex,
			  int do_create)
{
	struct dentry *diropq, *hidden_dentry;

	LKTRTrace("%.*s, bindex %d, do_create %d\n",
		  DLNPair(dentry), bindex, do_create);

	hidden_dentry = h_dptr_i(dentry, bindex);
	if (!test_perm(hidden_dentry->d_inode, MAY_EXEC | MAY_WRITE))
		diropq = do_diropq(dentry, bindex, do_create);
	else {
		struct do_diropq_args args = {
			.errp		= &diropq,
			.dentry		= dentry,
			.bindex		= bindex,
			.do_create	= do_create
		};
		wkq_wait(call_do_diropq, &args);
	}

	TraceErrPtr(diropq);
	return diropq;
}

/* ---------------------------------------------------------------------- */

/*
 * lookup whiteout dentry.
 * @hidden_parent: hidden parent dentry which must exist and be locked
 * @base_name: name of dentry which will be whiteouted
 * returns dentry for whiteout.
 */
struct dentry *lkup_wh(struct dentry *hidden_parent, struct qstr *base_name,
		       struct vfsmount *h_mnt)
{
	int err;
	struct qstr wh_name;
	struct dentry *wh_dentry;

	LKTRTrace("%.*s/%.*s\n", DLNPair(hidden_parent), LNPair(base_name));
	IMustLock(hidden_parent->d_inode);

	err = alloc_whname(base_name->name, base_name->len, &wh_name);
	//if (LktrCond) {free_whname(&wh_name); err = -1;}
	wh_dentry = ERR_PTR(err);
	if (!err) {
		// do not superio.
		wh_dentry = lkup_one(wh_name.name, hidden_parent, wh_name.len,
				     h_mnt);
		free_whname(&wh_name);
	}
	TraceErrPtr(wh_dentry);
	return wh_dentry;
}

/*
 * link/create a whiteout for @dentry on @bindex.
 */
struct dentry *simple_create_wh(struct dentry *dentry, aufs_bindex_t bindex,
				struct dentry *hidden_parent)
{
	struct dentry *wh_dentry;
	int err;
	struct super_block *sb;

	LKTRTrace("%.*s/%.*s on b%d\n", DLNPair(hidden_parent),
		  DLNPair(dentry), bindex);

	sb = dentry->d_sb;
	wh_dentry = lkup_wh(hidden_parent, &dentry->d_name,
			    mnt_nfs(sb, bindex));
	//if (LktrCond) {dput(wh_dentry); wh_dentry = ERR_PTR(-1);}
	if (!IS_ERR(wh_dentry) && !wh_dentry->d_inode) {
		IMustLock(hidden_parent->d_inode);
		err = link_or_create_wh(wh_dentry, sb, bindex);
		if (!err)
			set_dbwh(dentry, bindex);
		else {
			dput(wh_dentry);
			wh_dentry = ERR_PTR(err);
		}
	}

	TraceErrPtr(wh_dentry);
	return wh_dentry;
}

/* ---------------------------------------------------------------------- */

/* Delete all whiteouts in this directory in branch bindex. */
static int del_wh_children(struct aufs_nhash *whlist,
			   struct dentry *hidden_parent, aufs_bindex_t bindex,
			   struct vfsmount *h_mnt)
{
	int err, i;
	struct qstr wh_name;
	char *p;
	struct inode *hidden_dir;
	struct hlist_head *head;
	struct aufs_wh *tpos;
	struct hlist_node *pos;
	struct aufs_destr *str;

	LKTRTrace("%.*s\n", DLNPair(hidden_parent));
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);
	DEBUG_ON(IS_RDONLY(hidden_dir));
	//SiMustReadLock(??);

	err = -ENOMEM;
	wh_name.name = p = __getname();
	//if (LktrCond) {__putname(p); wh_name.name = p = NULL;}
	if (unlikely(!wh_name.name))
		goto out;
	memcpy(p, AUFS_WH_PFX, AUFS_WH_LEN);
	p += AUFS_WH_LEN;

	// already checked by test_perm().
	err = 0;
	for (i = 0; !err && i < AUFS_NHASH_SIZE; i++) {
		head = whlist->heads + i;
		hlist_for_each_entry(tpos, pos, head, wh_hash) {
			if (tpos->wh_bindex != bindex)
				continue;
			str = &tpos->wh_str;
			if (str->len + AUFS_WH_LEN <= PATH_MAX) {
				memcpy(p, str->name, str->len);
				wh_name.len = AUFS_WH_LEN + str->len;
				err = unlink_wh_name(hidden_parent, &wh_name,
						     h_mnt);
				//if (LktrCond) err = -1;
				if (!err)
					continue;
				break;
			}
			IOErr("whiteout name too long %.*s\n",
			      str->len, str->name);
			err = -EIO;
			break;
		}
	}
	__putname(wh_name.name);

 out:
	TraceErr(err);
	return err;
}

struct del_wh_children_args {
	int *errp;
	struct aufs_nhash *whlist;
	struct dentry *hidden_parent;
	aufs_bindex_t bindex;
	struct vfsmount *h_mnt;
};

static void call_del_wh_children(void *args)
{
	struct del_wh_children_args *a = args;
	*a->errp = del_wh_children(a->whlist, a->hidden_parent, a->bindex,
				   a->h_mnt);
}

/* ---------------------------------------------------------------------- */

/*
 * rmdir the whiteouted temporary named dir @hidden_dentry.
 * @whlist: whiteouted children.
 */
int rmdir_whtmp(struct dentry *hidden_dentry, struct aufs_nhash *whlist,
		aufs_bindex_t bindex, struct inode *dir, struct inode *inode)
{
	int err;
	struct inode *hidden_inode, *hidden_dir;
	struct vfsmount *h_mnt;
	struct aufs_h_ipriv ipriv;

	LKTRTrace("hd %.*s, b%d, i%lu\n",
		  DLNPair(hidden_dentry), bindex, dir->i_ino);
	IMustLock(dir);
	IiMustAnyLock(dir);
	hidden_dir = hidden_dentry->d_parent->d_inode;
	IMustLock(hidden_dir);

	h_mnt = mnt_nfs(inode->i_sb, bindex);
	hidden_inode = hidden_dentry->d_inode;
	DEBUG_ON(hidden_inode != h_iptr_i(inode, bindex));
	hdir_lock(hidden_inode, inode, bindex, &ipriv);
	if (!test_perm(hidden_inode, MAY_EXEC | MAY_WRITE))
		err = del_wh_children(whlist, hidden_dentry, bindex, h_mnt);
	else {
		struct del_wh_children_args args = {
			.errp		= &err,
			.whlist		= whlist,
			.hidden_parent	= hidden_dentry,
			.bindex		= bindex,
			.h_mnt		= h_mnt
		};
		wkq_wait(call_del_wh_children, &args);
	}
	/* keep this inode PRIVATE */
	hdir_unlock(hidden_inode, inode, bindex, NULL);

	if (!err) {
		err = hipriv_rmdir(hidden_dir, hidden_dentry, dir->i_sb);
		//d_drop(hidden_dentry);
		//if (LktrCond) err = -1;
	}
	/* revert PRIVATE */
	h_iunpriv(&ipriv, /*do_lock*/1);
	//smp_mb();

	if (!err) {
		if (ibstart(dir) == bindex) {
			cpup_attr_timesizes(dir);
			//cpup_attr_nlink(dir);
			dir->i_nlink--;
		}
		return 0; /* success */
	}

	Warn("failed removing %.*s(%d), ignored\n",
	     DLNPair(hidden_dentry), err);
	return err;
}

static int do_rmdir_whtmp(void *arg)
{
	int err;
	struct rmdir_whtmp_arg *a = arg;
	struct super_block *sb;

	LKTRTrace("%.*s, b%d, dir i%lu\n",
		  DLNPair(a->h_dentry), a->bindex, a->dir->i_ino);

	i_lock(a->dir);
	sb = a->dir->i_sb;
	si_read_lock(sb);
	err = test_ro(sb, a->bindex, NULL);
	if (!err) {
		struct inode *hidden_dir = a->h_dentry->d_parent->d_inode;
		struct aufs_h_ipriv ipriv;

		ii_write_lock(a->inode);
		ii_write_lock(a->dir);
		hdir_lock(hidden_dir, a->dir, a->bindex, &ipriv);
		err = rmdir_whtmp(a->h_dentry, &a->whlist, a->bindex,
				  a->dir, a->inode);
		hdir_unlock(hidden_dir, a->dir, a->bindex, &ipriv);
		ii_write_unlock(a->dir);
		ii_write_unlock(a->inode);
	}
	dput(a->h_dentry);
	free_nhash(&a->whlist);
	iput(a->inode);
	si_read_unlock(sb);
	i_unlock(a->dir);
	iput(a->dir);
	kfree(arg);
	if (unlikely(err))
		IOErr("err %d\n", err);
	do_exit(err);
	return err;
}

void kick_rmdir_whtmp(struct dentry *hidden_dentry, struct aufs_nhash *whlist,
		      aufs_bindex_t bindex, struct inode *dir,
		      struct inode *inode, struct rmdir_whtmp_arg *arg)
{
	static DECLARE_WAIT_QUEUE_HEAD(wq);
	struct task_struct *tsk;

	LKTRTrace("%.*s\n", DLNPair(hidden_dentry));
	IMustLock(dir);

	// all post-process will be done in do_rmdir_whtmp().
	arg->h_dentry = dget(hidden_dentry);
	init_nhash(&arg->whlist);
	move_nhash(&arg->whlist, whlist);
	arg->bindex = bindex;
	arg->dir = igrab(dir);
	arg->inode = igrab(inode);
	smp_mb();
	wait_event(wq, !IS_ERR(tsk = kthread_run
			       (do_rmdir_whtmp, arg,
				AUFS_NAME "_rmdir_whtmp")));

	// hoping do_rmdir_whtmp is the first waiter of 'dir' lock
	yield();
}
