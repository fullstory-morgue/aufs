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

/* $Id: sbinfo.c,v 1.22 2007/02/19 03:29:20 sfjro Exp $ */

#include "aufs.h"

struct aufs_sbinfo *stosi(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;
	sbinfo = sb->s_fs_info;
	//DEBUG_ON(sbinfo->si_bend < 0);
	return sbinfo;
}

#if 0 // debug
void MS_SET(struct super_block *sb, unsigned int flg)
{
	SiMustWriteLock(sb);
	stosi(sb)->si_flags |= flg;
}

void MS_CLR(struct super_block *sb, unsigned int flg)
{
	SiMustWriteLock(sb);
	stosi(sb)->si_flags &= ~flg;
}

unsigned int IS_MS(struct super_block *sb, unsigned int flg)
{
	SiMustAnyLock(sb);
	return (stosi(sb)->si_flags & flg);
}
#endif

aufs_bindex_t sbend(struct super_block *sb)
{
	SiMustAnyLock(sb);
	return stosi(sb)->si_bend;
}

struct aufs_branch *stobr(struct super_block *sb, aufs_bindex_t bindex)
{
	SiMustAnyLock(sb);
	DEBUG_ON(bindex < 0 || sbend(sb) < bindex
		 || !stosi(sb)->si_branch[0 + bindex]);
	return stosi(sb)->si_branch[0 + bindex];
}

int sigen(struct super_block *sb)
{
	SiMustAnyLock(sb);
	return stosi(sb)->si_generation;
}

int sigen_inc(struct super_block *sb)
{
	int gen;

	SiMustWriteLock(sb);
	gen = ++stosi(sb)->si_generation;
	update_digen(sb->s_root);
	sb->s_root->d_inode->i_version++;
	//smp_mb();
	return gen;
}

int find_bindex(struct super_block *sb, struct aufs_branch *br)
{
	aufs_bindex_t bindex, bend;

	bend = sbend(sb);
	for (bindex = 0; bindex <= bend; bindex++)
		if (stobr(sb, bindex) == br)
			return bindex;
	return -1;
}

/* ---------------------------------------------------------------------- */

/* dentry and super_block lock. call at entry point */
void aufs_read_lock(struct dentry *dentry, int flags)
{
	si_read_lock(dentry->d_sb);
	if (flags & AUFS_D_WLOCK)
		di_write_lock(dentry);
	else
		di_read_lock(dentry, flags);
}

void aufs_read_unlock(struct dentry *dentry, int flags)
{
	if (flags & AUFS_D_WLOCK)
		di_write_unlock(dentry);
	else
		di_read_unlock(dentry, flags);
	si_read_unlock(dentry->d_sb);
}

void aufs_write_lock(struct dentry *dentry)
{
	si_write_lock(dentry->d_sb);
	di_write_lock(dentry);
}

void aufs_write_unlock(struct dentry *dentry)
{
	di_write_unlock(dentry);
	si_write_unlock(dentry->d_sb);
}

void aufs_read_and_write_lock2(struct dentry *d1, struct dentry *d2, int isdir)
{
	DEBUG_ON(d1 == d2 || d1->d_sb != d2->d_sb);
	si_read_lock(d1->d_sb);
	di_write_lock2(d1, d2, isdir);
}

void aufs_read_and_write_unlock2(struct dentry *d1, struct dentry *d2)
{
	DEBUG_ON(d1 == d2 || d1->d_sb != d2->d_sb);
	di_write_unlock2(d1, d2);
	si_read_unlock(d1->d_sb);
}

/* ---------------------------------------------------------------------- */

struct aufs_pseudo_link {
	struct list_head list;
	struct inode *inode;
};

#ifdef CONFIG_AUFS_DEBUG
void list_plink(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;
	struct list_head *plink_list;
	struct aufs_pseudo_link *plink;

	TraceEnter();
	SiMustAnyLock(sb);
	DEBUG_ON(!IS_MS(sb, MS_PLINK));

	sbinfo = stosi(sb);
	plink_list = &sbinfo->si_plink;
	spin_lock(&sbinfo->si_plink_lock);
	list_for_each_entry(plink, plink_list, list)
		Dbg("%lu\n", plink->inode->i_ino);
	spin_unlock(&sbinfo->si_plink_lock);
}
#endif

