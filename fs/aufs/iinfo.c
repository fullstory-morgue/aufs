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

/* $Id: iinfo.c,v 1.24 2007/01/29 02:32:59 sfjro Exp $ */

//#include <linux/mm.h>
#include "aufs.h"

struct aufs_iinfo *itoii(struct inode *inode)
{
	struct aufs_iinfo *iinfo;

	iinfo = &(container_of(inode, struct aufs_icntnr, vfs_inode)->iinfo);
	/* bad_inode case */
	if (unlikely(!iinfo->ii_hinode))
		return NULL;
	DEBUG_ON(!iinfo->ii_hinode
		 /* || stosi(inode->i_sb)->si_bend < iinfo->ii_bend */
		 || iinfo->ii_bend < iinfo->ii_bstart);
	return iinfo;
}

aufs_bindex_t ibstart(struct inode *inode)
{
	IiMustAnyLock(inode);
	return itoii(inode)->ii_bstart;
}

aufs_bindex_t ibend(struct inode *inode)
{
	IiMustAnyLock(inode);
	return itoii(inode)->ii_bend;
}

struct aufs_vdir *ivdir(struct inode *inode)
{
	IiMustAnyLock(inode);
	DEBUG_ON(!S_ISDIR(inode->i_mode));
	return itoii(inode)->ii_vdir;
}

struct inode *h_iptr_i(struct inode *inode, aufs_bindex_t bindex)
{
	struct inode *hidden_inode;

	IiMustAnyLock(inode);
	DEBUG_ON(bindex < 0 || ibend(inode) < bindex);
	hidden_inode = itoii(inode)->ii_hinode[0 + bindex].hi_inode;
	DEBUG_ON(hidden_inode && atomic_read(&hidden_inode->i_count) <= 0);
	return hidden_inode;
}

struct inode *h_iptr(struct inode *inode)
{
	return h_iptr_i(inode, ibstart(inode));
}

unsigned int itoid_index(struct inode *inode, aufs_bindex_t bindex)
{
	IiMustAnyLock(inode);
	DEBUG_ON(bindex < 0
		 || ibend(inode) < bindex
		 || !itoii(inode)->ii_hinode[0 + bindex].hi_inode);
	return itoii(inode)->ii_hinode[0 + bindex].hi_id;
}

// hard/soft set
void set_ibstart(struct inode *inode, aufs_bindex_t bindex)
{
	IiMustWriteLock(inode);
	DEBUG_ON(sbend(inode->i_sb) < bindex);
	itoii(inode)->ii_bstart = bindex;
	//smp_mb();
}

void set_ibend(struct inode *inode, aufs_bindex_t bindex)
{
	IiMustWriteLock(inode);
	DEBUG_ON(sbend(inode->i_sb) < bindex
		 || bindex < ibstart(inode));
	itoii(inode)->ii_bend = bindex;
	//smp_mb();
}

void set_ivdir(struct inode *inode, struct aufs_vdir *vdir)
{
	IiMustWriteLock(inode);
	DEBUG_ON(!S_ISDIR(inode->i_mode)
		 || (itoii(inode)->ii_vdir && vdir));
	itoii(inode)->ii_vdir = vdir;
	//smp_mb();
}

void aufs_hiput(struct aufs_hinode *hinode)
{
	if (unlikely(hinode->hi_notify))
		do_free_hinotify(hinode);
	if (hinode->hi_inode)
		iput(hinode->hi_inode);
}

unsigned int hi_flags(struct inode *inode, int isdir)
{
	unsigned int flags;
	struct super_block *sb = inode->i_sb;

	flags = 0;
	if (IS_MS(sb, MS_XINO))
		flags = AUFS_HI_XINO;
	if (unlikely(isdir && IS_MS(sb, MS_UDBA_INOTIFY)))
		flags |= AUFS_HI_NOTIFY;
	return flags;
}

void set_h_iptr(struct inode *inode, aufs_bindex_t bindex,
		struct inode *val, unsigned int flags)
{
	struct aufs_hinode *hinode;
	struct inode *hi;

	LKTRTrace("i%lu, b%d, hi%lu, flags 0x%x\n",
		  inode->i_ino, bindex, val ? val->i_ino : 0, flags);

