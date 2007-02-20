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

/* $Id: dentry.c,v 1.31 2007/02/19 03:26:18 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
#include "aufs.h"

#ifdef CONFIG_AUFS_LHASH_PATCH
/* it doesn't mntget() */
struct vfsmount *mnt_nfs(struct super_block *sb, aufs_bindex_t bindex)
{
	struct vfsmount *h_mnt;

	h_mnt = sbr_mnt(sb, bindex);
	if (!SB_NFS(h_mnt->mnt_sb))
		return NULL;
	return h_mnt;
}

/* cf. lookup_one_len() in linux/fs/namei.c */
struct dentry *lkup_one(const char *name, struct dentry *parent, int len,
			struct vfsmount *mnt)
{
	struct dentry *dentry;
	char *p;
	unsigned long hash;
	struct qstr this;
	unsigned int c;
	struct nameidata tmp_nd;

	LKTRTrace("%.*s/%.*s, mnt %p\n", DLNPair(parent), len, name, mnt);

	if (!mnt)
		return lookup_one_len(name, parent, len);

	dentry = ERR_PTR(-EACCES);
	this.name = name;
	this.len = len;
	if (unlikely(!len))
		goto out;

	p = (void*)name;
	hash = init_name_hash();
	while (len--) {
		c = *p++;
		if (unlikely(c == '/' || c == '\0'))
			goto out;
		hash = partial_name_hash(c, hash);
	}
	this.hash = end_name_hash(hash);

	memset(&tmp_nd, 0, sizeof(tmp_nd));
	tmp_nd.dentry = dget(parent);
	tmp_nd.mnt = mntget(mnt);
	dentry = __lookup_hash(&this, parent, &tmp_nd);
	path_release(&tmp_nd);

 out:
	TraceErrPtr(dentry);
	return dentry;
}
#endif

struct lkup_one_args {
	struct dentry **errp;
	const char *name;
	struct dentry *parent;
	int len;
	struct vfsmount *mnt;
};

static void call_lkup_one(void *args)
{
	struct lkup_one_args *a = args;
	*a->errp = lkup_one(a->name, a->parent, a->len, a->mnt);
}

/*
 * returns positive/negative dentry, NULL or an error.
 * NULL means whiteout-ed or not-found.
 */
static struct dentry *do_lookup(struct dentry *hidden_parent,
				struct dentry *dentry, aufs_bindex_t bindex,
				struct qstr *wh_name, int allow_neg,
				mode_t type)
{
	struct dentry *hidden_dentry;
	int wh_found, wh_able, opq;
	struct inode *hidden_dir, *hidden_inode;
	struct qstr *name;
	struct vfsmount *h_mnt;
	struct aufs_h_ipriv ipriv;
	struct super_block *sb;

	LKTRTrace("%.*s/%.*s, b%d, allow_neg %d, type 0%o\n",
		  DLNPair(hidden_parent), DLNPair(dentry), bindex, allow_neg,
		  type);
	DEBUG_ON(IS_ROOT(dentry));
	hidden_dir = hidden_parent->d_inode;
	IMustLock(hidden_dir);

	wh_found = 0;
	sb = dentry->d_sb;
	wh_able = (sbr_perm(sb, bindex) & (MAY_WRITE | AUFS_MAY_WH));
	h_mnt = mnt_nfs(sb, bindex);
	name = &dentry->d_name;
	if (unlikely(wh_able)) {
#if 0 //def CONFIG_AUFS_NEST
		if (strncmp(name->name, AUFS_WH_PFX, AUFS_WH_LEN))
			wh_found = is_wh(hidden_parent, wh_name, /*try_sio*/0,
					 h_mnt);
		else
			wh_found = -EPERM;
#else
		wh_found = is_wh(hidden_parent, wh_name, /*try_sio*/0, h_mnt);
#endif
	}
	//if (LktrCond) wh_found = -1;
	hidden_dentry = ERR_PTR(wh_found);
	if (!wh_found)
		goto real_lookup;
	if (unlikely(wh_found < 0))
		goto out;

	/* We found a whiteout */
	//set_dbend(dentry, bindex);
	set_dbwh(dentry, bindex);
	if (!allow_neg)
		return NULL; /* success */

 real_lookup:
	// do not superio.
	hidden_dentry = lkup_one(name->name, hidden_parent, name->len, h_mnt);
	//if (LktrCond) {dput(hidden_dentry); hidden_dentry = ERR_PTR(-1);}
	if (IS_ERR(hidden_dentry))
		goto out;
	DEBUG_ON(d_unhashed(hidden_dentry));
	hidden_inode = hidden_dentry->d_inode;
	if (!hidden_inode) {
		if (!allow_neg)
			goto out_neg;
	} else if (wh_found
		   || (type && type != (hidden_inode->i_mode & S_IFMT)))
		goto out_neg;

	if (dbend(dentry) <= bindex)
		set_dbend(dentry, bindex);
	if (dbstart(dentry) == -1 || bindex < dbstart(dentry))
		set_dbstart(dentry, bindex);
	set_h_dptr(dentry, bindex, hidden_dentry);

	if (!hidden_inode || !S_ISDIR(hidden_inode->i_mode) || !wh_able)
		return hidden_dentry; /* success */

	i_lock_priv(hidden_inode, &ipriv, sb);
	opq = is_diropq(hidden_dentry, h_mnt);
	//if (LktrCond) opq = -1;
	i_unlock_priv(hidden_inode, &ipriv);
	if (opq > 0)
		set_dbdiropq(dentry, bindex);
	else if (unlikely(opq < 0)) {
		set_h_dptr(dentry, bindex, NULL);
		hidden_dentry = ERR_PTR(opq);
	}
	goto out;

 out_neg:
	dput(hidden_dentry);
	hidden_dentry = NULL;
 out:
	TraceErrPtr(hidden_dentry);
	return hidden_dentry;
}

