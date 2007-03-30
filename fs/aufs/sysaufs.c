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

/* $Id: sysaufs.c,v 1.1 2007/03/27 12:52:39 sfjro Exp $ */

#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/sysfs.h>
#include "aufs.h"

/* ---------------------------------------------------------------------- */

static ssize_t sysaufs_read(struct kobject *kobj, char *buf, loff_t offset,
			    size_t sz, int index);
static ssize_t sysaufs_free_write(struct kobject *kobj, char *buf,
				  loff_t offset, size_t sz, int index);
#define DefineRW(index) \
static ssize_t read_##index(struct kobject *kobj, char *buf, loff_t offset, \
			    size_t sz) \
{ \
	return sysaufs_read(kobj, buf, offset, sz, index); \
} \
static ssize_t write_##index(struct kobject *kobj, char *buf, loff_t offset, \
			     size_t sz) \
{ \
	return sysaufs_free_write(kobj, buf, offset, sz, index); \
}

#define Entry(_name, index, init_size) \
	[index] = { \
		.attr = { \
			.attr = { \
				.name	= #_name, \
				.owner	= THIS_MODULE, \
				.mode	= S_IRUGO | S_IWUSR \
			}, \
			.read	= read_##index, \
			.write	= write_##index \
		}, \
		.allocated = init_size, \
		.err = -1 \
	}

enum {Brs, Stat, Config};
DefineRW(Brs);
DefineRW(Stat);
DefineRW(Config);

static struct {
	struct bin_attribute attr;
	int allocated;	/* zero minus means pages */
	int err;
} entries[] = {
	Entry(brs, Brs, 128),
	Entry(stat, Stat, 32),
	Entry(config, Config, 256),
};

#define NEntries	(sizeof(entries) / sizeof(*entries))
#define Priv(i)		entries[i].attr.private
#define Allocated(i)	entries[i].allocated
#define Len(i)		entries[i].attr.size
#define Name(i)		attr_name(entries[i].attr)

/* ---------------------------------------------------------------------- */

/* super_blocks list is not exported */
static DEFINE_MUTEX(aufs_sbilist_mtx);
static LIST_HEAD(aufs_sbilist);

#ifdef DbgDlgt
int is_branch(struct super_block *h_sb)
{
	int found = 0;
	struct aufs_sbinfo *sbinfo;

	//Dbg("here\n");
	mutex_lock(&aufs_sbilist_mtx);
	list_for_each_entry(sbinfo, &aufs_sbilist, si_list) {
		aufs_bindex_t bindex, bend;
		struct super_block *sb;

		sb = sbinfo->si_mnt->mnt_sb;
		si_read_lock(sb);
		bend = sbend(sb);
		for (bindex = 0; !found && bindex <= bend; bindex++)
			found = (h_sb == sbr_sb(sb, bindex));
		si_read_unlock(sb);
	}
	mutex_unlock(&aufs_sbilist_mtx);
	return found;
}
#endif

/* ---------------------------------------------------------------------- */

static void free_entry(int i)
{
	MtxMustLock(&aufs_sbilist_mtx);
	DEBUG_ON(!Priv(i));

	if (Allocated(i) > 0)
		kfree(Priv(i));
	else
		free_pages((unsigned long)Priv(i), -Allocated(i));
	Priv(i) = NULL;
	Len(i) = 0;
}

static void free_entries(void)
{
	static int a[] = {Brs, -1};
	int *p = a;

	MtxMustLock(&aufs_sbilist_mtx);

	while (*p >= 0) {
		if (Priv(*p))
			free_entry(*p);
		p++;
	}
}

static int alloc_entry(int i)
{
	MtxMustLock(&aufs_sbilist_mtx);
	DEBUG_ON(Priv(i));
	//Dbg("%d\n", Allocated(i));

	if (Allocated(i) > 0)
		Priv(i) = kmalloc(Allocated(i), GFP_KERNEL);
	else
		Priv(i) = (void*)__get_free_pages(GFP_KERNEL, -Allocated(i));
	if (Priv(i))
		return 0;
	return -ENOMEM;
}

void add_sbilist(struct aufs_sbinfo *sbinfo)
{
	mutex_lock(&aufs_sbilist_mtx);
	list_add_tail(&sbinfo->si_list, &aufs_sbilist);
	free_entries();
	mutex_unlock(&aufs_sbilist_mtx);
}

void del_sbilist(struct aufs_sbinfo *sbinfo)
{
	mutex_lock(&aufs_sbilist_mtx);
	list_del(&sbinfo->si_list);
	free_entries();
	mutex_unlock(&aufs_sbilist_mtx);
}

