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

/* $Id: branch.c,v 1.42 2007/03/19 04:29:53 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
#include "aufs.h"

static void free_branch(struct aufs_branch *br)
{
	if (br->br_xino)
		fput(br->br_xino);
	if (unlikely(br->br_wh))
		dput(br->br_wh);
	if (unlikely(br->br_plink))
		dput(br->br_plink);
	rw_destroy(&br->br_wh_rwsem);
	mntput(br->br_mnt);
	DEBUG_ON(br_count(br) || atomic_read(&br->br_wh_running));
	kfree(br);
}

/*
 * frees all branches
 */
void free_branches(struct aufs_sbinfo *sbinfo)
{
	aufs_bindex_t bmax;
	struct aufs_branch **br;

	TraceEnter();
	bmax = sbinfo->si_bend + 1;
	br = sbinfo->si_branch;
	while (bmax--)
		free_branch(*br++);
}

/*
 * find the index of a branch which is specified by @br_id.
 */
int find_brindex(struct super_block *sb, aufs_bindex_t br_id)
{
	aufs_bindex_t bindex, bend;

	TraceEnter();

	bend = sbend(sb);
	for (bindex = 0; bindex <= bend; bindex++)
		if (sbr_id(sb, bindex) == br_id)
			return bindex;
	return -1;
}

/*
 * test if the @br is readonly or not.
 */
int br_rdonly(struct aufs_branch *br)
{
	return ((br->br_mnt->mnt_sb->s_flags & MS_RDONLY)
		|| !(br->br_perm & MAY_WRITE))
		? -EROFS : 0;
}

/*
 * returns writable branch index, otherwise an error.
 * todo: customizable writable-branch-policy
 */
static int find_rw_parent(struct dentry *dentry, aufs_bindex_t bend)
{
	aufs_bindex_t bindex, candidate;
	struct super_block *sb;
	struct dentry *parent, *hidden_parent;

	candidate = -1;
	sb = dentry->d_sb;
	parent = dentry->d_parent; // dget_parent()
#if 1 // branch policy
	hidden_parent = h_dptr_i(parent, bend);
	if (hidden_parent && !br_rdonly(stobr(sb, bend)))
		return bend;
#endif

	for (bindex = dbstart(parent); bindex <= bend; bindex++) {
		hidden_parent = h_dptr_i(parent, bindex);
		if (hidden_parent && !br_rdonly(stobr(sb, bindex))) {
#if 0 // branch policy
			if (candidate == -1)
				candidate = bindex;
			if (!au_test_perm(hidden_parent->d_inode, MAY_WRITE))
#endif
				return bindex;
		}
	}
#if 0 // branch policy
	if (candidate != -1)
		return candidate;
#endif
	return -EROFS;
}

int find_rw_br(struct super_block *sb, aufs_bindex_t bend)
{
	aufs_bindex_t bindex;

	for (bindex = bend; bindex >= 0; bindex--)
		if (!br_rdonly(stobr(sb, bindex)))
			return bindex;
	return -EROFS;
}

int find_rw_parent_br(struct dentry *dentry, aufs_bindex_t bend)
{
	int err;

	err = find_rw_parent(dentry, bend);
	if (err >= 0)
		return err;
	return find_rw_br(dentry->d_sb, bend);
}

/* ---------------------------------------------------------------------- */

/*
 * test if two hidden_dentries have overlapping branches.
 */
static int do_is_overlap(struct super_block *sb, struct dentry *hidden_d1,
			 struct dentry *hidden_d2)
{
	struct dentry *d;

	d = hidden_d1;
	do {
		if (unlikely(d == hidden_d2))
			return 1;
		d = d->d_parent; // dget_parent()
	} while (!IS_ROOT(d));

	return (d == hidden_d2);
}

#if defined(CONFIG_BLK_DEV_LOOP) || defined(CONFIG_BLK_DEV_LOOP_MODULE)
#include <linux/loop.h>
static int is_overlap_loopback(struct super_block *sb, struct dentry *hidden_d1,
			       struct dentry *hidden_d2)
{
	struct inode *hidden_inode;
	struct loop_device *l;

	hidden_inode = hidden_d1->d_inode;
	if (MAJOR(hidden_inode->i_sb->s_dev) != LOOP_MAJOR)
		return 0;