/*
 * returns the number of hidden positive dentries,
 * otherwise an error.
 */
int lkup_dentry(struct dentry *dentry, aufs_bindex_t bstart, mode_t type)
{
	int npositive, err, allow_neg;
	struct dentry *parent;
	aufs_bindex_t bindex, btail;
	const struct qstr *name = &dentry->d_name;
	struct qstr whname;
	struct super_block *sb;

	LKTRTrace("%.*s, b%d, type 0%o\n", LNPair(name), bstart, type);
	DEBUG_ON(bstart < 0 || IS_ROOT(dentry));
	parent = dentry->d_parent;

#if 1 //ndef CONFIG_AUFS_NEST
	err = -EPERM;
	if (unlikely(!strncmp(name->name, AUFS_WH_PFX, AUFS_WH_LEN)))
		goto out;
#endif

	err = alloc_whname(name->name, name->len, &whname);
	//if (LktrCond) {free_whname(&whname); err = -1;}
	if (unlikely(err))
		goto out;

	sb = dentry->d_sb;
	allow_neg = !type;
	npositive = 0;
	btail = dbtaildir(parent);
	for (bindex = bstart; bindex <= btail; bindex++) {
		struct dentry *hidden_parent, *hidden_dentry;
		struct inode *hidden_inode;
		struct inode *hidden_dir;
		struct aufs_h_ipriv ipriv;

		hidden_dentry = h_dptr_i(dentry, bindex);
		if (hidden_dentry) {
			if (hidden_dentry->d_inode)
				npositive++;
			if (type != S_IFDIR)
				break;
			continue;
		}
		hidden_parent = h_dptr_i(parent, bindex);
		if (!hidden_parent)
			continue;
		hidden_dir = hidden_parent->d_inode;
		if (!hidden_dir || !S_ISDIR(hidden_dir->i_mode))
			continue;

		i_lock_priv(hidden_dir, &ipriv, sb);
		hidden_dentry = do_lookup(hidden_parent, dentry, bindex,
					  &whname, allow_neg, type);
		// do not dput for testing
		//if (LktrCond) {hidden_dentry = ERR_PTR(-1);}
		i_unlock_priv(hidden_dir, &ipriv);
		err = PTR_ERR(hidden_dentry);
		if (IS_ERR(hidden_dentry))
			goto out_wh;
		allow_neg = 0;

		if (dbwh(dentry) != -1)
			break;
		if (!hidden_dentry)
			continue;
		hidden_inode = hidden_dentry->d_inode;
		if (!hidden_inode)
			continue;
		npositive++;
		if (!type)
			type = hidden_inode->i_mode & S_IFMT;
		if (type != S_IFDIR)
			break;
		else if (dbdiropq(dentry) != -1)
			break;
	}

	if (npositive) {
		LKTRLabel(positive);
		update_dbstart(dentry);
	}
	err = npositive;

 out_wh:
	free_whname(&whname);
 out:
	TraceErr(err);
	return err;
}