	hinode = itoii(inode)->ii_hinode + bindex;
	hi = hinode->hi_inode;
	IiMustWriteLock(inode);
	DEBUG_ON(bindex < ibstart(inode) || ibend(inode) < bindex
		 || (val && atomic_read(&val->i_count) <= 0)
		 || (val && hi));
	if (!val && hi)
		aufs_hiput(hinode);
	hinode->hi_inode = val;
	if (val) {
		int err;
		struct super_block *sb = inode->i_sb;

		hinode->hi_id = stobr(sb, bindex)->br_id;
		if (flags & AUFS_HI_XINO) {
			err = xino_write(sb, bindex, val->i_ino, inode->i_ino);
			if (unlikely(err)) {
				IOErr1("failed xino_write() %d, force noxino\n",
				       err);
				MS_CLR(sb, MS_XINO);
			}
		}
		if (flags & AUFS_HI_NOTIFY) {
			err = alloc_hinotify(hinode, inode, val);
			if (unlikely(err))
				IOErr1("alloc_hinotify() %d\n", err);
			else
				DEBUG_ON(!hinode->hi_notify);
		}
	}
	//smp_mb();
}

/* it may be called at remount time, too */
void update_brange(struct inode *inode, int do_put_zero)
{
	struct aufs_iinfo *iinfo;

	LKTRTrace("i%lu, %d\n", inode->i_ino, do_put_zero);
	IiMustWriteLock(inode);

	iinfo = itoii(inode);
	if (unlikely(!iinfo) || iinfo->ii_bstart < 0)
		return;

	if (do_put_zero) {
		aufs_bindex_t bindex;
		for (bindex = iinfo->ii_bstart; bindex <= iinfo->ii_bend;
		     bindex++) {
			struct inode *h_i;
			h_i = iinfo->ii_hinode[0 + bindex].hi_inode;
			if (h_i && !h_i->i_nlink)
				set_h_iptr(inode, bindex, NULL, 0);
		}
	}

	iinfo->ii_bstart = -1;
	while (++iinfo->ii_bstart <= iinfo->ii_bend)
		if (iinfo->ii_hinode[0 + iinfo->ii_bstart].hi_inode)
			break;
	if (iinfo->ii_bstart > iinfo->ii_bend) {
		iinfo->ii_bend = iinfo->ii_bstart = -1;
		return;
	}

	iinfo->ii_bend++;
	while (0 <= --iinfo->ii_bend)
		if (iinfo->ii_hinode[0 + iinfo->ii_bend].hi_inode)
			break;
}

/* ---------------------------------------------------------------------- */

int iinfo_init(struct inode *inode)
{
	struct aufs_iinfo *iinfo;
	struct super_block *sb;
	int nbr;

	sb = inode->i_sb;
	DEBUG_ON(!sb);
	iinfo = &(container_of(inode, struct aufs_icntnr, vfs_inode)->iinfo);
	nbr = sbend(sb) + 1;
	if (unlikely(!nbr))
		nbr++;
	iinfo->ii_hinode = kcalloc(nbr, sizeof(*iinfo->ii_hinode), GFP_KERNEL);
	//iinfo->ii_hinode = NULL;
	if (iinfo->ii_hinode) {
		rw_init_nolock(&iinfo->ii_rwsem);
		iinfo->ii_bstart = -1;
		iinfo->ii_bend = -1;
		iinfo->ii_vdir = NULL;
		return 0;
	}
	return -ENOMEM;
}

void iinfo_fin(struct inode *inode)
{
	struct aufs_iinfo *iinfo;

	iinfo = itoii(inode);
	/* bad_inode case */
	if (unlikely(!iinfo))
		return;

	if (unlikely(iinfo->ii_vdir))
		free_vdir(iinfo->ii_vdir);

	if (iinfo->ii_bstart >= 0) {
		aufs_bindex_t bend;
		struct aufs_hinode *hi;
		hi = iinfo->ii_hinode + iinfo->ii_bstart;
		bend = iinfo->ii_bend;
		while (iinfo->ii_bstart++ <= bend) {
			if (hi->hi_inode)
				aufs_hiput(hi);
			hi++;
		}
		//iinfo->ii_bstart = iinfo->ii_bend = -1;
	}

	rw_destroy(&iinfo->ii_rwsem);
	kfree(iinfo->ii_hinode);
	//iinfo->ii_hinode = NULL;
}
