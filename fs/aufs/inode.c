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

/* $Id: inode.c,v 1.20 2007/04/23 00:59:51 sfjro Exp $ */

#include "aufs.h"

int au_refresh_hinode(struct dentry *dentry)
{
	int err, new_sz, update, isdir;
	struct inode *inode, *first;
	struct aufs_hinode *p;
	struct super_block *sb;
	struct aufs_iinfo *iinfo;
	aufs_bindex_t bindex, bend;
	unsigned int flags;

	LKTRTrace("%.*s\n", DLNPair(dentry));
	inode = dentry->d_inode;
	DEBUG_ON(!inode);
	IiMustWriteLock(inode);

	err = -ENOMEM;
	sb = dentry->d_sb;
	new_sz = sizeof(*iinfo->ii_hinode) * (sbend(sb) + 1);
	iinfo = itoii(inode);
	p = au_kzrealloc(iinfo->ii_hinode, sizeof(*p) * (iinfo->ii_bend + 1),
			 new_sz, GFP_KERNEL);
	//p = NULL;
	if (unlikely(!p))
		goto out;
	iinfo->ii_hinode = p;
	err = 0;
	update = 0;
	p = iinfo->ii_hinode + iinfo->ii_bstart;
	first = p->hi_inode;
	for (bindex = iinfo->ii_bstart; bindex <= iinfo->ii_bend;
	     bindex++, p++) {
		struct aufs_hinode *q, tmp;
		aufs_bindex_t new_bindex;

		if (unlikely(!p->hi_inode))
			continue;

		new_bindex = find_brindex(sb, p->hi_id);
		if (new_bindex == bindex)
			continue;
		if (new_bindex < 0) {
			update++;
			aufs_hiput(p);
			p->hi_inode = NULL;
			continue;
		}

		if (new_bindex < iinfo->ii_bstart)
			iinfo->ii_bstart = new_bindex;
		if (iinfo->ii_bend < new_bindex)
			iinfo->ii_bend = new_bindex;
		/* swap two hidden inode, and loop again */
		q = iinfo->ii_hinode + new_bindex;
		tmp = *q;
		*q = *p;
		*p = tmp;
		if (tmp.hi_inode) {
			bindex--;
			p--;
		}
	}

	isdir = S_ISDIR(inode->i_mode);
	flags = au_hi_flags(inode, isdir);
	bend = dbend(dentry);
	for (bindex = dbstart(dentry); bindex <= bend; bindex++) {
		struct inode *hi;
		struct dentry *hd;

		hd = au_h_dptr_i(dentry, bindex);
		if (!hd || !hd->d_inode)
			continue;

		if (iinfo->ii_bstart <= bindex && bindex <= iinfo->ii_bend) {
			hi = au_h_iptr_i(inode, bindex);
			if (hi) {
				DEBUG_ON(hi != hd->d_inode);
				continue;
			}
		}
		if (bindex < iinfo->ii_bstart)
			iinfo->ii_bstart = bindex;
		if (iinfo->ii_bend < bindex)
			iinfo->ii_bend = bindex;
		set_h_iptr(inode, bindex, igrab(hd->d_inode), flags);
		update++;
	}

	bend = iinfo->ii_bend;
	p = iinfo->ii_hinode;
	for (bindex = 0; bindex <= bend; bindex++, p++)
		if (p->hi_inode) {
			iinfo->ii_bstart = bindex;
			break;
		}
	p = iinfo->ii_hinode + bend;
	//for (bindex = bend; bindex > iinfo->ii_bstart; bindex--, p--)
	for (bindex = bend; bindex >= 0; bindex--, p--)
		if (p->hi_inode) {
			iinfo->ii_bend = bindex;
			break;
		}

	if (first != au_h_iptr(inode))
		au_cpup_attr_all(inode);
	if (update && isdir)
		inode->i_version++;

 out:
	TraceErr(err);
	return err;
}