int is_plinked(struct super_block *sb, struct inode *inode)
{
	int found;
	struct aufs_sbinfo *sbinfo;
	struct list_head *plink_list;
	struct aufs_pseudo_link *plink;

	LKTRTrace("i%lu\n", inode->i_ino);
	SiMustAnyLock(sb);
	DEBUG_ON(!IS_MS(sb, MS_PLINK));

	found = 0;
	sbinfo = stosi(sb);
	plink_list = &sbinfo->si_plink;
	spin_lock(&sbinfo->si_plink_lock);
	list_for_each_entry(plink, plink_list, list)
		if (plink->inode == inode) {
			found = 1;
			break;
		}
	spin_unlock(&sbinfo->si_plink_lock);
	return found;
}

static int do_whplink(char *tgt, int len, struct dentry *h_parent,
		      struct dentry *h_dentry, struct vfsmount *h_mnt,
		      struct super_block *sb)
{
	int err;
	struct dentry *h_tgt;
	struct inode *h_dir;

	h_tgt = lkup_one(tgt, h_parent, len, h_mnt);
	err = PTR_ERR(h_tgt);
	if (IS_ERR(h_tgt))
		goto out;

	err = 0;
	h_dir = h_parent->d_inode;
	if (unlikely(h_tgt->d_inode && h_tgt->d_inode != h_dentry->d_inode))
		err = hipriv_unlink(h_dir, h_tgt, sb);
	if (!err && !h_tgt->d_inode) {
		err = hipriv_link(h_dentry, h_dir, h_tgt, sb);
		//inode->i_nlink++;
	}
	dput(h_tgt);

 out:
	TraceErr(err);
	return err;
}

struct do_whplink_args {
	int *errp;
	char *tgt;
	int len;
	struct dentry *h_parent;
	struct dentry *h_dentry;
	struct vfsmount *h_mnt;
	struct super_block *sb;
};

static void call_do_whplink(void *args)
{
	struct do_whplink_args *a = args;
	*a->errp = do_whplink(a->tgt, a->len, a->h_parent, a->h_dentry,
			      a->h_mnt, a->sb);
}

static int whplink(struct dentry *h_dentry, struct inode *inode,
		   aufs_bindex_t bindex, struct super_block *sb)
{
	int err, len;
	struct aufs_branch *br;
	struct dentry *h_parent;
	struct inode *h_dir;
	// 20 is max digits length of ulong 64
	char tgtname[(20 + 1) * 2];
	struct aufs_h_ipriv ipriv;

	LKTRTrace("%.*s\n", DLNPair(h_dentry));
	br = stobr(inode->i_sb, bindex);
	h_parent = br->br_plink;
	DEBUG_ON(!h_parent);
	h_dir = h_parent->d_inode;
	DEBUG_ON(!h_dir);

	len = snprintf(tgtname, sizeof(tgtname), "%lu.%lu",
		       inode->i_ino, h_dentry->d_inode->i_ino);
	DEBUG_ON(len >= sizeof(tgtname));

	// always superio.
	i_lock_priv(h_dir, &ipriv, sb);
	if (!is_kthread(current)) {
		struct do_whplink_args args = {
			.errp		= &err,
			.tgt		= tgtname,
			.len		= len,
			.h_parent	= h_parent,
			.h_dentry	= h_dentry,
			.h_mnt		= br->br_mnt,
			.sb		= sb
		};
		wkq_wait(call_do_whplink, &args);
	} else
		err = do_whplink(tgtname, len, h_parent, h_dentry, br->br_mnt,
				 sb);
	i_unlock_priv(h_dir, &ipriv);

	TraceErr(err);
	return err;
}

void append_plink(struct super_block *sb, struct inode *inode,
		  struct dentry *h_dentry, aufs_bindex_t bindex)
{
	struct aufs_sbinfo *sbinfo;
	struct list_head *plink_list;
	struct aufs_pseudo_link *plink;
	int found, err, cnt;

