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

/* $Id: xino.c,v 1.21 2007/03/19 04:31:31 sfjro Exp $ */

//#include <linux/fs.h>
#include <linux/fsnotify.h>
#include <asm/uaccess.h>
#include "aufs.h"

static readf_t find_readf(struct file *hidden_file)
{
	const struct file_operations *op = hidden_file->f_op;

	if (op) {
		if (op->read)
			return op->read;
		if (op->aio_read)
			return do_sync_read;
	}
	return ERR_PTR(-ENOSYS);
}

static writef_t find_writef(struct file *hidden_file)
{
	const struct file_operations *op = hidden_file->f_op;

	if (op) {
		if (op->write)
			return op->write;
		if (op->aio_write)
			return do_sync_write;
	}
	return ERR_PTR(-ENOSYS);
}

static ssize_t fread(readf_t func, struct file *file, void *buf, size_t size,
		     loff_t *pos)
{
	ssize_t err;
	mm_segment_t oldfs;

	LKTRTrace("%.*s, sz %d, *pos %Ld\n",
		  DLNPair(file->f_dentry), size, *pos);

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	do {
		err = func(file, (char __user*)buf, size, pos);
	} while (err == -EAGAIN || err == -EINTR);
	set_fs(oldfs);
	//smp_mb();

#if 0
	if (err > 0)
		fsnotify_access(file->f_dentry);
#endif

	TraceErr(err);
	return err;
}

static ssize_t do_fwrite(writef_t func, struct file *file, void *buf,
			 size_t size, loff_t *pos)
{
	ssize_t err;
	mm_segment_t oldfs;

	lockdep_off();
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	do {
		err = func(file, (const char __user*)buf, size, pos);
	} while (err == -EAGAIN || err == -EINTR);
	set_fs(oldfs);
	lockdep_on();

#if 0
	if (err > 0)
		fsnotify_modify(file->f_dentry);
#endif

	TraceErr(err);
	return err;
}

struct do_fwrite_args {
	ssize_t *errp;
	writef_t func;
	struct file *file;
	void *buf;
	size_t size;
	loff_t *pos;
};

static void call_do_fwrite(void *args)
{
	struct do_fwrite_args *a = args;
	*a->errp = do_fwrite(a->func, a->file, a->buf, a->size, a->pos);
}