struct dentry *sio_lkup_one(const char *name, struct dentry *parent, int len,
			    struct vfsmount *mnt)
{
	struct dentry *dentry;

	LKTRTrace("%.*s/%.*s\n", DLNPair(parent), len, name);
	IMustLock(parent->d_inode);

	if (!test_perm(parent->d_inode, MAY_EXEC))
		dentry = lkup_one(name, parent, len, mnt);
	else {
		struct lkup_one_args args = {
			.errp	= &dentry,
			.name	= name,
			.parent	= parent,
			.len	= len,
			.mnt	= mnt
		};
		wkq_wait(call_lkup_one, &args);
	}

	TraceErrPtr(dentry);
	return dentry;
}

/*
 * lookup @dentry on @bindex which should be negative.
 */
int lkup_neg(struct dentry *dentry, aufs_bindex_t bindex)
{
	int err;
	struct dentry *parent, *hidden_parent, *hidden_dentry;
	struct inode *hidden_dir;

	LKTRTrace("%.*s, b%d\n", DLNPair(dentry), bindex);
	parent = dentry->d_parent;
	DEBUG_ON(!parent || !parent->d_inode
		 || !S_ISDIR(parent->d_inode->i_mode));
	hidden_parent = h_dptr_i(parent, bindex);
	DEBUG_ON(!hidden_parent);
	hidden_dir = hidden_parent->d_inode;
	DEBUG_ON(!hidden_dir || !S_ISDIR(hidden_dir->i_mode));
	IMustLock(hidden_dir);

	hidden_dentry = sio_lkup_one(dentry->d_name.name, hidden_parent,
				     dentry->d_name.len,
				     mnt_nfs(dentry->d_sb, bindex));
	//if (LktrCond) {dput(hidden_dentry); hidden_dentry = ERR_PTR(-1);}
	err = PTR_ERR(hidden_dentry);
	if (IS_ERR(hidden_dentry))
		goto out;
	if (unlikely(hidden_dentry->d_inode)) {
		err = -EIO;
		IOErr("b%d %.*s should be negative\n",
		      bindex, DLNPair(hidden_dentry));
		dput(hidden_dentry);
		goto out;
	}

	if (bindex < dbstart(dentry))
		set_dbstart(dentry, bindex);
	if (dbend(dentry) < bindex)
		set_dbend(dentry, bindex);
	set_h_dptr(dentry, bindex, hidden_dentry);
	err = 0;

 out:
	TraceErr(err);
	return err;
}

/*
 * returns the number of found hidden positive dentries,
 * otherwise an error.
 */
