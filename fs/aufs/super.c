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

/* $Id: super.c,v 1.37 2007/02/19 03:29:30 sfjro Exp $ */

#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
//#include <linux/version.h>
#include "aufs.h"

/*
 * super_operations
 */
static struct inode *aufs_alloc_inode(struct super_block *sb)
{
	struct aufs_icntnr *c;

	TraceEnter();

	c = cache_alloc_icntnr();
	//if (LktrCond) {cache_free_icntnr(c); c = NULL;}
	if (c) {
		inode_init_once(&c->vfs_inode);
		c->vfs_inode.i_version = 1; //sigen(sb);
		c->iinfo.ii_hinode = NULL;
		return &c->vfs_inode;
	}
	return NULL;
}

static void aufs_destroy_inode(struct inode *inode)
{
	LKTRTrace("i%lu\n", inode->i_ino);
	iinfo_fin(inode);
	cache_free_icntnr(container_of(inode, struct aufs_icntnr, vfs_inode));
}

static void aufs_read_inode(struct inode *inode)
{
	int err;

	LKTRTrace("i%lu\n", inode->i_ino);

	err = iinfo_init(inode);
	//if (LktrCond) err = -1;
	if (!err) {
		inode->i_version++;
		inode->i_op = &aufs_iop;
		inode->i_fop = &aufs_file_fop;
		inode->i_mapping->a_ops = &aufs_aop;
		return; /* success */
	}

	LKTRTrace("intializing inode info failed(%d)\n", err);
	make_bad_inode(inode);
}

static int pr_esc(struct seq_file *m, char *p)
{
	return seq_escape(m, p, " \t\n\\");
}