static int set_inode(struct inode *inode, struct dentry *dentry)
{
	int err, isdir;
	struct dentry *hidden_dentry;
	struct inode *hidden_inode;
	umode_t mode;
	aufs_bindex_t bindex, bstart, btail;
	struct aufs_iinfo *iinfo;
	unsigned int flags;

	LKTRTrace("i%lu, %.*s\n", inode->i_ino, DLNPair(dentry));
	DEBUG_ON(!(inode->i_state & I_NEW));
	IiMustWriteLock(inode);
	hidden_dentry = au_h_dptr(dentry);
	DEBUG_ON(!hidden_dentry);
	hidden_inode = hidden_dentry->d_inode;
	DEBUG_ON(!hidden_inode);

	err = 0;
	isdir = 0;
	bstart = dbstart(dentry);
	mode = hidden_inode->i_mode;
	switch (mode & S_IFMT) {
	case S_IFREG:
		btail = dbtail(dentry);
		break;
	case S_IFDIR:
		isdir = 1;
		btail = dbtaildir(dentry);
		inode->i_op = &aufs_dir_iop;
		inode->i_fop = &aufs_dir_fop;
		break;
	case S_IFLNK:
		btail = dbtail(dentry);
		inode->i_op = &aufs_symlink_iop;
		break;
	case S_IFBLK:
	case S_IFCHR:
	case S_IFIFO:
	case S_IFSOCK:
		btail = dbtail(dentry);
		init_special_inode(inode, mode, hidden_inode->i_rdev);
		break;
	default:
		IOErr("Unknown file type 0%o\n", mode);
		err = -EIO;
		goto out;
	}

	flags = au_hi_flags(inode, isdir);
	iinfo = itoii(inode);
	iinfo->ii_bstart = bstart;
	iinfo->ii_bend = btail;
	for (bindex = bstart; bindex <= btail; bindex++) {
		hidden_dentry = au_h_dptr_i(dentry, bindex);
		if (!hidden_dentry)
			continue;
		DEBUG_ON(!hidden_dentry->d_inode);
		set_h_iptr(inode, bindex, igrab(hidden_dentry->d_inode), flags);
	}
	au_cpup_attr_all(inode);

 out:
	TraceErr(err);
	return err;
}

/* successful returns with iinfo write_locked */
struct inode *au_new_inode(struct dentry *dentry)
{
	struct inode *inode, *hidden_inode;
	struct dentry *hidden_dentry;
	ino_t hidden_ino, ino;
	struct super_block *sb;
	int err;
	aufs_bindex_t bindex, bstart;

	LKTRTrace("%.*s\n", DLNPair(dentry));
	sb = dentry->d_sb;
	hidden_dentry = au_h_dptr(dentry);
	DEBUG_ON(!hidden_dentry);
	hidden_inode = hidden_dentry->d_inode;
	DEBUG_ON(!hidden_inode);

	bstart = dbstart(dentry);
	hidden_ino = hidden_inode->i_ino;
 new_ino:
	err = xino_read(sb, bstart, hidden_ino, &ino, 1);
	//err = -1;
	inode = ERR_PTR(err);
	if (unlikely(err))
		goto out;

	LKTRTrace("i%lu\n", ino);
	err = -ENOMEM;
	//inode = iget(sb, ino);
	inode = iget_locked(sb, ino);
	if (unlikely(!inode))
		goto out;
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out;
	err = -ENOMEM;
	if (unlikely(is_bad_inode(inode)))
		goto out_iput;
	LKTRTrace("%lx, new %d\n", inode->i_state, !!(inode->i_state & I_NEW));
	if (inode->i_state & I_NEW)
		sb->s_op->read_inode(inode);

	err = 0;
	ii_write_lock_new(inode);
	bindex = ibstart(inode);
	if (inode->i_state & I_NEW) {
		err = set_inode(inode, dentry);
		//err = -1;
		unlock_new_inode(inode);
#if 1 // test
	} else if (unlikely(bindex >= 0
			    /* && au_flag_test(sb, AuFlag_UDBA_INOTIFY) */)) {
		int found = 0;
		aufs_bindex_t bend = ibend(inode);

		for (; !found && bindex <= bend; bindex++)
			found = (hidden_inode == au_h_iptr_i(inode, bindex));

		if (unlikely(!found)) {
			ii_write_unlock(inode);
			//todo: bug?
			iput(inode);
			err = xino_write(sb, bstart, hidden_ino, /*ino*/0);
			if (unlikely(err))
				goto out_iput;
			//iput(inode);
			goto new_ino;
		}

		if (S_ISDIR(inode->i_mode)) {
			unsigned int flags = au_hi_flags(inode, /*isdir*/1);

			bindex = dbstart(dentry);
			bend = dbend(dentry);
			for (; bindex <= bend; bindex++) {
				hidden_dentry = au_h_dptr_i(dentry, bindex);
				if (hidden_dentry && hidden_dentry->d_inode)
					break;
			}
			if (bindex < ibstart(inode))
				set_ibstart(inode, bindex);
			for (; bend >= bindex; bend--) {
				hidden_dentry = au_h_dptr_i(dentry, bend);
				if (hidden_dentry && hidden_dentry->d_inode)
					break;
			}
			if (ibend(inode) < bend)
				set_ibend(inode, bend);

			for (; bindex <= bend; bindex++) {
				hidden_dentry = au_h_dptr_i(dentry, bindex);
				if (hidden_dentry
				    && hidden_dentry->d_inode
				    && !au_h_iptr_i(inode, bindex))
				set_h_iptr(inode, bindex,
					   igrab(hidden_dentry->d_inode), flags);
			}
		}
#endif
	}
	if (!err)
		return inode; /* success */

	/* error */
	ii_write_unlock(inode);

 out_iput:
	iput(inode);
	inode = ERR_PTR(err);
 out:
	TraceErrPtr(inode);
	return inode;
}
