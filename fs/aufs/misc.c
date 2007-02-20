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

/* $Id: misc.c,v 1.23 2007/02/19 03:28:50 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
//#include <linux/mm.h>
#include <asm/uaccess.h>
#include "aufs.h"

/*
 * @hidden_dir: must be locked.
 * @hidden_dentry: target dentry.
 */
int safe_unlink(struct inode *hidden_dir, struct dentry *hidden_dentry)
{
	int err;
	struct inode *hidden_inode;
	const int stop_sillyrename
		= (SB_NFS(hidden_dentry->d_sb)
		   && atomic_read(&hidden_dentry->d_count) == 1);

	LKTRTrace("%.*s\n", DLNPair(hidden_dentry));
	IMustLock(hidden_dir);

	if (!stop_sillyrename)
		dget(hidden_dentry);
	hidden_inode = hidden_dentry->d_inode;
	if (hidden_inode)
		atomic_inc(&hidden_inode->i_count);
#if 0 // partial testing
	{struct qstr *name = &hidden_dentry->d_name;
	if (name->len == sizeof(AUFS_XINO_FNAME)-1
	    && !strncmp(name->name, AUFS_XINO_FNAME, name->len))
		err = vfs_unlink(hidden_dir, hidden_dentry);
	else err = -1;}
#else
	// vfs_unlink() locks inode
	err = vfs_unlink(hidden_dir, hidden_dentry);
#endif
	if (!stop_sillyrename)
		dput(hidden_dentry);
	if (hidden_inode)
		iput(hidden_inode);

	TraceErr(err);
	return err;
}

int hidden_notify_change(struct dentry *hidden_dentry, struct iattr *ia,
			 struct dentry *dentry)
{
	int err;
	struct inode *hidden_inode;

	LKTRTrace("%.*s, ia_valid 0x%x, dentry %p\n",
		  DLNPair(hidden_dentry), ia->ia_valid, dentry);
	hidden_inode = hidden_dentry->d_inode;
	IMustLock(hidden_inode);

	err = -EPERM;
	if (!IS_IMMUTABLE(hidden_inode) && !IS_APPEND(hidden_inode)) {
		err = notify_change(hidden_dentry, ia);
		//err = -1;
#if 0 // todo: remove this
		if (unlikely(!err && dentry
			     && (ia->ia_valid & ~(ATTR_ATIME | ATTR_ATIME_SET))
			     && IS_MS(dentry->d_sb, MS_UDBA_INOTIFY))) {
			struct dentry *parent = dentry->d_parent;
			//dump_stack();
			// root is handled in aufs_hinotify().
			if (!IS_ROOT(dentry) && S_ISDIR(hidden_inode->i_mode))
				direval_dec(dentry);
			// parent is notified too.
			if (!IS_ROOT(parent))
				direval_dec(parent);
		}
#endif
	}
	TraceErr(err);
	return err;
}

void *kzrealloc(void *p, int nused, int new_sz)
{
	void *q;

	LKTRTrace("p %p, nused %d, sz %d, ksize %d\n",
		  p, nused, new_sz, ksize(p));
	DEBUG_ON(new_sz <= 0);
	if (new_sz <= nused)
		return p;
	if (new_sz <= ksize(p)) {
		memset(p + nused, 0, new_sz - nused);
		return p;
	}

	q = kmalloc(new_sz, GFP_KERNEL);
	//q = NULL;
	if (unlikely(!q))
		return NULL;
	memcpy(q, p, nused);
	memset(q + nused, 0, new_sz - nused);
	//smp_mb();
	kfree(p);
	return q;
}

/* ---------------------------------------------------------------------- */

// todo: if MS_PRIVATE is set, no need to set them.
// todo: make it inline
struct nameidata *fake_dm(struct nameidata *fake_nd, struct nameidata *nd,
			  struct super_block *sb, aufs_bindex_t bindex)
{
	LKTRTrace("nd %p, b%d\n", nd, bindex);

	if (!nd)
		return NULL;

	fake_nd->dentry = NULL;
	fake_nd->mnt = NULL;

#ifndef CONFIG_AUFS_FAKE_DM
	DiMustAnyLock(nd->dentry);