	l = hidden_inode->i_sb->s_bdev->bd_disk->private_data;
	hidden_d1 = l->lo_backing_file->f_dentry;
	if (unlikely(hidden_d1->d_sb == sb))
		return 1;
	return do_is_overlap(sb, hidden_d1, hidden_d2);
}
#else
#define is_overlap_loopback(sb, hidden_d1, hidden_d2) 0
#endif

static int is_overlap(struct super_block *sb, struct dentry *hidden_d1,
		      struct dentry *hidden_d2)
{
	LKTRTrace("d1 %.*s, d2 %.*s\n", DLNPair(hidden_d1), DLNPair(hidden_d2));
	if (unlikely(hidden_d1 == hidden_d2))
		return 1;
	return do_is_overlap(sb, hidden_d1, hidden_d2)
		|| do_is_overlap(sb, hidden_d2, hidden_d1)
		|| is_overlap_loopback(sb, hidden_d1, hidden_d2)
		|| is_overlap_loopback(sb, hidden_d2, hidden_d1);
}

/* ---------------------------------------------------------------------- */

/*
 * returns a newly allocated branch. @new_nbranch is a number of branches
 * after adding a branch.
 */
static struct aufs_branch *alloc_addbr(struct super_block *sb, int new_nbranch)
{
	struct aufs_branch **branchp, *add_branch;
	int sz;
	void *p;
	struct dentry *root;
	struct inode *inode;
	struct aufs_hinode *hinodep;
	struct aufs_hdentry *hdentryp;

	LKTRTrace("new_nbranch %d\n", new_nbranch);
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	inode = root->d_inode;
	IiMustWriteLock(inode);

	add_branch = kmalloc(sizeof(*add_branch), GFP_KERNEL);
	//if (LktrCond) {kfree(add_branch); add_branch = NULL;}
	if (unlikely(!add_branch))
		goto out;

	sz = sizeof(*branchp) * (new_nbranch - 1);
	if (unlikely(!sz))
		sz = sizeof(*branchp);
	p = stosi(sb)->si_branch;
	branchp = kzrealloc(p, sz, sizeof(*branchp) * new_nbranch);
	//if (LktrCond) branchp = NULL;
	if (unlikely(!branchp))
		goto out;
	stosi(sb)->si_branch = branchp;

	sz = sizeof(*hdentryp) * (new_nbranch - 1);
	if (unlikely(!sz))
		sz = sizeof(*hdentryp);
	p = dtodi(root)->di_hdentry;
	hdentryp = kzrealloc(p, sz, sizeof(*hdentryp) * new_nbranch);
	//if (LktrCond) hdentryp = NULL;
	if (unlikely(!hdentryp))
		goto out;
	dtodi(root)->di_hdentry = hdentryp;

	sz = sizeof(*hinodep) * (new_nbranch - 1);
	if (unlikely(!sz))
		sz = sizeof(*hinodep);
	p = itoii(inode)->ii_hinode;
	hinodep = kzrealloc(p, sz, sizeof(*hinodep) * new_nbranch);
	//if (LktrCond) hinodep = NULL; // unavailable test
	if (unlikely(!hinodep))
		goto out;
	itoii(inode)->ii_hinode = hinodep;
	return add_branch; /* success */

 out:
	kfree(add_branch);
	TraceErr(-ENOMEM);
	return ERR_PTR(-ENOMEM);
}

/*
 * test if the branch permission is legal or not.
 */
static int test_br(struct super_block *sb, struct inode *inode,
		   unsigned int perm, char *path)
{
	int err;

	err = 0;
	if (unlikely((perm & MAY_WRITE) && IS_RDONLY(inode))) {
		Err("write permission for readonly fs or inode, %s\n", path);
		err = -EINVAL;
	}

	TraceErr(err);
	return err;
}

/*
 * retunrs,,,
 * 0: success, the caller will add it
 * plus: success, it is already unified, the caller should ignore it
 * minus: error
 */
