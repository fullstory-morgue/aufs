/*
 * Copyright (C) 2007 Junjiro Okajima
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

/* $Id: vfsub.h,v 1.3 2007/03/19 04:33:28 sfjro Exp $ */

#ifndef __AUFS_VFSUB_H__
#define __AUFS_VFSUB_H__

#include <linux/fs.h>
#include <asm/uaccess.h>

#ifdef __KERNEL__

/* ---------------------------------------------------------------------- */

static inline
int do_vfsub_permission(struct inode *inode, int mask, struct nameidata *nd)
{
	return permission(inode, mask, nd);
}

static inline
struct file *vfsub_filp_open(const char *path, int oflags, int mode)
{
	struct file *err;
	lockdep_off();
	err = filp_open(path, oflags, mode);
	lockdep_on();
	return err;
}

static inline
int vfsub_path_lookup(const char *name, unsigned int flags,
		      struct nameidata *nd)
{
	int err;
	//lockdep_off();
	err = path_lookup(name, flags, nd);
	//lockdep_on();
	return err;
}

/* ---------------------------------------------------------------------- */

static inline
int do_vfsub_create(struct inode *dir, struct dentry *dentry, int mode,
		    struct nameidata *nd)
{
	return vfs_create(dir, dentry, mode, nd);
}

static inline
int do_vfsub_symlink(struct inode *dir, struct dentry *dentry,
		     const char *symname, int mode)
{
	return vfs_symlink(dir, dentry, symname, mode);
}

static inline
int do_vfsub_mknod(struct inode *dir, struct dentry *dentry, int mode,
		   dev_t dev)
{
	return vfs_mknod(dir, dentry, mode, dev);
}

static inline
int do_vfsub_link(struct dentry *src_dentry, struct inode *dir,
		  struct dentry *dentry)
{
	int err;
	lockdep_off();
	err = vfs_link(src_dentry, dir, dentry);
	lockdep_on();
	return err;
}

static inline
int do_vfsub_rename(struct inode *src_dir, struct dentry *src_dentry,
		    struct inode *dir, struct dentry *dentry)
{
	int err;
	lockdep_off();
	err = vfs_rename(src_dir, src_dentry, dir, dentry);
	lockdep_on();
	return err;
}

static inline
int do_vfsub_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	return vfs_mkdir(dir, dentry, mode);
}

static inline
int do_vfsub_rmdir(struct inode *dir, struct dentry *dentry)
{
	int err;
	lockdep_off();
	err = vfs_rmdir(dir, dentry);
	lockdep_on();
	return err;
}

/* ---------------------------------------------------------------------- */

static inline
int do_vfsub_notify_change(struct dentry *dentry, struct iattr *ia)
{
	int err;
	lockdep_off();
	err = notify_change(dentry, ia);
	lockdep_on();
	return err;
}

/* ---------------------------------------------------------------------- */

static inline
ssize_t do_vfsub_read_u(struct file *file, char __user *ubuf, size_t count,
			loff_t *ppos)
{
	ssize_t err;
	//lockdep_off();
	err = vfs_read(file, ubuf, count, ppos);
	//lockdep_on();
	return err;
}

static inline
ssize_t do_vfsub_read_k(struct file *file, void *kbuf, size_t count,
			loff_t *ppos)
{
	ssize_t err;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	err = do_vfsub_read_u(file, (char __user*)kbuf, count, ppos);
	set_fs(oldfs);
	return err;
}

static inline
ssize_t do_vfsub_write_u(struct file *file, const char __user *ubuf,
			 size_t count, loff_t *ppos)
{
	ssize_t err;
	lockdep_off();
	err = vfs_write(file, ubuf, count, ppos);
	lockdep_on();
	return err;
}

static inline
ssize_t do_vfsub_write_k(struct file *file, void *kbuf, size_t count,
			 loff_t *ppos)
{
	ssize_t err;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	err = do_vfsub_write_u(file, (const char __user*)kbuf, count, ppos);
	set_fs(oldfs);
	return err;
}

static inline
int do_vfsub_readdir(struct file *file, filldir_t filldir, void *arg)
{
	int err;
	lockdep_off();
	err = vfs_readdir(file, filldir, arg);
	lockdep_on();
	return err;
}

/* ---------------------------------------------------------------------- */

static inline
loff_t vfsub_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t err;
	lockdep_off();
	err = vfs_llseek(file, offset, origin);
	lockdep_on();
	return err;
}

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_DLGT
#define need_dlgt(sb)	(IS_MS(sb, MS_DLGT) && !is_kthread(current))