int refresh_hdentry(struct dentry *dentry, mode_t type)
{
	int npositive, pgen, new_sz, sgen, dgen;
	struct aufs_dinfo *dinfo;
	struct super_block *sb;
	struct dentry *parent;
	aufs_bindex_t bindex, parent_bend, parent_bstart, bwh, bdiropq, bend;
	struct aufs_hdentry *p;
	//struct nameidata nd;

	LKTRTrace("%.*s, type 0%o\n", DLNPair(dentry), type);
	DiMustWriteLock(dentry);
	sb = dentry->d_sb;
	DEBUG_ON(IS_ROOT(dentry));
	parent = dentry->d_parent; // dget_parent()
	pgen = digen(parent);
	sgen = sigen(sb);
	dgen = digen(dentry);
	DEBUG_ON(pgen != sgen
		 || AufsGenOlder(sgen, dgen)
		 || AufsGenOlder(pgen, dgen));

	npositive = -ENOMEM;
	new_sz = sizeof(*dinfo->di_hdentry) * (sbend(sb) + 1);
	dinfo = dtodi(dentry);
	p = kzrealloc(dinfo->di_hdentry, sizeof(*p) * (dinfo->di_bend + 1),
		      new_sz);
	//p = NULL;
	if (unlikely(!p))
		goto out;
	dinfo->di_hdentry = p;
	//smp_mb();

	bend = dinfo->di_bend;
	bwh = dinfo->di_bwh;
	bdiropq = dinfo->di_bdiropq;
	p += dinfo->di_bstart;
	for (bindex = dinfo->di_bstart; bindex <= bend; bindex++, p++) {
		struct dentry *hd;
		struct aufs_hdentry tmp, *q;
		aufs_bindex_t new_bindex;

		hd = p->hd_dentry;
		if (!hd)
			continue;
		DEBUG_ON(!hd->d_inode);
		if (hd->d_parent == h_dptr_i(parent, bindex)) // dget_parent()
			continue;

		new_bindex = find_dbindex(parent, hd->d_parent); // dget_parent()
		DEBUG_ON(new_bindex == bindex);
		if (dinfo->di_bwh == bindex)
			bwh = new_bindex;
		if (dinfo->di_bdiropq == bindex)
			bdiropq = new_bindex;
		if (new_bindex < 0) { // test here
			hdput(p);
			p->hd_dentry = NULL;
			continue;
		}
		/* swap two hidden dentries, and loop again */
		q = dinfo->di_hdentry + new_bindex;
		tmp = *q;
		*q = *p;
		*p = tmp;
		if (tmp.hd_dentry) {
			bindex--;
			p--;
		}
	}

	dinfo->di_bwh = bwh;
	dinfo->di_bdiropq = bdiropq;
	parent_bend = dbend(parent);
	p = dinfo->di_hdentry;
	for (bindex = 0; bindex <= parent_bend; bindex++, p++)
		if (p->hd_dentry) {
			dinfo->di_bstart = bindex;
			break;
		}
	p = dinfo->di_hdentry + parent_bend;
	for (bindex = parent_bend; bindex >= 0; bindex--, p--)
		if (p->hd_dentry) {
			dinfo->di_bend = bindex;
			break;
		}
	//smp_mb();

	npositive = 0;
	parent_bstart = dbstart(parent);
	if (type != S_IFDIR && dinfo->di_bstart == parent_bstart)
		goto out_dgen; /* success */

#if 0
	nd.last_type = LAST_ROOT;
	nd.flags = LOOKUP_FOLLOW;
	nd.depth = 0;
	nd.mnt = mntget(??);
	nd.dentry = dget(parent);
#endif
	npositive = lkup_dentry(dentry, parent_bstart, type);
	//if (LktrCond) npositive = -1;
	if (npositive < 0)
		goto out;

 out_dgen:
	update_digen(dentry);
 out:
	TraceErr(npositive);
	return npositive;
}

static int hidden_d_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	int err, plus, udba, locked;
	struct nameidata fake_nd, *p;
	aufs_bindex_t bindex, btail, bstart;
	struct super_block *sb;
	struct inode *inode, *first;
	umode_t mode;
	struct dentry *hidden_dentry;
	int (*reval)(struct dentry *, struct nameidata *);

	LKTRTrace("%.*s\n", DLNPair(dentry));

	err = 0;
	sb = dentry->d_sb;
	plus = 0;
	mode = 0;
	first = NULL;
	inode = dentry->d_inode;

	/*
	 * Theoretically, REVAL test should be unnecessary in case of INOTIFY.
	 * But inotify doesn't fire some necessary events,
	 *	IN_ATTRIB for atime/nlink/pageio
	 *	IN_DELETE for NFS dentry
	 * Let's do REVAL test too.
	 */
	udba = IS_MS(sb, (MS_UDBA_REVAL | MS_UDBA_INOTIFY));
	if (udba && inode) {
		mode = inode->i_mode & S_IFMT;
		plus = inode->i_nlink > 0;
		first = h_iptr(inode);
	}

	btail = bstart = dbstart(dentry);
	if (inode && S_ISDIR(inode->i_mode))
		btail = dbtaildir(dentry);
	locked = 0;
	if (nd) {
		fake_nd = *nd;
#ifndef CONFIG_AUFS_FAKE_DM
		if (dentry != nd->dentry) {
			di_read_lock(nd->dentry, 0);
			locked = 1;
		}
#endif
	}
	for (bindex = bstart; bindex <= btail; bindex++) {
		hidden_dentry = h_dptr_i(dentry, bindex);
		if (unlikely(!hidden_dentry))
			continue;

		reval = NULL;
		if (hidden_dentry->d_op)
			reval = hidden_dentry->d_op->d_revalidate;
		if (unlikely(reval)) {
			p = fake_dm(&fake_nd, nd, sb, bindex);
			DEBUG_ON(IS_ERR(p));
			if (unlikely(!reval(hidden_dentry, p))) {
				err = -EINVAL;
#if 0
				if (unlikely(SB_AUFS(hidden_dentry->d_sb))) {
					struct dentry *h_d;
					struct qstr *name = &hidden_dentry->d_name;
					h_d = lkup_one(name->name, hidden_dentry->d_parent,
						       name->len, NULL);
					if (!IS_ERR(h_d)) {
						if (h_d == hidden_dentry)
							err = 0;
						dput(h_d);
					}
				}
#endif
			}
			fake_dm_release(p);
			if (unlikely(err))
				break;
		}

		if (udba) {
			struct inode *hinode = hidden_dentry->d_inode;
			int hplus = 0;
			umode_t hmode = 0;
			if (hinode) {
				hmode = hinode->i_mode & S_IFMT;
				hplus = hinode->i_nlink > 0;
			}
			if (unlikely(plus != hplus || mode != hmode)) {
				err = -EINVAL;
				break;
			}
		}
	}