static int test_add(struct super_block *sb, struct opt_add *add, int remount)
{
	int err;
	struct dentry *root;
	struct inode *inode, *hidden_inode;
	aufs_bindex_t bend, bindex;

	LKTRTrace("%s, remo%d\n", add->path, remount);

	root = sb->s_root;
	if (unlikely(au_find_dbindex(root, add->nd.dentry) != -1)) {
		err = 1;
		if (!remount) {
			err = -EINVAL;
			Err("%s duplicated\n", add->path);
		}
		goto out;
	}

	err = -ENOSPC;
	bend = sbend(sb);
	//if (LktrCond) bend = AUFS_BRANCH_MAX;
	if (unlikely(AUFS_BRANCH_MAX <= add->bindex
		    || AUFS_BRANCH_MAX - 1 <= bend)) {
		Err("number of branches exceeded %s\n", add->path);
		goto out;
	}

	err = -EDOM;
	if (unlikely(add->bindex < 0 || bend + 1 < add->bindex)) {
		Err("bad index %d\n", add->bindex);
		goto out;
	}

	inode = add->nd.dentry->d_inode;
	DEBUG_ON(!inode || !S_ISDIR(inode->i_mode));
	err = -ENOENT;
	if (unlikely(!inode->i_nlink)) {
		Err("no existence %s\n", add->path);
		goto out;
	}

	err = -EINVAL;
	if (unlikely(inode->i_sb == sb)) {
		Err("%s must be outside\n", add->path);
		goto out;
	}

#if 1 //ndef CONFIG_AUFS_AS_BRANCH
	if (unlikely(SB_AUFS(inode->i_sb)
		     || !strcmp(sbtype(inode->i_sb), "unionfs"))) {
		Err("nested " AUFS_NAME " %s\n", add->path);
		goto out;
	}
#endif

#ifdef AufsNoNfsBranch
	if (unlikely(SB_NFS(inode->i_sb))) {
		Err(AufsNoNfsBranchMsg ". %s\n", add->path);
		goto out;
	}
#endif

	err = test_br(sb, add->nd.dentry->d_inode, add->perm, add->path);
	if (unlikely(err))
		goto out;

	if (unlikely(bend == -1))
		return 0; /* success */

	hidden_inode = h_dptr(root)->d_inode;
	if (unlikely(IS_MS(sb, MS_WARN_PERM)
		     && ((hidden_inode->i_mode & S_IALLUGO)
			 != (inode->i_mode & S_IALLUGO)
			 || hidden_inode->i_uid != inode->i_uid
			 || hidden_inode->i_gid != inode->i_gid)))
		Warn("different uid/gid/permission, %s\n", add->path);

	err = -EINVAL;
	for (bindex = 0; bindex <= bend; bindex++)
		if (unlikely(is_overlap(sb, add->nd.dentry,
					h_dptr_i(root, bindex)))) {
			Err("%s is overlapped\n", add->path);
			goto out;
		}
	err = 0;

 out:
	TraceErr(err);
	return err;
}