	LKTRTrace("i%lu\n", inode->i_ino);
	SiMustAnyLock(sb);
	DEBUG_ON(!IS_MS(sb, MS_PLINK));

	cnt = 0;
	found = 0;
	sbinfo = stosi(sb);
	plink_list = &sbinfo->si_plink;
	spin_lock(&sbinfo->si_plink_lock);
	list_for_each_entry(plink, plink_list, list) {
		cnt++;
		if (plink->inode == inode) {
			found = 1;
			break;
		}
	}

	err = 0;
	if (!found) {
		struct aufs_pseudo_link *plink;

		plink = kmalloc(sizeof(*plink), GFP_ATOMIC);
		if (plink) {
			plink->inode = igrab(inode);
			list_add(&plink->list, plink_list);
			cnt++;
		} else
			err = -ENOMEM;
	}
	spin_unlock(&sbinfo->si_plink_lock);

	if (!err)
		err = whplink(h_dentry, inode, bindex, sb);

	if (unlikely(cnt > 100))
		Warn1("unexpectedly many pseudo links, %d\n", cnt);
	if (unlikely(err))
		Warn("err %d, damaged pseudo link. ignored.\n", err);
}

static void do_put_plink(struct aufs_pseudo_link *plink, int do_del)
{
	TraceEnter();

	iput(plink->inode);
	if (do_del)
		list_del(&plink->list);
	kfree(plink);
}

void put_plink(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;
	struct list_head *plink_list;
	struct aufs_pseudo_link *plink, *tmp;

	TraceEnter();
	SiMustWriteLock(sb);
	DEBUG_ON(!IS_MS(sb, MS_PLINK));

	sbinfo = stosi(sb);
	plink_list = &sbinfo->si_plink;
	//spin_lock(&sbinfo->si_plink_lock);
	list_for_each_entry_safe(plink, tmp, plink_list, list)
		do_put_plink(plink, 0);
	INIT_LIST_HEAD(plink_list);
	//spin_unlock(&sbinfo->si_plink_lock);
}

void half_refresh_plink(struct super_block *sb, unsigned int id)
{
	struct aufs_sbinfo *sbinfo;
	struct list_head *plink_list;
	struct aufs_pseudo_link *plink, *tmp;
	struct inode *inode;
	aufs_bindex_t bstart, bend, bindex;
	int do_put;

	TraceEnter();
	SiMustWriteLock(sb);
	DEBUG_ON(!IS_MS(sb, MS_PLINK));

	sbinfo = stosi(sb);
	plink_list = &sbinfo->si_plink;
	//spin_lock(&sbinfo->si_plink_lock);
	list_for_each_entry_safe(plink, tmp, plink_list, list) {
		do_put = 0;
		inode = igrab(plink->inode);
		ii_write_lock(inode);
		bstart = ibstart(inode);
		bend = ibend(inode);
		if (bstart >= 0) {
			for (bindex = bstart; bindex <= bend; bindex++) {
				if (!h_iptr_i(inode, bindex)
				    || itoid_index(inode, bindex) != id)
					continue;
				set_h_iptr(inode, bindex, NULL, 0);
				do_put = 1;
				break;
			}
		} else
			do_put_plink(plink, 1);

		if (do_put) {
			for (bindex = bstart; bindex <= bend; bindex++)
				if (h_iptr_i(inode, bindex)) {
					do_put = 0;
					break;
				}
			if (do_put)
				do_put_plink(plink, 1);
		}
		ii_write_unlock(inode);
		iput(inode);
	}
	//spin_unlock(&sbinfo->si_plink_lock);
}

/* ---------------------------------------------------------------------- */

unsigned int new_br_id(struct super_block *sb)
{
	unsigned int id;

	TraceEnter();
	SiMustWriteLock(sb);

	while (1) {
		id = ++stosi(sb)->si_last_br_id;
		if (id && find_brindex(sb, id) < 0)
			return id;
	}
}