static int aufs_show_options(struct seq_file *m, struct vfsmount *mnt)
{
	int err, n;
	aufs_bindex_t bindex, bend;
	struct super_block *sb;
	struct aufs_sbinfo *sbinfo;
	char *hidden_path, *page;
	struct dentry *root;

	TraceEnter();

	err = -ENOMEM;
	page = __getname();
	//if (LktrCond) {__putname(page);page = NULL;}
	if (unlikely(!page))
		goto out;

	sb = mnt->mnt_sb;
	root = sb->s_root;
	aufs_read_lock(root, !AUFS_I_RLOCK);
	sbinfo = stosi(sb);
	if (IS_MS(sb, MS_XINO)) {
		struct aufs_branch *br;
		struct file *f;
		char *p, *q;

		br = stobr(sb, 0);
		f = br->br_xino;
		p = d_path(f->f_dentry, f->f_vfsmnt, page, PATH_MAX);
		//if (LktrCond) p = ERR_PTR(-1);
		err = PTR_ERR(p);
		if (IS_ERR(p))
			goto out_unlock;
#define Deleted " (deleted)"
		q = p + strlen(p) - sizeof(Deleted) + 1;
		DEBUG_ON(strcmp(q, Deleted));
#undef Deleted
		*q = 0;
		seq_printf(m, ",xino=");
		pr_esc(m, p);
	} else
		seq_printf(m, ",noxino");
	n = IS_MS(sb, MS_PLINK);
	if (unlikely((MS_DEF & MS_PLINK) != n))
		seq_printf(m, ",%splink", n ? "" : "no");
	n = MS_UDBA(sb);
	if (unlikely(n != MS_UDBA_DEF))
		seq_printf(m, ",udba=%s", udba_str(n));
	n = IS_MS(sb, MS_ALWAYS_DIROPQ);
	if (unlikely((MS_DEF & MS_ALWAYS_DIROPQ) != n))
		seq_printf(m, ",diropq=%c", n ? 'a' : 'w');
	n = IS_MS(sb, MS_IPRIVATE);
	if (unlikely((MS_DEF & MS_IPRIVATE) != n))
		seq_printf(m, ",%sipriv", n ? "" : "no");
	n = sbinfo->si_dirwh;
	if (unlikely(n != AUFS_DIRWH_DEF))
		seq_printf(m, ",dirwh=%d", n);
	n = sbinfo->si_rdcache / HZ;
	if (unlikely(n != AUFS_RDCACHE_DEF))
		seq_printf(m, ",rdcache=%d", n);
	n = MS_COO(sb);
	if (unlikely(n != MS_COO_DEF))
		seq_printf(m, ",coo=%s", coo_str(n));

#ifdef CONFIG_AUFS_COMPAT
	seq_printf(m, ",dirs=");
#else
	seq_printf(m, ",br:");
#endif
	err = 0;
	bend = sbend(sb);
	for (bindex = 0; bindex <= bend; bindex++) {
		char a[16];
		hidden_path = d_path(h_dptr_i(root, bindex),
				     sbr_mnt(sb, bindex), page, PATH_MAX);
		//if (LktrCond) hidden_path = ERR_PTR(-1);
		err = PTR_ERR(hidden_path);
		if (IS_ERR(hidden_path))
			break;
		err = sbr_perm_str(sb, bindex, a, sizeof(a));
		//if (LktrCond) err = -1;
		if (unlikely(err))
			break;
		pr_esc(m, hidden_path);
		seq_printf(m, "=%s", a);
		if (bindex != bend)
			seq_printf(m, ":");
	}

 out_unlock:
	aufs_read_unlock(root, !AUFS_I_RLOCK);
	__putname(page);
 out:
	TraceErr(err);
	return err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#define StatfsLock(d)	aufs_read_lock(d->d_sb->s_root, 0)
#define StatfsUnlock(d)	aufs_read_unlock(d->d_sb->s_root, 0)
#define StatfsArg(d)	h_dptr(d->d_sb->s_root)
#define StatfsHInode(d)	StatfsArg(d)->d_inode
#define StatfsSb(d)	d->d_sb
static int aufs_statfs(struct dentry *arg, struct kstatfs *buf)
#else
#define StatfsLock(s)	si_read_lock(s)
#define StatfsUnlock(s)	si_read_unlock(s)
#define StatfsArg(s)	sbr_sb(s, 0)
#define StatfsHInode(s)	StatfsArg(s)->s_root->d_inode
#define StatfsSb(s)	s
static int aufs_statfs(struct super_block *arg, struct kstatfs *buf)
#endif
{
	int err;
	struct inode *h_inode;
	struct aufs_h_ipriv ipriv;

	TraceEnter();

	StatfsLock(arg);
	h_inode = StatfsHInode(arg);
	h_ipriv(h_inode, &ipriv, StatfsSb(arg), /*do_lock*/1);
	err = vfs_statfs(StatfsArg(arg), buf);
	h_iunpriv(&ipriv, /*do_lock*/1);
	//if (LktrCond) err = -1;
	StatfsUnlock(arg);
	if (!err) {
		//buf->f_type = AUFS_SUPER_MAGIC;
		buf->f_type = 0;
		buf->f_namelen -= AUFS_WH_LEN;
		memset(&buf->f_fsid, 0, sizeof(buf->f_fsid));
	}
	//buf->f_bsize = buf->f_blocks = buf->f_bfree = buf->f_bavail = -1;

	TraceErr(err);
	return err;
}
#undef StatfsSb
#undef StatfsArg

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#define UmountBeginSb(mnt)	(mnt)->mnt_sb
static void aufs_umount_begin(struct vfsmount *arg, int flags)
#else
#define UmountBeginSb(sb)	(sb)
static void aufs_umount_begin(struct super_block *arg)
#endif
{
	struct super_block *sb = UmountBeginSb(arg);

	si_write_lock(sb);
	if (IS_MS(sb, MS_PLINK)) {
		put_plink(sb);
		//kobj_umount(stosi(sb));
	}
	si_write_unlock(sb);
}
#undef UmountBeginSb

static void free_sbinfo(struct aufs_sbinfo *sbinfo)
{
	TraceEnter();
	DEBUG_ON(!sbinfo);
	DEBUG_ON(!list_empty(&sbinfo->si_plink));

	rw_destroy(&sbinfo->si_rwsem);
	free_branches(sbinfo);
	kfree(sbinfo->si_branch);
	kfree(sbinfo);
}

/* final actions when unmounting a file system */
static void aufs_put_super(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;

	TraceEnter();

	sbinfo = stosi(sb);
	if (unlikely(!sbinfo))
		return;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
	// umount_begin() may not be called.
	aufs_umount_begin(sb);
#endif
	free_sbinfo(sbinfo);
}

/* ---------------------------------------------------------------------- */

/*
 * refresh directories at remount time.
 */
static int do_refresh_dir(struct dentry *dentry, unsigned int flags)
{
	int err;
	struct dentry *parent;

	LKTRTrace("%.*s\n", DLNPair(dentry));
	DEBUG_ON(!dentry->d_inode || !S_ISDIR(dentry->d_inode->i_mode));

	di_write_lock(dentry);
	parent = dentry->d_parent;
	if (!IS_ROOT(parent))
		di_read_lock(parent, AUFS_I_RLOCK);
	else
		DiMustAnyLock(parent);
	err = refresh_hdentry(dentry, S_IFDIR);
	if (err >= 0) {
		err = refresh_hinode(dentry);
		if (!err)
			reset_hinotify(dentry->d_inode, flags);
	}
	if (unlikely(err))
		Err("unrecoverable error %d\n", err);
	if (!IS_ROOT(parent))
		di_read_unlock(parent, AUFS_I_RLOCK);
	di_write_unlock(dentry);

	TraceErr(err);
	return err;
}

static int refresh_dir(struct dentry *root, int sgen)
{
	int err;
	struct dentry *this_parent = root;
	struct list_head *next;
	const unsigned int flags = hi_flags(root->d_inode, /*isdir*/1);

	LKTRTrace("sgen %d\n", sgen);
	SiMustWriteLock(root->d_sb);
	DEBUG_ON(digen(root) != sgen);
	DiMustWriteLock(root);

	err = 0;
	//spin_lock(&dcache_lock);
 repeat:
	next = this_parent->d_subdirs.next;
 resume:
	if (!IS_ROOT(this_parent)
	    && this_parent->d_inode
	    && S_ISDIR(this_parent->d_inode->i_mode)
	    && digen(this_parent) != sgen) {
		err = do_refresh_dir(this_parent, flags);
		if (unlikely(err))
			goto out;
	}

	while (next != &this_parent->d_subdirs) {
		struct list_head *tmp = next;
		struct dentry *dentry;

		dentry = list_entry(tmp, struct dentry, D_CHILD);
		next = tmp->next;
		if (unlikely(d_unhashed(dentry) || !dentry->d_inode))
			continue;
		if (!list_empty(&dentry->d_subdirs)) {
			this_parent = dentry;
			goto repeat;
		}
		if (dentry->d_inode && S_ISDIR(dentry->d_inode->i_mode)) {
			DEBUG_ON(IS_ROOT(dentry) || digen(dentry) == sgen);
			err = do_refresh_dir(dentry, flags);
			if (unlikely(err))
				goto out;
		}
	}
	if (this_parent != root) {
		next = this_parent->D_CHILD.next;
		this_parent = this_parent->d_parent;
		goto resume;
	}

 out:
	//spin_unlock(&dcache_lock);
	TraceErr(err);
	return err;
}

// stop extra interpretation of errno in mount(8), and strange error messages.
static int cvt_err(int err)
{
	TraceErr(err);

	switch (err) {
	case -ENOENT:
	case -ENOTDIR:
	case -EEXIST:
	case -EIO:
		err = -EINVAL;
	}
	return err;
}

/* protected by s_umount */
static int aufs_remount_fs(struct super_block *sb, int *flags, char *data)
{
	enum {Old, New};
	enum {Sgen, Hinotify, Xino};
	int err, v[2][3], i, refresh;
	struct dentry *root;
	struct inode *inode;
	struct aufs_sbinfo *sbinfo;
	struct opts opts;

	LKTRTrace("flags 0x%x, data %s, len %d\n",
		  *flags, data ? data : "NULL", data ? strlen(data) : 0);

	if (unlikely(!data || !*data))
		return 0; /* success */

	err = -ENOMEM;
	opts.max_opt = PAGE_SIZE / sizeof(*opts.opt);
	memset(&opts.xino, 0, sizeof(opts.xino));
	opts.opt = (void*)__get_free_page(GFP_KERNEL);
	//if (LktrCond) {free_page((unsigned long)opts.opt); opts.opt = NULL;}
	if (unlikely(!opts.opt))
		goto out;

	/* parse it before aufs lock */
	err = parse_opts(sb, data, &opts, /*remount*/1);
	//if (LktrCond) {free_opts(&opts); err = -1;}
	if (unlikely(err))
		goto out_opts;

	root = sb->s_root;
	inode = root->d_inode;
	i_lock(inode);
	aufs_write_lock(root);

	v[Old][Sgen] = sigen(sb);
	v[Old][Hinotify] = IS_MS(sb, MS_UDBA_INOTIFY);
	v[Old][Xino] = IS_MS(sb, MS_XINO);
	err = do_opts(sb, &opts, /*remount*/1);
	//if (LktrCond) err = -1;
	free_opts(&opts);
	v[New][Sgen] = sigen(sb);
	v[New][Hinotify] = IS_MS(sb, MS_UDBA_INOTIFY);
	v[New][Xino] = IS_MS(sb, MS_XINO);

	/* do_opts() may return an error */
	sbinfo = stosi(sb);
	refresh = 0;
	for (i = 0; !refresh && i < 3; i++)
		refresh = (v[Old][i] != v[New][i]);
	if (refresh) {
		int rerr;
		sbinfo->si_failed_refresh_dirs = 0;
		rerr = refresh_dir(root, v[New][Sgen]);
		if (unlikely(rerr)) {
			sbinfo->si_failed_refresh_dirs = 1;
			Warn("Refreshing directories failed, ignores (%d)\n",
			     rerr);
		}
	}

	aufs_write_unlock(root);
	i_unlock(inode);

 out_opts:
	free_page((unsigned long)opts.opt);
 out:
	if (!err)
		return 0;
	err = cvt_err(err);
	TraceErr(err);
	return err;
}

static struct super_operations aufs_sop = {
	.alloc_inode	= aufs_alloc_inode,
	.destroy_inode	= aufs_destroy_inode,
	.read_inode	= aufs_read_inode,
	//.dirty_inode	= aufs_dirty_inode,
	//.write_inode	= aufs_write_inode,
	//void (*put_inode) (struct inode *);
	.drop_inode	= generic_delete_inode,
	//.delete_inode	= aufs_delete_inode,
	//.clear_inode	= aufs_clear_inode,

	.show_options	= aufs_show_options,
	.statfs		= aufs_statfs,

	.put_super	= aufs_put_super,
	//void (*write_super) (struct super_block *);
	//int (*sync_fs)(struct super_block *sb, int wait);
	//void (*write_super_lockfs) (struct super_block *);
	//void (*unlockfs) (struct super_block *);
	.remount_fs	= aufs_remount_fs,
	// depends upon umount flags. also use put_super() (< 2.6.18)
	.umount_begin	= aufs_umount_begin
};

/* ---------------------------------------------------------------------- */

/*
 * at first mount time.
 */

static int alloc_sbinfo(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;

	TraceEnter();

	sbinfo = kmalloc(sizeof(*sbinfo), GFP_KERNEL);
	//if (LktrCond) {kfree(sbinfo); sbinfo = NULL;}
	if (unlikely(!sbinfo))
		goto out;
	sbinfo->si_branch = kzalloc(sizeof(*sbinfo->si_branch), GFP_KERNEL);
	//if (LktrCond) {kfree(sbinfo->si_branch); sbinfo->si_branch = NULL;}
	if (unlikely(!sbinfo->si_branch)) {
		kfree(sbinfo);
		goto out;
	}
	rw_init_wlock(&sbinfo->si_rwsem);
	sbinfo->si_bend = -1;
	atomic_long_set(&sbinfo->si_xino, AUFS_FIRST_INO);
	spin_lock_init(&sbinfo->si_plink_lock);
	INIT_LIST_HEAD(&sbinfo->si_plink);
	init_lvma(sbinfo);
	sbinfo->si_generation = 1;
	sbinfo->si_last_br_id = 0;
	sbinfo->si_failed_refresh_dirs = 0;
	sbinfo->si_flags = 0;
	sbinfo->si_dirwh = AUFS_DIRWH_DEF;
	sbinfo->si_rdcache = AUFS_RDCACHE_DEF * HZ;
	//memset(&sbinfo->si_kobj, 0, sizeof(sbinfo->si_kobj));
	sb->s_fs_info = sbinfo;
	MS_SET(sb, MS_DEF);
	return 0; /* success */

 out:
	TraceErr(-ENOMEM);
	return -ENOMEM;
}

static int alloc_root(struct super_block *sb)
{
	int err;
	struct inode *inode;
	struct dentry *root;

	TraceEnter();

	err = -ENOMEM;
	inode = iget(sb, AUFS_ROOT_INO);
	//if (LktrCond) {iput(inode); inode = NULL;}
	if (unlikely(!inode))
		goto out;
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out;
	err = -ENOMEM;
	if (unlikely(is_bad_inode(inode)))
		goto out_iput;

	root = d_alloc_root(inode);
	//if (LktrCond) {igrab(inode); dput(root); root = NULL;}
	if (unlikely(!root))
		goto out_iput;
	if (!IS_ERR(root)) {
		err = alloc_dinfo(root);
		//if (LktrCond){rw_write_unlock(&dtodi(root)->di_rwsem);err=-1;}
		if (!err) {
			sb->s_root = root;
			return 0; /* success */
		}
		dput(root);
		goto out;
	}
	err = PTR_ERR(root);

 out_iput:
	iput(inode);
 out:
	TraceErr(err);
	return err;

}

static int aufs_fill_super(struct super_block *sb, void *raw_data, int silent)
{
	int err, need_xino;
	struct dentry *root;
	struct inode *inode;
	struct opts opts;
	struct opt_xino xino;
	aufs_bindex_t bend, bindex;
	char *arg = raw_data;

	if (unlikely(!arg || !*arg)) {
		err = -EINVAL;
		Err("no arg\n");
		goto out;
	}
	LKTRTrace("%s, silent %d\n", arg, silent);

	err = -ENOMEM;
	opts.opt = (void*)__get_free_page(GFP_KERNEL);
	//if (LktrCond) {free_page((unsigned long)opts.opt); opts.opt = NULL;}
	if (unlikely(!opts.opt))
		goto out;
	opts.max_opt = PAGE_SIZE / sizeof(opts);
	memset(&opts.xino, 0, sizeof(opts.xino));

	err = alloc_sbinfo(sb);
	//if (LktrCond) {si_write_unlock(sb);free_sbinfo(stosi(sb));err=-1;}
	if (unlikely(err))
		goto out_opts;
	SiMustWriteLock(sb);
	/* all timestamps always follow the ones on the branch */
	sb->s_flags |= MS_NOATIME | MS_NODIRATIME;
	sb->s_op = &aufs_sop;
	init_export_op(sb);
	//err = kobj_mount(stosi(sb));
	//if (err)
	//goto out_info;

	err = alloc_root(sb);
	//if (LktrCond) {rw_write_unlock(&dtodi(sb->s_root)->di_rwsem);
	//dput(sb->s_root);sb->s_root=NULL;err=-1;}
	if (unlikely(err)) {
		DEBUG_ON(sb->s_root);
		si_write_unlock(sb);
		goto out_info;
	}
	root = sb->s_root;
	DiMustWriteLock(root);
	inode = root->d_inode;
	inode->i_nlink = 2;
	ii_write_lock(inode);

	// lock vfs_inode first, then aufs.
	aufs_write_unlock(root);

	/*
	 * actually we can parse options regardless aufs lock here.
	 * but at remount time, parsing must be done before aufs lock.
	 * so we follow the same rule.
	 */
	err = parse_opts(sb, arg, &opts, /*remount*/0);
	//if (LktrCond) {free_opts(&opts); err = -1;}
	if (unlikely(err))
		goto out_root;

	i_lock(inode);
	inode->i_op = &aufs_dir_iop;
	inode->i_fop = &aufs_dir_fop;
	aufs_write_lock(root);

	/* handle options except xino */
	sb->s_maxbytes = 0;
	xino = opts.xino;
	memset(&opts.xino, 0, sizeof(opts.xino));
	need_xino = IS_MS(sb, MS_XINO);
	MS_CLR(sb, MS_XINO);
	err = do_opts(sb, &opts, /*remount*/0);
	//if (LktrCond) err = -1;
	free_opts(&opts);
	if (unlikely(err))
		goto out_unlock;

	bend = sbend(sb);
	if (unlikely(bend < 0)) {
		err = -EINVAL;
		Err("no branches\n");
		goto out_unlock;
	}
	DEBUG_ON(!sb->s_maxbytes);

	/* post-process options, xino only */
	if (need_xino) {
		/* disable inotify temporary */
		const unsigned int hinotify = IS_MS(sb, MS_UDBA_INOTIFY);
		if (unlikely(hinotify)) {
			MS_CLR(sb, MS_UDBA_INOTIFY);
			MS_SET(sb, MS_UDBA_NONE);
			reset_hinotify(inode, hi_flags(inode, 1) & ~AUFS_HI_XINO);
		}

		MS_SET(sb, MS_XINO);
		if (!xino.file) {
			xino.file = xino_def(sb);
			//if (LktrCond) {fput(xino.file);xino.file=ERR_PTR(-1);}
			err = PTR_ERR(xino.file);
			if (IS_ERR(xino.file))
				goto out_unlock;
			err = 0;
		}

		for (bindex = 0; !err && bindex <= bend; bindex++) {
			err = xino_init(sb, bindex, xino.file,
					/*do_test*/bindex);
			//if (LktrCond) err = -1;
		}
		//err = -1;
		fput(xino.file);
		if (unlikely(err))
			goto out_unlock;

		if (unlikely(hinotify)) {
			MS_CLR(sb, MS_UDBA_NONE);
			MS_SET(sb, MS_UDBA_INOTIFY);
			reset_hinotify(inode, hi_flags(inode, 1) & ~AUFS_HI_XINO);
		}
	}

	//DbgDentry(root);
	aufs_write_unlock(root);
	i_unlock(inode);
	//DbgSb(sb);
	return 0; /* success */

 out_unlock:
	aufs_write_unlock(root);
	i_unlock(inode);
 out_root:
	dput(root);
	sb->s_root = NULL;
 out_info:
	free_sbinfo(stosi(sb));
	sb->s_fs_info = NULL;
 out_opts:
	free_page((unsigned long)opts.opt);
 out:
	err = cvt_err(err);
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
static int aufs_get_sb(struct file_system_type *fs_type, int flags,
		       const char *dev_name, void *raw_data,
		       struct vfsmount *mnt)
{
	/* all timestamps always follow the ones on the branch */
	//mnt->mnt_flags |= MNT_NOATIME | MNT_NODIRATIME;
	return get_sb_nodev(fs_type, flags, raw_data, aufs_fill_super, mnt);
}
#else
static struct super_block *aufs_get_sb(struct file_system_type *fs_type,
				       int flags, const char *dev_name,
				       void *raw_data)
{
	return get_sb_nodev(fs_type, flags, raw_data, aufs_fill_super);
}
#endif

struct file_system_type aufs_fs_type = {
	.name		= AUFS_FSTYPE,
	.fs_flags	= FS_REVAL_DOT, // for UDBA and NFS branch
	.get_sb		= aufs_get_sb,
	.kill_sb	= generic_shutdown_super,
	//no need to __module_get() and module_put().
	.owner		= THIS_MODULE,
};