int vfsub_permission(struct inode *inode, int mask, struct nameidata *nd,
		     int dlgt);

int vfsub_create(struct inode *dir, struct dentry *dentry, int mode,
		 struct nameidata *nd, int dlgt);
int vfsub_symlink(struct inode *dir, struct dentry *dentry, const char *symname,
		  int mode, int dlgt);
int vfsub_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev,
		int dlgt);
int vfsub_link(struct dentry *src_dentry, struct inode *dir,
	       struct dentry *dentry, int dlgt);
int vfsub_rename(struct inode *src_dir, struct dentry *src_dentry,
		 struct inode *dir, struct dentry *dentry, int dlgt);
int vfsub_mkdir(struct inode *dir, struct dentry *dentry, int mode,
		int dlgt);
int vfsub_rmdir(struct inode *dir, struct dentry *dentry, int dlgt);

int vfsub_notify_change(struct dentry *dentry, struct iattr *ia, int dlgt);

ssize_t vfsub_read_u(struct file *file, char __user *ubuf, size_t count,
		     loff_t *ppos, int dlgt);
ssize_t vfsub_read_k(struct file *file, void *kbuf, size_t count, loff_t *ppos,
		     int dlgt);
ssize_t vfsub_write_u(struct file *file, const char __user *ubuf, size_t count,
		      loff_t *ppos, int dlgt);
ssize_t vfsub_write_k(struct file *file, void *kbuf, size_t count, loff_t *ppos,
		      int dlgt);
int vfsub_readdir(struct file *file, filldir_t filldir, void *arg, int dlgt);

#else

#define need_dlgt(sb)	0

#define vfsub_permission(inode, mask, nd, dlgt) \
	do_vfsub_permission(inode, mask, nd)

#define vfsub_create(dir, dentry, mode, nd, dlgt) \
	do_vfsub_create(dir, dentry, mode, nd)
#define vfsub_symlink(dir, dentry, symname, mode, dlgt) \
	do_vfsub_symlink(dir, dentry, symname, mode)
#define vfsub_mknod(dir, dentry, mode, dev, dlgt) \
	do_vfsub_mknod(dir, dentry, mode, dev)
#define vfsub_mkdir(dir, dentry, mode, dlgt) \
	do_vfsub_mkdir(dir, dentry, mode)
#define vfsub_link(src_dentry, dir, dentry, dlgt) \
	do_vfsub_link(src_dentry, dir, dentry)
#define vfsub_rename(src_dir, src_dentry, dir, dentry, dlgt) \
	do_vfsub_rename(src_dir, src_dentry, dir, dentry)
#define vfsub_rmdir(dir, dentry, dlgt) \
	do_vfsub_rmdir(dir, dentry)

#define vfsub_notify_change(dentry, ia, dlgt) \
	do_vfsub_notify_change(dentry, ia)

#define vfsub_read_u(file, ubuf, count, ppos, dlgt) \
	do_vfsub_read_u(file, ubuf, count, ppos)
#define vfsub_read_k(file, kbuf, count, ppos, dlgt) \
	do_vfsub_read_k(file, kbuf, count, ppos)
#define vfsub_write_u(file, ubuf, count, ppos, dlgt) \
	do_vfsub_write_u(file, ubuf, count, ppos)
#define vfsub_write_k(file, kbuf, count, ppos, dlgt) \
	do_vfsub_write_k(file, kbuf, count, ppos)
#define vfsub_readdir(file, filldir, arg, dlgt) \
	do_vfsub_readdir(file, filldir, arg)
#endif /* CONFIG_AUFS_DLGT */

/* ---------------------------------------------------------------------- */

static inline
struct dentry *vfsub_lock_rename(struct dentry *d1, struct dentry *d2)
{
	struct dentry *d;
	lockdep_off();
	d = lock_rename(d1, d2);
	lockdep_on();
	return d;
}

static inline void vfsub_unlock_rename(struct dentry *d1, struct dentry *d2)
{
	lockdep_off();
	unlock_rename(d1, d2);
	lockdep_on();
}

/* ---------------------------------------------------------------------- */

int vfsub_unlink(struct inode *dir, struct dentry *dentry, int dlgt);
int vfsub_statfs(void *arg, struct kstatfs *buf, int dlgt);

#endif /* __KERNEL__ */
#endif /* __AUFS_VFSUB_H__ */
