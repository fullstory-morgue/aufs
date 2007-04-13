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

/* $Id: sbinfo.c,v 1.27 2007/04/02 01:12:31 sfjro Exp $ */

#include "aufs.h"

struct aufs_sbinfo *stosi(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;
	sbinfo = sb->s_fs_info;
	//DEBUG_ON(sbinfo->si_bend < 0);
	return sbinfo;
}

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

int au_sigen(struct super_block *sb)
{
	SiMustAnyLock(sb);
	return stosi(sb)->si_generation;
}

int au_sigen_inc(struct super_block *sb)
{
	int gen;

	SiMustWriteLock(sb);
	gen = ++stosi(sb)->si_generation;
	au_update_digen(sb->s_root);
	sb->s_root->d_inode->i_version++;
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
		di_write_lock_child(dentry);
	else
		di_read_lock_child(dentry, flags);
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
	di_write_lock_child(dentry);
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
	di_write_lock2_child(d1, d2, isdir);
}

void aufs_read_and_write_unlock2(struct dentry *d1, struct dentry *d2)
{
	DEBUG_ON(d1 == d2 || d1->d_sb != d2->d_sb);
	di_write_unlock2(d1, d2);
	si_read_unlock(d1->d_sb);
}

/* ---------------------------------------------------------------------- */

aufs_bindex_t new_br_id(struct super_block *sb)
{
	aufs_bindex_t br_id;

	TraceEnter();
	SiMustWriteLock(sb);

	while (1) {
		br_id = ++stosi(sb)->si_last_br_id;
		if (br_id && find_brindex(sb, br_id) < 0)
			return br_id;
	}
}