void sysaufs_notify_remount(void)
{
	mutex_lock(&aufs_sbilist_mtx);
	free_entries();
	mutex_unlock(&aufs_sbilist_mtx);
}

/* ---------------------------------------------------------------------- */

static int make_brs(struct seq_file *seq)
{
	int err;
	struct aufs_sbinfo *sbinfo;

	TraceEnter();
	MtxMustLock(&aufs_sbilist_mtx);

	err = 0;
	list_for_each_entry(sbinfo, &aufs_sbilist, si_list) {
		struct super_block *sb;
		struct dentry *root;
		struct vfsmount *mnt;

		sb = sbinfo->si_mnt->mnt_sb;
		root = sb->s_root;
		aufs_read_lock(root, !AUFS_I_RLOCK);
		mnt = sbinfo->si_mnt;
		err = seq_escape
			(seq, mnt->mnt_devname ? mnt->mnt_devname : "none",
			 au_esc_chars);
		if (!err)
			err = seq_putc(seq, ' ');
		if (!err)
			err = seq_path(seq, mnt, root, au_esc_chars);
		if (err > 0)
			err = seq_printf(seq, " %p br:", sb);
		if (!err)
			err = au_show_brs(seq, sb);
		aufs_read_unlock(root, !AUFS_I_RLOCK);
		if (!err)
			err = seq_putc(seq, '\n');
		else
			break;
	}

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static int make_config(struct seq_file *seq)
{
	int err;

	TraceEnter();

#ifdef CONFIG_AUFS
	err = seq_puts(seq, "CONFIG_AUFS=y\n");
#else
	err = seq_puts(seq, "CONFIG_AUFS=m\n");
#endif

#define puts(m, v) \
	if (!err) err = seq_puts(seq, "CONFIG_AUFS_" #m "=" #v "\n")
#define puts_bool(m)	puts(m, y)
#define puts_mod(m)	puts(m, m)

#ifdef CONFIG_AUFS_FAKE_DM
	puts_bool(FAKE_DM);
#endif
#ifdef CONFIG_AUFS_BRANCH_MAX_127
	puts_bool(BRANCH_MAX_127);
#elif defined(CONFIG_AUFS_BRANCH_MAX_511)
	puts_bool(BRANCH_MAX_511);
#elif defined(CONFIG_AUFS_BRANCH_MAX_1023)
	puts_bool(BRANCH_MAX_1023);
#elif defined(CONFIG_AUFS_BRANCH_MAX_32767)
	puts_bool(BRANCH_MAX_32767);
#endif
	puts_bool(SYSAUFS);
#ifdef CONFIG_AUFS_HINOTIFY
	puts_bool(HINOTIFY);
#endif
#ifdef CONFIG_AUFS_EXPORT
	puts_bool(EXPORT);
#endif
#ifdef CONFIG_AUFS_AS_BRANCH
	puts_bool(AS_BRANCH);
#endif
#ifdef CONFIG_AUFS_DLGT
	puts_bool(DLGT);
#endif
#ifdef CONFIG_AUFS_LHASH_PATCH
	puts_bool(LHASH_PATCH);
#endif
#ifdef CONFIG_AUFS_KSIZE_PATCH
	puts_bool(KSIZE_PATCH);
#endif
#ifdef CONFIG_AUFS_DEBUG
	puts_bool(DEBUG);
#endif
#ifdef CONFIG_AUFS_COMPAT
	puts_bool(COMPAT);
#endif

#undef puts_bool
#undef puts

	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static int make_stat(struct seq_file *seq)
{
	int err, i;

	TraceEnter();
	err = seq_puts(seq, "wkq max_busy:");
	for (i = 0; !err && i < aufs_nwkq; i++)
		err = seq_printf(seq, " %u", au_wkq[i].max_busy);
	if (!err)
		err = seq_putc(seq, '\n');
	TraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static int make(int index, int (*make_index)(struct seq_file *seq))
{
	int err;
	struct seq_file *seq; // dirty

	TraceEnter();
	DEBUG_ON(Priv(index));
	MtxMustLock(&aufs_sbilist_mtx);

	err = -ENOMEM;
	seq = kzalloc(sizeof(*seq), GFP_KERNEL);
	if (unlikely(!seq))
		goto out;

	Len(index) = 0;
	while (1) {
		err = alloc_entry(index);
		if (unlikely(err))
			break;

		//mutex_init(&seq.lock);
		seq->buf = Priv(index);
		seq->count = 0;
		seq->size = Allocated(index);
		if (unlikely(Allocated(index) <= 0))
			seq->size = PAGE_SIZE << -Allocated(index);

		err = make_index(seq);
		if (!err) {
			Len(index) = seq->count;
			break; /* success */
		}

		free_entry(index);
		if (Allocated(index) > 0) {
			Allocated(index) <<= 1;
			if (unlikely(Allocated(index) >= PAGE_SIZE))
				Allocated(index) = 0;
		} else
			Allocated(index)--;
		//Dbg("%d\n", Allocated(index));
	}
	kfree(seq);

 out:
	TraceErr(err);
	return err;
}

/* why does sysfs pass my parent kobject? */
static struct dentry *find_me(struct dentry *parent, int index)
{
#if 1
	struct dentry *dentry;
	const char *name = Name(index);
	const int len = strlen(name);

	// no lock
	//Dbg("%.*s\n", DLNPair(parent));
	list_for_each_entry(dentry, &parent->d_subdirs, D_CHILD) {
		//Dbg("%.*s\n", DLNPair(dentry));
		if (len == dentry->d_name.len
		    && !strcmp(dentry->d_name.name, name))
			return dentry;
	}
#endif
	return NULL;
}

static ssize_t sysaufs_read(struct kobject *kobj, char *buf, loff_t offset,
			    size_t sz, int index)
{
	ssize_t err;
	loff_t len;
	struct dentry *d;

	LKTRTrace("%d, offset %Ld, sz %u\n", index, offset, sz);

	if (unlikely(!sz))
		return 0;

	err = 0;
	mutex_lock(&aufs_sbilist_mtx);
	if (unlikely(!Priv(index))) {
		int do_size = 1;
		switch (index) {
		case Brs:
			if (!list_empty(&aufs_sbilist))
				err = make(index, make_brs);
			break;
		case Config:
			err = make(index, make_config);
			break;
		case Stat:
			do_size = 0;
			err = make(index, make_stat);
			break;
		default:
			BUG();
		}

		if (do_size) {
			d = find_me(kobj->dentry, index);
			if (d)
				i_size_write(d->d_inode, Len(index));
		}
	}

	if (!err) {
		err = len = Len(index) - offset;
		LKTRTrace("%Ld\n", len);
		if (len > 0) {
			if (err > sz)
				err = sz;
			memcpy(buf, Priv(index) + offset, err);
		}

		switch (index) {
		case Stat:
			free_entry(index);
		}
	}
	mutex_unlock(&aufs_sbilist_mtx);

	TraceErr(err);
	return err;
}

static ssize_t sysaufs_free_write(struct kobject *kobj, char *buf,
				  loff_t offset, size_t sz, int index)
{
	struct dentry *d;
	int allocated, len;

	LKTRTrace("%d\n", index);

	mutex_lock(&aufs_sbilist_mtx);
	if (Priv(index)) {
		allocated = Allocated(index);
		if (unlikely(allocated <= 0))
			allocated = PAGE_SIZE << -allocated;
		allocated >>= 1;
		len = Len(index);

		free_entry(index);
		if (unlikely(len <= allocated)) {
			if (Allocated(index) > 0)
				Allocated(index) >>= 1;
			else if (!Allocated(index))
				Allocated(index) = PAGE_SIZE >> 1;
			else
				Allocated(index)++;
		}

		d = find_me(kobj->dentry, index);
		if (d)
			i_size_write(d->d_inode, 0);
	}
	mutex_unlock(&aufs_sbilist_mtx);
	return sz;
}

/* ---------------------------------------------------------------------- */

static decl_subsys(aufs, NULL, NULL);
void sysaufs_fin(void)
{
	int i;
	mutex_lock(&aufs_sbilist_mtx);
	for (i = 0; i < NEntries; i++)
		if (!entries[i].err) {
			sysfs_remove_bin_file(&aufs_subsys.kset.kobj,
					      &entries[i].attr);
			if (Priv(i))
				free_entry(i);
		}
	mutex_unlock(&aufs_sbilist_mtx);
	subsystem_unregister(&aufs_subsys);
	subsys_put(&fs_subsys);
}

int __init sysaufs_init(void)
{
	int err, i;

	subsys_get(&fs_subsys);
	kset_set_kset_s(&aufs_subsys, fs_subsys);
	err = subsystem_register(&aufs_subsys);
	if (unlikely(err))
		goto out;

	for (i = 0; !err && i < NEntries; i++) {
		switch (i) {
		case Brs:
			if (!sysaufs_brs)
				continue;
		}
		err = entries[i].err
			= sysfs_create_bin_file(&aufs_subsys.kset.kobj,
						&entries[i].attr);
	}

	if (unlikely(err))
		sysaufs_fin();

 out:
	TraceErr(err);
	return err;
}