#ifndef CONFIG_AUFS_FAKE_DM
	if (unlikely(locked))
		di_read_unlock(nd->dentry, 0);
#endif

#if 0
	// some filesystem uses CURRENT_TIME_SEC instead of CURRENT_TIME.
	// NFS may stop IN_DELETE because of DCACHE_NFSFS_RENAMED.
#if 0
		     && (!timespec_equal(&inode->i_ctime, &first->i_ctime)
			 || !timespec_equal(&inode->i_atime, &first->i_atime))
#endif
	if (unlikely(!err && udba && first))
		cpup_attr_all(inode);
#endif

	TraceErr(err);
	return err;
}

static int simple_reval_dpath(struct dentry *dentry, int sgen)
{
	int err;
	mode_t type;
	struct dentry *parent;

	LKTRTrace("%.*s, sgen %d\n", DLNPair(dentry), sgen);
	SiMustAnyLock(dentry->d_sb);
	DiMustWriteLock(dentry);
	DEBUG_ON(!dentry->d_inode);

	if (!AufsGenOlder(digen(dentry), sgen))
		return 0;

	parent = dget_parent(dentry);
	di_read_lock(parent, AUFS_I_RLOCK);
	DEBUG_ON(digen(parent) != sgen);
#ifdef CONFIG_AUFS_DEBUG
	{
		struct dentry *d = parent;
		while (!IS_ROOT(d)) {
			DEBUG_ON(digen(d) != sgen);
			d = d->d_parent;
		}
	}
#endif
	type = dentry->d_inode->i_mode & S_IFMT;
	/* returns a number of positive dentries */
	err = refresh_hdentry(dentry, type);
	if (err >= 0)
		err = refresh_hinode(dentry);
	di_read_unlock(parent, AUFS_I_RLOCK);
	dput(parent);
	TraceErr(err);
	return err;
}

int reval_dpath(struct dentry *dentry, int sgen)
{
	int err;
	struct dentry *d, *parent;

	LKTRTrace("%.*s, sgen %d\n", DLNPair(dentry), sgen);
	DEBUG_ON(!dentry->d_inode);
	DiMustWriteLock(dentry);

	if (!stosi(dentry->d_sb)->si_failed_refresh_dirs)
		return simple_reval_dpath(dentry, sgen);

	/* slow loop, keep it simple and stupid */
	/* cf: cpup_dirs() */
	err = 0;
	while (digen(dentry) != sgen) {
		d = dentry;
		while (1) {
			parent = d->d_parent; // dget_parent()
			if (digen(parent) == sgen)
				break;
			d = parent;
		}

		if (d != dentry)
			di_write_lock(d);

		/* someone might update our dentry while we were sleeping */
		if (AufsGenOlder(digen(d), sgen)) {
			di_read_lock(parent, AUFS_I_RLOCK);
			/* returns a number of positive dentries */
			err = refresh_hdentry(d, d->d_inode->i_mode & S_IFMT);
			//err = -1;
			if (err >= 0)
				err = refresh_hinode(d);
			//err = -1;
			di_read_unlock(parent, AUFS_I_RLOCK);
		}

		if (d != dentry)
			di_write_unlock(d);
		if (unlikely(err))
			break;
	}

	TraceErr(err);
	return err;
}