int br_add(struct super_block *sb, struct opt_add *add, int remount)
{
	int err, sz;
	aufs_bindex_t bend, add_bindex;
	struct dentry *root;
	struct aufs_iinfo *iinfo;
	struct aufs_sbinfo *sbinfo;
	struct aufs_dinfo *dinfo;
	struct inode *root_inode;
	unsigned long long maxb;
	struct aufs_branch **branchp, *add_branch;
	struct aufs_hdentry *hdentryp;
	struct aufs_hinode *hinodep;

	LKTRTrace("b%d, %s, 0x%x, %.*s\n", add->bindex, add->path,
		  add->perm, DLNPair(add->nd.dentry));
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	root_inode = root->d_inode;
	IMustLock(root_inode);
	IiMustWriteLock(root_inode);

	err = test_add(sb, add, remount);
	if (unlikely(err < 0))
		goto out;
	if (unlikely(err))
		return 0; /* success */

	bend = sbend(sb);
	add_branch = alloc_addbr(sb, bend + 2);
	err = PTR_ERR(add_branch);
	if (IS_ERR(add_branch))
		goto out;
	add_branch->br_plink = NULL;
	/*
	 * for the moment, aufs supports the branch filesystem
	 * which does not support link(2).
	 * testing on FAT which does not support i_op->setattr() fully either,
	 * copyup failed.
	 * finally, such filesystem will not be used as the writable branch.
	 */
	if (!(add->perm & MAY_WRITE)
	    || !add->nd.dentry->d_inode->i_op
	    || !add->nd.dentry->d_inode->i_op->link) {
		rw_init_nolock(&add_branch->br_wh_rwsem);
		add_branch->br_wh = NULL;
	} else {
		hi_lock_parent(add->nd.dentry->d_inode);
		rw_init_wlock(&add_branch->br_wh_rwsem, AUFS_LSC_BR_WH);
		err = init_wh(add->nd.dentry, add_branch,
			      do_mnt_nfs(add->nd.mnt));
		//if (LktrCond)
		//{dput(add_branch->br_wh); add_branch->br_wh = NULL; err = -1;}
		br_wh_write_unlock(add_branch);
		i_unlock(add->nd.dentry->d_inode);
		if (unlikely(err)) {
			rw_destroy(&add_branch->br_wh_rwsem);
			kfree(add_branch);
			goto out;
		}
	}
	add_branch->br_xino = NULL;
	add_branch->br_mnt = mntget(add->nd.mnt);
	atomic_set(&add_branch->br_wh_running, 0);
	add_branch->br_id = new_br_id(sb);
	add_branch->br_perm = add->perm;
	atomic_set(&add_branch->br_count, 0);
	err = 0;

	sbinfo = stosi(sb);
	dinfo = dtodi(root);
	iinfo = itoii(root_inode);

	add_bindex = add->bindex;
	sz = sizeof(*(sbinfo->si_branch)) * (bend + 1 - add_bindex);
	branchp = sbinfo->si_branch + add_bindex;
	memmove(branchp + 1, branchp, sz);
	*branchp = add_branch;
	sz = sizeof(*hdentryp) * (bend + 1 - add_bindex);
	hdentryp = dinfo->di_hdentry + add_bindex;
	memmove(hdentryp + 1, hdentryp, sz);
	hdentryp->hd_dentry = NULL;
	sz = sizeof(*hinodep) * (bend + 1 - add_bindex);
	hinodep = iinfo->ii_hinode + add_bindex;
	memmove(hinodep + 1, hinodep, sz);
	hinodep->hi_inode = NULL;
	hinodep->hi_notify = NULL;

	sbinfo->si_bend++;
	dinfo->di_bend++;
	iinfo->ii_bend++;
	if (unlikely(bend == -1)) {
		dinfo->di_bstart = 0;
		iinfo->ii_bstart = 0;
	}
	set_h_dptr(root, add_bindex, dget(add->nd.dentry));
	set_h_iptr(root_inode, add_bindex, igrab(add->nd.dentry->d_inode), 0);
	if (!add_bindex)
		au_cpup_attr_all(root_inode);
	else
		au_add_nlink(root_inode, add->nd.dentry->d_inode);
	maxb = add->nd.dentry->d_sb->s_maxbytes;
	if (sb->s_maxbytes < maxb)
		sb->s_maxbytes = maxb;
	au_sigen_inc(sb);

	if (IS_MS(sb, MS_XINO)) {
		struct file *base_file = stobr(sb, 0)->br_xino;
		if (!add_bindex)
			base_file = stobr(sb, 1)->br_xino;
		err = xino_init(sb, add_bindex, base_file, /*do_test*/1);
		if (unlikely(err)) {
			DEBUG_ON(add_branch->br_xino);
			Err("ignored xino err %d, force noxino\n", err);
			err = 0;
			MS_CLR(sb, MS_XINO);
		}
	}

 out:
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * test if the branch is deletable or not.
 */
static int test_children(struct dentry *root, aufs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *this_parent = root;
	struct list_head *next;

	LKTRTrace("b%d\n", bindex);
	SiMustWriteLock(root->d_sb);
	DiMustWriteLock(root);
	DiMustNoWaiters(root);
	IiMustNoWaiters(root->d_inode);
	di_write_unlock(root);

	//spin_lock(&dcache_lock);
 repeat:
	next = this_parent->d_subdirs.next;
 resume:
	while (next != &this_parent->d_subdirs) {
		struct list_head *tmp = next;
		struct dentry *dentry = list_entry(tmp, struct dentry, D_CHILD);

		next = tmp->next;
		di_read_lock_child(dentry, AUFS_I_RLOCK);
		if (!h_dptr_i(dentry, bindex)
		    || d_unhashed(dentry)
		    || !dentry->d_inode) {
			di_read_unlock(dentry, AUFS_I_RLOCK);
			continue;
		}
		if (!list_empty(&dentry->d_subdirs)) {
			this_parent = dentry;
			di_read_unlock(dentry, AUFS_I_RLOCK);
			goto repeat;
		}
		if (!atomic_read(&dentry->d_count)
		    || (S_ISDIR(dentry->d_inode->i_mode)
			&& dbstart(dentry) != dbend(dentry))) {
			di_read_unlock(dentry, AUFS_I_RLOCK);
			continue;
		}
		//atomic_dec(&dentry->d_count);
		//Dbg("%.*s\n", DLNPair(dentry));
		ret = 1;
		di_read_unlock(dentry, AUFS_I_RLOCK);
		goto out;
	}
	if (this_parent != root) {
		next = this_parent->D_CHILD.next;
		di_read_lock_child(this_parent, AUFS_I_RLOCK);
		//atomic_dec(&this_parent->d_count);
		if (unlikely(h_dptr_i(this_parent, bindex)
			     && atomic_read(&this_parent->d_count)
			     && (!S_ISDIR(this_parent->d_inode->i_mode)
				 || dbstart(this_parent) == dbend(this_parent))
			    )) {
			ret = 1;
			di_read_unlock(this_parent, AUFS_I_RLOCK);
			//Dbg("%.*s\n", DLNPair(this_parent));
			goto out;
		}
		di_read_unlock(this_parent, AUFS_I_RLOCK);
		this_parent = this_parent->d_parent;
		goto resume;
	}

 out:
	di_write_lock_child(root); /* aufs_write_lock() calls ..._child() */
	//spin_unlock(&dcache_lock);
	TraceErr(ret);
	return ret;
}

int br_del(struct super_block *sb, struct opt_del *del, int remount)
{
	int err, do_wh, rerr;
	struct dentry *root;
	struct inode *inode, *hidden_dir;
	aufs_bindex_t bindex, bend;
	struct aufs_sbinfo *sbinfo;
	struct aufs_dinfo *dinfo;
	struct aufs_iinfo *iinfo;
	struct aufs_branch *br;
	aufs_bindex_t br_id;

	LKTRTrace("%s, %.*s\n", del->path, DLNPair(del->h_root));
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	inode = root->d_inode;
	IiMustWriteLock(inode);

	bindex = au_find_dbindex(root, del->h_root);
	if (unlikely(bindex < 0)) {
		if (remount)
			return 0; /* success */
		err = -ENOENT;
		Err("%s no such branch\n", del->path);
		goto out;
	}
	LKTRTrace("bindex b%d\n", bindex);

	err = -EBUSY;
	bend = sbend(sb);
	br = stobr(sb, bindex);
	if (unlikely(!bend || br_count(br))) {
		LKTRTrace("bend %d, br_count %d\n", bend, br_count(br));
		goto out;
	}

	do_wh = 0;
	if (unlikely(br->br_wh)) {
		//no need to lock br_wh_rwsem
		dput(br->br_wh);
		br->br_wh = NULL;
		do_wh = 1;
		DEBUG_ON(!br->br_plink);
		dput(br->br_plink);
		br->br_plink = NULL;
	}

	if (unlikely(test_children(root, bindex))) {
		if (unlikely(do_wh))
			goto out_wh;
		goto out;
	}
	err = 0;

	sbinfo = stosi(sb);
	dinfo = dtodi(root);
	iinfo = itoii(inode);

	dput(h_dptr_i(root, bindex));
	aufs_hiput(iinfo->ii_hinode + bindex);
	br_id = br->br_id;
	free_branch(br);

	//todo: realloc and shrink memeory
	if (bindex < bend) {
		const aufs_bindex_t n = bend - bindex;
		struct aufs_branch **brp;
		struct aufs_hdentry *hdp;
		struct aufs_hinode *hip;

		brp = sbinfo->si_branch + bindex;
		memmove(brp, brp + 1, sizeof(*brp) * n);
		hdp = dinfo->di_hdentry + bindex;
		memmove(hdp, hdp + 1, sizeof(*hdp) * n);
		hip = iinfo->ii_hinode + bindex;
		memmove(hip, hip + 1, sizeof(*hip) * n);
	}
	sbinfo->si_branch[0 + bend] = NULL;
	dinfo->di_hdentry[0 + bend].hd_dentry = NULL;
	iinfo->ii_hinode[0 + bend].hi_inode = NULL;
	iinfo->ii_hinode[0 + bend].hi_notify = NULL;

	sbinfo->si_bend--;
	dinfo->di_bend--;
	iinfo->ii_bend--;
	au_sigen_inc(sb);
	if (!bindex)
		au_cpup_attr_all(inode);
	else
		au_sub_nlink(inode, del->h_root->d_inode);
	if (IS_MS(sb, MS_PLINK))
		half_refresh_plink(sb, br_id);

	if (sb->s_maxbytes == del->h_root->d_sb->s_maxbytes) {
		bend--;
		sb->s_maxbytes = 0;
		for (bindex = 0; bindex <= bend; bindex++) {
			unsigned long long maxb;
			maxb = sbr_sb(sb, bindex)->s_maxbytes;
			if (sb->s_maxbytes < maxb)
				sb->s_maxbytes = maxb;
		}
	}
	goto out; /* success */

 out_wh:
	/* revert */
	hidden_dir = del->h_root->d_inode;
	if (hidden_dir->i_op && hidden_dir->i_op->link) {
		struct inode *dir = sb->s_root->d_inode;
		hdir_lock(hidden_dir, dir, bindex);
		br_wh_write_lock(br);
		rerr = init_wh(del->h_root, br, do_mnt_nfs(br->br_mnt));
		br_wh_write_unlock(br);
		hdir_unlock(hidden_dir, dir, bindex);
		if (rerr)
			Warn("failed re-creating base whiteout, %s. (%d)\n",
			     del->path, rerr);
	}
 out:
	TraceErr(err);
	return err;
}

int br_mod(struct super_block *sb, struct opt_mod *mod, int remount)
{
	int err;
	struct dentry *root;
	aufs_bindex_t bindex;
	struct aufs_branch *br;
	struct inode *hidden_dir;

	LKTRTrace("%s, %.*s, 0x%x\n",
		  mod->path, DLNPair(mod->h_root), mod->perm);
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	IiMustWriteLock(root->d_inode);

	bindex = au_find_dbindex(root, mod->h_root);
	if (unlikely(bindex < 0)) {
		if (remount)
			return 0; /* success */
		err = -ENOENT;
		Err("%s no such branch\n", mod->path);
		goto out;
	}
	LKTRTrace("bindex b%d\n", bindex);

	hidden_dir = mod->h_root->d_inode;
	err = test_br(sb, hidden_dir, mod->perm, mod->path);
	if (unlikely(err))
		goto out;

	br = stobr(sb, bindex);
	if (unlikely(br->br_perm == mod->perm))
		return 0; /* success */

	if (br->br_perm & MAY_WRITE) {
		if (!(mod->perm & MAY_WRITE)) {
			/* rw --> ro, file might be mmapped */
			struct file *file, *hf;

			if (br->br_wh) {
				// no need to lock br_wh_rwsem
				dput(br->br_wh);
				br->br_wh = NULL;
				DEBUG_ON(!br->br_plink);
				dput(br->br_plink);
				br->br_plink = NULL;
			}
#if 1 // test here
			DiMustNoWaiters(root);
			IiMustNoWaiters(root->d_inode);
			di_write_unlock(root);

			// no need file_list_lock() since sbinfo is locked
			list_for_each_entry(file, &sb->s_files, f_u.fu_list) {
				LKTRTrace("%.*s\n", DLNPair(file->f_dentry));
				// lockdep:
				fi_read_lock(file);
				if (!S_ISREG(file->f_dentry->d_inode->i_mode)
				    || !(file->f_mode & FMODE_WRITE)
				    || fbstart(file) != bindex) {
					fi_read_unlock(file);
					continue;
				}

				// todo: already flushed?
				hf = h_fptr(file);
				hf->f_flags = au_file_roflags(hf->f_flags);
				hf->f_mode &= ~FMODE_WRITE;
				fi_read_unlock(file);
			}

			/* aufs_write_lock() calls ..._child() */
			di_write_lock_child(root);
#endif
		}
	} else if ((mod->perm & MAY_WRITE) && !br->br_wh
		   && hidden_dir->i_op && hidden_dir->i_op->link) {
		struct inode *dir = sb->s_root->d_inode;
		hdir_lock(hidden_dir, dir, bindex);
		br_wh_write_lock(br);
		err = init_wh(mod->h_root, br, do_mnt_nfs(br->br_mnt));
		br_wh_write_unlock(br);
		hdir_unlock(hidden_dir, dir, bindex);
		if (unlikely(err))
			goto out;
	}

	br->br_perm = mod->perm;
	// no need to inc?
	//au_sigen_inc(sb);
	return 0; /* success */

 out:
	TraceErr(err);
	return err;
}