	if (bindex <= dbend(nd->dentry))
		fake_nd->dentry = h_dptr_i(nd->dentry, bindex);
	if (fake_nd->dentry) {
		dget(fake_nd->dentry);
		fake_nd->mnt = sbr_mnt(sb, bindex);
		DEBUG_ON(!fake_nd->mnt);
		mntget(fake_nd->mnt);
	} else
		fake_nd = ERR_PTR(-ENOENT);
#endif

	TraceErrPtr(fake_nd);
	return fake_nd;
}

void fake_dm_release(struct nameidata *fake_nd)
{
#ifndef CONFIG_AUFS_FAKE_DM
	if (fake_nd) {
		mntput(fake_nd->mnt);
		dput(fake_nd->dentry);
	}
#endif
}

/* ---------------------------------------------------------------------- */

int copy_file(struct file *dst, struct file *src, loff_t len)
{
	int err;
	unsigned long blksize;
	char *buf;

	LKTRTrace("%.*s, %.*s\n",
		  DLNPair(dst->f_dentry), DLNPair(src->f_dentry));
	DEBUG_ON(!(dst->f_mode & FMODE_WRITE));
	IMustLock(dst->f_dentry->d_parent->d_inode);

	err = -ENOMEM;
	blksize = dst->f_dentry->d_sb->s_blocksize;
	if (!blksize || PAGE_SIZE < blksize)
		blksize = PAGE_SIZE;
	LKTRTrace("blksize %lu\n", blksize);
	buf = kmalloc(blksize, GFP_KERNEL);
	//buf = NULL;
	if (unlikely(!buf))
		goto out;

	err = 0;
	dst->f_pos = src->f_pos = 0;
	//smp_mb();
	while (len) {
		size_t sz, rbytes, wbytes;
		char *p;
		int all_zero, i;
		mm_segment_t oldfs;

		LKTRTrace("len %lld\n", len);
		sz = blksize;
		if (len < blksize)
			sz = len;

		/* support LSM and notify */
		rbytes = 0;
		while (!rbytes || err == -EAGAIN || err == -EINTR) {
			oldfs = get_fs();
			set_fs(KERNEL_DS);
			err = rbytes = vfs_read(src, (char __user*)buf, sz,
						&src->f_pos);
			set_fs(oldfs);
		}
		if (unlikely(err < 0))
			break;

		/* force write the last block even if it is a hole */
		all_zero = 0;
		if (len > rbytes && rbytes == blksize) {
			all_zero = 1;
			p = buf;
			for (i = 0; all_zero && i < rbytes; i++)
				all_zero = !*p++;
		}
		if (!all_zero) {
			wbytes = rbytes;
			p = buf;
			while (wbytes) {
				size_t b;
				/* support LSM and notify */
				oldfs = get_fs();
				set_fs(KERNEL_DS);
				err = b = vfs_write(dst, (const char __user*)p,
						    wbytes, &dst->f_pos);
				set_fs(oldfs);
				if (unlikely(err == -EAGAIN || err == -EINTR))
					continue;
				if (unlikely(err < 0))
					break;
				wbytes -= b;
				p += b;
			}
		} else {
			loff_t res;
			LKTRLabel(hole);
			err = res = vfs_llseek(dst, rbytes, SEEK_CUR);
			if (unlikely(res < 0))
				break;
		}
		len -= rbytes;
		err = 0;
	}

	kfree(buf);
 out:
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

int test_ro(struct super_block *sb, aufs_bindex_t bindex, struct inode *inode)
{
	int err;

	err = br_rdonly(stobr(sb, bindex));
	if (!err && inode) {
		struct inode *hi = h_iptr_i(inode, bindex);
		if (hi)
			err = IS_IMMUTABLE(hi) ? -EROFS : 0;
	}
	return err;
}

int test_perm(struct inode *hidden_inode, int mask)
{
	if (!current->fsuid)
		return 0;
	if (unlikely(SB_NFS(hidden_inode->i_sb)
		     && (mask & MAY_WRITE)
		     && S_ISDIR(hidden_inode->i_mode)))
		mask |= MAY_READ; /* force permission check */
	return permission(hidden_inode, mask, NULL);
}