/*
 * THIS IS A BOOLEAN FUNCTION: returns 1 if valid, 0 otherwise.
 * nfsd passes NULL as nameidata.
 */
static int aufs_d_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	int valid, sgen, err;
	struct super_block *sb;

	LKTRTrace("dentry %.*s\n", DLNPair(dentry));
	if (nd && nd->dentry)
		LKTRTrace("nd %.*s\n", DLNPair(nd->dentry));
	//dir case: DEBUG_ON(dentry->d_parent != nd->dentry);
	//remove failure case: DEBUG_ON(!IS_ROOT(dentry) && d_unhashed(dentry));
	DEBUG_ON(!dentry->d_fsdata);

	err = -EINVAL;
	DEBUG_ON(direval_test(dentry) < 0);
	if (unlikely(direval_test(dentry))) {
#ifdef ForceHinotify
		Dbg("UDBA %.*s\n", DLNPair(dentry));
#endif
		if (IS_ROOT(dentry))
			atomic_set(&dtodi(dentry)->di_reval, 0);
#if 0
		/* root directory was handled in aufs_inotify() */
		DEBUG_ON(IS_ROOT(dentry));
#endif
		goto out;
	}

	sb = dentry->d_sb;
	si_read_lock(sb);
	sgen = sigen(dentry->d_sb);
	if (digen(dentry) == sgen)
		di_read_lock(dentry, AUFS_I_RLOCK);
	else {
		DEBUG_ON(!dentry->d_inode);
		di_write_lock(dentry);
		err = reval_dpath(dentry, sgen);
		//err = -1;
		di_downgrade_lock(dentry, AUFS_I_RLOCK);
		if (unlikely(err))
			goto out_unlock;
	}

#if 0 // fix it
	/* parent dir i_nlink is not updated in the case of setattr */
	if (S_ISDIR(inode->i_mode)) {
		i_lock(inode);
		ii_write_lock(inode);
		cpup_attr_nlink(inode);
		ii_write_unlock(inode);
		i_unlock(inode);
	}
#endif
	err = hidden_d_revalidate(dentry, nd);
	//err = -1;

 out_unlock:
	aufs_read_unlock(dentry, AUFS_I_RLOCK);
 out:
	TraceErr(err);
	valid = !err;
	if (!valid)
		LKTRTrace("%.*s invalid\n", DLNPair(dentry));
	return valid;
}

static void aufs_d_release(struct dentry *dentry)
{
	struct aufs_dinfo *dinfo;
	aufs_bindex_t bend, bindex;

	LKTRTrace("%.*s\n", DLNPair(dentry));
	DEBUG_ON(!d_unhashed(dentry));

	dinfo = dentry->d_fsdata;
	if (unlikely(!dinfo))
		return;

	/* dentry may not be revalidated */
	bindex = dinfo->di_bstart;
	if (bindex >= 0) {
		struct aufs_hdentry *p;
		bend = dinfo->di_bend;
		DEBUG_ON(bend < bindex);
		p = dinfo->di_hdentry + bindex;
		while (bindex++ <= bend) {
			if (p->hd_dentry)
				hdput(p);
			p++;
		}
	}
	DEBUG_ON(direval_test(dentry) < 0);
	rw_destroy(&dinfo->di_rwsem);
	kfree(dinfo->di_hdentry);
	cache_free_dinfo(dinfo);
}

/* it may be called at remount time, too */
static void aufs_d_iput(struct dentry *dentry, struct inode *inode)
{
	struct super_block *sb;

	LKTRTrace("%.*s, i%lu\n", DLNPair(dentry), inode->i_ino);

	sb = dentry->d_sb;
	si_read_lock(sb);
	if (unlikely(IS_MS(sb, MS_PLINK) && is_plinked(sb, inode))) {
		ii_write_lock(inode);
		update_brange(inode, 1);
		ii_write_unlock(inode);
	}
	si_read_unlock(sb);
	iput(inode);
}

struct dentry_operations aufs_dop = {
	.d_revalidate	= aufs_d_revalidate,
	.d_release	= aufs_d_release,
	.d_iput		= aufs_d_iput
};