static ssize_t fwrite(writef_t func, struct file *file, void *buf, size_t size,
		      loff_t *pos)
{
	ssize_t err;

	LKTRTrace("%.*s, sz %d, *pos %Ld\n",
		  DLNPair(file->f_dentry), size, *pos);

	// signal block and no wkq?
	/*
	 * it breaks RLIMIT_FSIZE and normal user's limit,
	 * users should care about quota and real 'filesystem full.'
	 */
	if (!is_kthread(current)) {
		struct do_fwrite_args args = {
			.errp	= &err,
			.func	= func,
			.file	= file,
			.buf	= buf,
			.size	= size,
			.pos	= pos,
		};
		wkq_wait(call_do_fwrite, &args, /*dlgt*/0);
	} else
		err = do_fwrite(func, file, buf, size, pos);

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

struct file *xino_create(struct super_block *sb, char *fname, int silent,
			 struct dentry *parent)
{
	struct file *file;
	int err;
	struct dentry *hidden_parent;
	struct inode *hidden_dir;
	const int udba = IS_MS(sb, MS_UDBA_INOTIFY);

	LKTRTrace("%s\n", fname);

	// LSM may detect it
	// use sio?
	file = vfsub_filp_open(fname, O_RDWR | O_CREAT | O_EXCL | O_LARGEFILE,
			       S_IRUGO | S_IWUGO);
	//file = ERR_PTR(-1);
	if (IS_ERR(file)) {
		if (!silent)
			Err("open %s(%ld)\n", fname, PTR_ERR(file));
		return file;
	}
	if (unlikely(udba && parent))
		direval_dec(parent);

	/* keep file count */
	hidden_parent = dget_parent(file->f_dentry);
	hidden_dir = hidden_parent->d_inode;
	hi_lock_parent(hidden_dir);
	err = vfsub_unlink(hidden_dir, file->f_dentry, /*dlgt*/0);
	if (unlikely(udba && parent))
		direval_dec(parent);
	i_unlock(hidden_dir);
	dput(hidden_parent);
	if (unlikely(err)) {
		if (!silent)
			Err("unlink %s(%d)\n", fname, err);
		goto out;
	}
	if (sb != file->f_dentry->d_sb)
		return file; /* success */

	if (!silent)
		Err("%s must be outside\n", fname);
	err = -EINVAL;

 out:
	fput(file);
	file = ERR_PTR(err);
	return file;
}

/* ---------------------------------------------------------------------- */

/*
 * write @ino to the xinofile for the specified branch{@sb, @bindex}
 * at the position of @hidden_ino.
 * when @ino is zero, it is written to the xinofile and means no entry.
 */
int xino_write(struct super_block *sb, aufs_bindex_t bindex, ino_t hidden_ino,
	       ino_t ino)
{
	struct aufs_branch *br;
	loff_t pos;
	ssize_t sz;

	LKTRTrace("b%d, hi%lu, i%lu\n", bindex, hidden_ino, ino);
	//DEBUG_ON(!ino);
	if (unlikely(!IS_MS(sb, MS_XINO)))
		return 0;

	br = stobr(sb, bindex);
	DEBUG_ON(!br || !br->br_xino);
	pos = hidden_ino * sizeof(hidden_ino);
	sz = fwrite(br->br_xino_write, br->br_xino, &ino, sizeof(ino), &pos);
	//if (LktrCond) sz = 1;
	if (sz == sizeof(ino))
		return 0; /* success */

	IOErr("write failed (%d)\n", sz);
	return -EIO;
}

// why atomic_long_inc_return is not defined?
#if BITS_PER_LONG == 64
#define atomic_long_inc_return(a)	atomic64_inc_return(a)
#else
#define atomic_long_inc_return(a)	atomic_inc_return(a)
#endif

/*
 * read @ino from xinofile for the specified branch{@sb, @bindex}
 * at the position of @hidden_ino.
 * if @ino does not exist and @force is true, get new one.
 */
int xino_read(struct super_block *sb, aufs_bindex_t bindex, ino_t hidden_ino,
	      ino_t *ino, int force)
{
	struct file *file;
	loff_t pos, pos2;
	ssize_t sz;
	struct aufs_branch *br;

	LKTRTrace("b%d, hi%lu, force %d\n", bindex, hidden_ino, force);
	if (unlikely(!IS_MS(sb, MS_XINO))) {
		*ino = iunique(sb, AUFS_FIRST_INO);
		if (*ino >= AUFS_FIRST_INO)
			return 0;
		goto out_overflow;
	}

	br = stobr(sb, bindex);
	file = br->br_xino;
	DEBUG_ON(!file);
	sz = 0;
	pos2 = pos = hidden_ino * sizeof(hidden_ino);
	if (i_size_read(file->f_dentry->d_inode) >= pos + sizeof(*ino)) {
		sz = fread(br->br_xino_read, file, ino, sizeof(*ino), &pos);
		//if (LktrCond) sz = 1;
		if (sz == sizeof(*ino) && *ino)
			return 0; /* success */
	}
	if (unlikely(!force))
		goto out;

	if (!sz || sz == sizeof(*ino)) {
		*ino = atomic_long_inc_return(&stosi(sb)->si_xino);
		if (*ino >= AUFS_FIRST_INO) {
			sz = fwrite(br->br_xino_write, file, ino, sizeof(*ino),
				    &pos2);
			//if (LktrCond) sz = 1;
			if (sz == sizeof(ino))
				return 0; /* success */
			*ino = 0;
			IOErr("write failed (%d)\n", sz);
			return -EIO;
		} else
			goto out_overflow;
	}
	IOErr("read failed (%d)\n", sz);
 out:
	return -EIO;
 out_overflow:
	*ino = 0;
	IOErr("inode number overflow\n");
	goto out;
}

/*
 * find another branch who is on the same filesystem of the specified
 * branch{@btgt}. search until @bend.
 */
static int is_sb_shared(struct super_block *sb, aufs_bindex_t btgt,
			aufs_bindex_t bend)
{
	aufs_bindex_t bindex;
	struct super_block *tgt_sb = sbr_sb(sb, btgt);

	for (bindex = 0; bindex <= bend; bindex++)
		if (unlikely(btgt != bindex && tgt_sb == sbr_sb(sb, bindex)))
			return bindex;
	return -1;
}

/*
 * create a new xinofile at the same place/path as @base_file.
 */
static struct file *xino_create2(struct file *base_file)
{
	struct file *file;
	int err;
	struct dentry *base, *dentry, *parent;
	struct inode *dir;
	struct qstr *name;
	struct lkup_args lkup = {
		.nfsmnt	= NULL,
		.dlgt	= 0
	};

	base = base_file->f_dentry;
	LKTRTrace("%.*s\n", DLNPair(base));
	parent = base->d_parent;
	dir = parent->d_inode;
	IMustLock(dir);

#if 0
	file = ERR_PTR(-EINVAL);
	if (unlikely(SB_NFS(parent->d_sb)))
		goto out;
#endif

	// do not superio, nor NFS.
	name = &base->d_name;
	dentry = lkup_one(name->name, parent, name->len, &lkup);
	//if (LktrCond) {dput(dentry); dentry = ERR_PTR(-1);}
	if (IS_ERR(dentry)) {
		file = (void*)dentry;
		Err("%.*s lookup err %ld\n", LNPair(name), PTR_ERR(dentry));
		goto out;
	}
	err = vfsub_create(dir, dentry, S_IRUGO | S_IWUGO, NULL, /*dlgt*/0);
	//if (LktrCond) {vfs_unlink(dir, dentry); err = -1;}
	if (unlikely(err)) {
		file = ERR_PTR(err);
		Err("%.*s create err %d\n", LNPair(name), err);
		goto out_dput;
	}
	file = dentry_open(dget(dentry), mntget(base_file->f_vfsmnt),
			   O_RDWR | O_CREAT | O_EXCL | O_LARGEFILE);
	//if (LktrCond) {fput(file); file = ERR_PTR(-1);}
	if (IS_ERR(file)) {
		Err("%.*s open err %ld\n", LNPair(name), PTR_ERR(file));
		goto out_dput;
	}
	err = vfsub_unlink(dir, dentry, /*dlgt*/0);
	//if (LktrCond) err = -1;
	if (!err)
		goto out_dput; /* success */

	Err("%.*s unlink err %d\n", LNPair(name), err);
	fput(file);
	file = ERR_PTR(err);

 out_dput:
	dput(dentry);
 out:
	TraceErrPtr(file);
	return file;
}

/*
 * initialize the xinofile for the specified branch{@sb, @bindex}
 * at the place/path where @base_file indicates.
 * test whether another branch is on the same filesystem or not,
 * if @do_test is true.
 */
int xino_init(struct super_block *sb, aufs_bindex_t bindex,
	      struct file *base_file, int do_test)
{
	int err;
	struct aufs_branch *br;
	aufs_bindex_t bshared, bend;
	struct file *file;
	struct inode *inode, *hidden_inode;

	LKTRTrace("b%d, base_file %p, do_test %d\n",
		  bindex, base_file, do_test);
	SiMustWriteLock(sb);
	DEBUG_ON(!IS_MS(sb, MS_XINO));
	br = stobr(sb, bindex);
	DEBUG_ON(br->br_xino);

	file = NULL;
	bshared = -1;
	bend = sbend(sb);
	if (do_test)
		bshared = is_sb_shared(sb, bindex, bend);
	if (unlikely(bshared >= 0)) {
		struct aufs_branch *shared_br = stobr(sb, bshared);
		if (shared_br->br_xino) {
			file = shared_br->br_xino;
			get_file(file);
		}
	}

	if (!file) {
		struct dentry *parent = dget_parent(base_file->f_dentry);
		struct inode *dir = parent->d_inode;

		hi_lock_parent(dir);
		file = xino_create2(base_file);
		//if (LktrCond) {fput(file); file = ERR_PTR(-1);}
		i_unlock(dir);
		dput(parent);
		err = PTR_ERR(file);
		if (IS_ERR(file))
			goto out;
	}
	br->br_xino = file;
	br->br_xino_read = find_readf(file);
	err = PTR_ERR(br->br_xino_read);
	if (IS_ERR(br->br_xino_read))
		goto out_put;
	br->br_xino_write = find_writef(file);
	err = PTR_ERR(br->br_xino_write);
	if (IS_ERR(br->br_xino_write))
		goto out_put;

	inode = sb->s_root->d_inode;
	hidden_inode = h_iptr_i(inode, bindex);
	err = xino_write(sb, bindex, hidden_inode->i_ino, inode->i_ino);
	//if (LktrCond) err = -1;
	if (!err)
		return 0; /* success */

 out_put:
	fput(file);
	br->br_xino = NULL;
 out:
	TraceErr(err);
	return err;
}

/*
 * set xino mount option.
 */
int xino_set(struct super_block *sb, struct opt_xino *xino, int remount)
{
	int err, sparse;
	aufs_bindex_t bindex, bend;
	struct aufs_branch *br;
	struct dentry *parent;
	struct qstr *name;
	struct file *cur_xino;
	struct inode *dir;

	LKTRTrace("%s\n", xino->path);

	err = 0;
	name = &xino->file->f_dentry->d_name;
	parent = dget_parent(xino->file->f_dentry);
	dir = parent->d_inode;
	cur_xino = stobr(sb, 0)->br_xino;
	if (remount
	    && cur_xino
	    && cur_xino->f_dentry->d_parent == parent
	    && name->len == cur_xino->f_dentry->d_name.len
	    && !memcmp(name->name, cur_xino->f_dentry->d_name.name, name->len))
		goto out;

	MS_SET(sb, MS_XINO);
	bend = sbend(sb);
	for (bindex = bend; bindex >= 0; bindex--) {
		br = stobr(sb, bindex);
		if (br->br_xino && file_count(br->br_xino) > 1) {
			fput(br->br_xino);
			br->br_xino = NULL;
		}
	}

	for (bindex = 0; bindex <= bend; bindex++) {
		struct file *file;
		struct inode *inode;

		br = stobr(sb, bindex);
		if (!br->br_xino)
			continue;

		DEBUG_ON(file_count(br->br_xino) != 1);
		hi_lock_parent(dir);
		file = xino_create2(xino->file);
		//if (LktrCond) {fput(file); file = ERR_PTR(-1);}
		err = PTR_ERR(file);
		if (IS_ERR(file)) {
			i_unlock(dir);
			break;
		}
		inode = br->br_xino->f_dentry->d_inode;
		err = au_copy_file(file, br->br_xino, i_size_read(inode), sb,
				   &sparse);
		//if (LktrCond) err = -1;
		i_unlock(dir);
		if (unlikely(err)) {
			fput(file);
			break;
		}
		fput(br->br_xino);
		br->br_xino = file;
		br->br_xino_read = find_readf(file);
		DEBUG_ON(IS_ERR(br->br_xino_read));
		br->br_xino_write = find_writef(file);
		DEBUG_ON(IS_ERR(br->br_xino_write));
	}

	for (bindex = 0; bindex <= bend; bindex++)
		if (!stobr(sb, bindex)->br_xino) {
			err = xino_init(sb, bindex, xino->file, /*do_test*/1);
			//if (LktrCond) {fput(stobr(sb, bindex)->br_xino);
			//stobr(sb, bindex)->br_xino = NULL; err = -1;}
			if (!err)
				continue;
			IOErr("creating xino for branch %d(%d), "
			      "forcing noxino\n", bindex, err);
			MS_CLR(sb, MS_XINO);
			err = -EIO;
			break;
		}
 out:
	dput(parent);
	TraceErr(err);
	return err;
}

/*
 * clear xino mount option
 */
int xino_clr(struct super_block *sb)
{
	aufs_bindex_t bindex, bend;

	TraceEnter();
	SiMustWriteLock(sb);

	bend = sbend(sb);
	for (bindex = 0; bindex <= bend; bindex++) {
		struct aufs_branch *br;
		br = stobr(sb, bindex);
		if (br->br_xino) {
			fput(br->br_xino);
			br->br_xino = NULL;
		}
	}

	MS_CLR(sb, MS_XINO);
	return 0;
}

/*
 * create a xinofile at the default place/path.
 */
struct file *xino_def(struct super_block *sb)
{
	struct file *file;
	aufs_bindex_t bend, bindex, bwr;
	char *page, *p;

	bend = sbend(sb);
	bwr = -1;
	for (bindex = 0; bindex <= bend; bindex++)
		if ((sbr_perm(sb, bindex) & MAY_WRITE)
		    && !SB_NFS(h_dptr_i(sb->s_root, bindex)->d_sb)) {
			bwr = bindex;
			break;
		}

	if (bwr != -1) {
		// todo: rewrite with lkup_one()
		file = ERR_PTR(-ENOMEM);
		page = __getname();
		//if (LktrCond) {__putname(page); page = NULL;}
		if (unlikely(!page))
			goto out;
		p = d_path(h_dptr_i(sb->s_root, bwr), sbr_mnt(sb, bwr), page,
			   PATH_MAX - sizeof(AUFS_XINO_FNAME));
		//if (LktrCond) p = ERR_PTR(-1);
		file = (void*)p;
		if (p && !IS_ERR(p)) {
			strcat(p, "/" AUFS_XINO_FNAME);
			LKTRTrace("%s\n", p);
			file = xino_create(sb, p, /*silent*/0, sb->s_root);
			//if (LktrCond) {fput(file); file = ERR_PTR(-1);}
		}
		__putname(page);
	} else {
		file = xino_create(sb, AUFS_XINO_DEFPATH, /*silent*/0,
				   /*parent*/NULL);
		//if (LktrCond) {fput(file); file = ERR_PTR(-1);}
	}

 out:
	TraceErrPtr(file);
	return file;
}
