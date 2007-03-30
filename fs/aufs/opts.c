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

/* $Id: opts.c,v 1.29 2007/03/27 12:50:10 sfjro Exp $ */

#include <asm/types.h> // a distribution requires
#include <linux/parser.h>
#include "aufs.h"

enum {
	Opt_br,
	Opt_add, Opt_del, Opt_mod, Opt_append, Opt_prepend,
	Opt_idel, Opt_imod,
	Opt_dirwh, Opt_rdcache, Opt_deblk, Opt_nhash,
	Opt_xino, Opt_zxino, Opt_noxino,
	Opt_plink, Opt_noplink, Opt_list_plink, Opt_clean_plink,
	Opt_udba,
	Opt_diropq_a, Opt_diropq_w,
	Opt_warn_perm, Opt_nowarn_perm,
	Opt_findrw_dir, Opt_findrw_br,
	Opt_coo,
	Opt_dlgt, Opt_nodlgt,
	Opt_tail, Opt_ignore, Opt_err
};

static match_table_t options = {
	{Opt_br, "br=%s"},
	{Opt_br, "br:%s"},

	{Opt_add, "add=%d:%s"},
	{Opt_add, "add:%d:%s"},
	{Opt_add, "ins=%d:%s"},
	{Opt_add, "ins:%d:%s"},
	{Opt_append, "append=%s"},
	{Opt_append, "append:%s"},
	{Opt_prepend, "prepend=%s"},
	{Opt_prepend, "prepend:%s"},

	{Opt_del, "del=%s"},
	{Opt_del, "del:%s"},
	{Opt_idel, "idel:%d"},
	{Opt_mod, "mod=%s"},
	{Opt_mod, "mod:%s"},
	{Opt_imod, "imod:%d:%s"},

	{Opt_dirwh, "dirwh=%d"},
	{Opt_dirwh, "dirwh:%d"},

	{Opt_xino, "xino=%s"},
	{Opt_xino, "xino:%s"},
	{Opt_noxino, "noxino"},
	//{Opt_zxino, "zxino=%s"},

#if LINUX_VERSION_CODE != KERNEL_VERSION(2,6,15)
	{Opt_plink, "plink"},
	{Opt_noplink, "noplink"},
#ifdef CONFIG_AUFS_DEBUG
	{Opt_list_plink, "list_plink"},
#endif
	{Opt_clean_plink, "clean_plink"},
#endif

	{Opt_udba, "udba=%s"},

	{Opt_diropq_a, "diropq=always"},
	{Opt_diropq_a, "diropq=a"},
	{Opt_diropq_w, "diropq=whiteouted"},
	{Opt_diropq_w, "diropq=w"},

	{Opt_warn_perm, "warn_perm"},
	{Opt_nowarn_perm, "nowarn_perm"},

#ifdef CONFIG_AUFS_DLGT
	{Opt_dlgt, "dlgt"},
	{Opt_nodlgt, "nodlgt"},
#endif

#if 0
	{Opt_findrw_dir, "findrw=dir"},
	{Opt_findrw_br, "findrw=br"},

	{Opt_coo, "coo=%s"},

	{Opt_rdcache, "rdcache=%d"},
	{Opt_rdcache, "rdcache:%d"},
	{Opt_deblk, "deblk=%d"},
	{Opt_deblk, "deblk:%d"},
	{Opt_nhash, "nhash=%d"},
	{Opt_nhash, "nhash:%d"},
#endif

	{Opt_br, "dirs=%s"},
	{Opt_ignore, "debug=%d"},
	{Opt_ignore, "delete=whiteout"},
	{Opt_ignore, "delete=all"},
	{Opt_ignore, "imap=%s"},

	{Opt_err, NULL}
};

/* ---------------------------------------------------------------------- */

#define RW	"rw"
#define RO	"ro"
#define WH	"wh"
#define RR	"rr"

static match_table_t brperms = {
	{AuBrPerm_RR, RR},
	{AuBrPerm_RO, RO},
	{AuBrPerm_RW, RW},

	{AuBrPerm_ROWH, RO WH},
	{AuBrPerm_ROWH, RO "+" WH},
	{AuBrPerm_RRWH, RR WH},
	{AuBrPerm_RRWH, RR "+" WH},

	{AuBrPerm_RO, "nfsro"},
	{AuBrPerm_RO, NULL}
};

static int br_perm_val(char *perm)
{
	int val;
	substring_t args[MAX_OPT_ARGS];

	LKTRTrace("perm %s\n", perm);
	val = AuBrPerm_RO;
	if (perm && *perm)
		val = match_token(perm, brperms, args);
	TraceErr(val);
	return val;
}

int br_perm_str(char *p, int len, int brperm)
{
	struct match_token *bp = brperms;

	LKTRTrace("len %d, 0x%x\n", len, brperm);

	while (bp->pattern) {
		if (bp->token == brperm) {
			if (strlen(bp->pattern) < len) {
				strcpy(p, bp->pattern);
				return 0;
			} else
				return -E2BIG;
		}
		bp++;
	}

	return -EIO;
}

/* ---------------------------------------------------------------------- */

static match_table_t udbalevel = {
	{AuFlag_UDBA_REVAL, "reval"},
#ifdef CONFIG_AUFS_HINOTIFY
	{AuFlag_UDBA_INOTIFY, "inotify"},
#endif
	{AuFlag_UDBA_NONE, "none"},
	{-1, NULL}
};

static int udba_val(char *str)
{
	substring_t args[MAX_OPT_ARGS];
	return match_token(str, udbalevel, args);
}

char *udba_str(int udba)
{
	struct match_token *p = udbalevel;
	while (p->pattern) {
		if (p->token == udba)
			return p->pattern;
		p++;
	}
	BUG();
	return "??";
}

static void udba_set(struct super_block *sb, unsigned int flg)
{
	au_flag_clr(sb, AuMask_UDBA);
	au_flag_set(sb, flg);
}

/* ---------------------------------------------------------------------- */

static match_table_t coolevel = {
	{AuFlag_COO_LEAF, "leaf"},
	{AuFlag_COO_ALL, "all"},
	{AuFlag_COO_NONE, "none"},
	{-1, NULL}
};

#if 0
static int coo_val(char *str)
{
	substring_t args[MAX_OPT_ARGS];
	return match_token(str, coolevel, args);
}
#endif

char *coo_str(int coo)
{
	struct match_token *p = coolevel;
	while (p->pattern) {
		if (p->token == coo)
			return p->pattern;
		p++;
	}
	BUG();
	return "??";
}
static void coo_set(struct super_block *sb, unsigned int flg)
{
	au_flag_clr(sb, AuMask_COO);
	au_flag_set(sb, flg);
}

/* ---------------------------------------------------------------------- */

static const int lkup_dirflags = LOOKUP_FOLLOW | LOOKUP_DIRECTORY;

#ifdef CONFIG_AUFS_DEBUG
static void dump_opts(struct opts *opts)
{
	struct opt_add *add;
	struct opt_del *del;
	struct opt_mod *mod;
	struct opt *opt;

	TraceEnter();
	if (opts->xino.path)
		LKTRTrace("xino {%s %.*s}\n",
			  opts->xino.path, DLNPair(opts->xino.file->f_dentry));

	opt = opts->opt;
	while (/* opt < opts_tail && */ opt->type != Opt_tail) {
		switch (opt->type) {
		case Opt_add:
			add = &opt->add;
			LKTRTrace("add {b%d, %s, 0x%x, %p}\n",
				  add->bindex, add->path, add->perm,
				  add->nd.dentry);
			break;
		case Opt_del:
		case Opt_idel:
			del = &opt->del;
			LKTRTrace("del {%s, %p}\n", del->path, del->h_root);
			break;
		case Opt_mod:
		case Opt_imod:
			mod = &opt->mod;
			LKTRTrace("mod {%s, 0x%x, %p}\n",
				  mod->path, mod->perm, mod->h_root);
			break;
		case Opt_append:
			add = &opt->add;
			LKTRTrace("append {b%d, %s, 0x%x, %p}\n",
				  add->bindex, add->path, add->perm,
				  add->nd.dentry);
			break;
		case Opt_prepend:
			add = &opt->add;
			LKTRTrace("prepend {b%d, %s, 0x%x, %p}\n",
				  add->bindex, add->path, add->perm,
				  add->nd.dentry);
			break;
		case Opt_dirwh:
			LKTRTrace("dirwh %d\n", opt->dirwh);
			break;
		case Opt_rdcache:
			LKTRTrace("rdcache %d\n", opt->rdcache);
			break;
		case Opt_noxino:
			LKTRLabel(noxino);
			break;
		case Opt_plink:
			LKTRLabel(plink);
			break;
		case Opt_noplink:
			LKTRLabel(noplink);
			break;
		case Opt_list_plink:
			LKTRLabel(list_plink);
			break;
		case Opt_clean_plink:
			LKTRLabel(clean_plink);
			break;
		case Opt_udba:
			LKTRTrace("udba %d, %s\n",
				  opt->udba, udba_str(opt->udba));
			break;
		case Opt_diropq_a:
			LKTRLabel(diropq_a);
			break;
		case Opt_diropq_w:
			LKTRLabel(diropq_w);
			break;
		case Opt_warn_perm:
			LKTRLabel(warn_perm);
			break;
		case Opt_nowarn_perm:
			LKTRLabel(nowarn_perm);
			break;
		case Opt_dlgt:
			LKTRLabel(dlgt);
			break;
		case Opt_nodlgt:
			LKTRLabel(nodlgt);
			break;
		case Opt_coo:
			LKTRTrace("coo %d, %s\n", opt->coo, coo_str(opt->coo));
			break;
		default:
			BUG();
		}
		opt++;
	}
}
#else
#define dump_opts(opts) /* */
#endif

void au_free_opts(struct opts *opts)
{
	struct opt *opt;

	TraceEnter();

	if (opts->xino.file)
		fput(opts->xino.file);

	opt = opts->opt;
	while (opt->type != Opt_tail) {
		switch (opt->type) {
		case Opt_add:
		case Opt_append:
		case Opt_prepend:
			path_release(&opt->add.nd);
			break;
		case Opt_del:
		case Opt_idel:
			dput(opt->del.h_root);
			break;
		case Opt_mod:
		case Opt_imod:
			dput(opt->mod.h_root);
			break;
		}
		opt++;
	}
}

static int opt_add(struct opt *opt, char *opt_str, struct super_block *sb,
		   aufs_bindex_t bindex)
{
	int err;
	struct opt_add *add = &opt->add;
	char *p;

	LKTRTrace("%s, b%d\n", opt_str, bindex);

	add->bindex = bindex;
	add->perm = AuBrPerm_RO;
	if (!bindex && !(sb->s_flags & MS_RDONLY))
		add->perm = AuBrPerm_RW;
#ifdef CONFIG_AUFS_COMPAT
	add->perm = AuBrPerm_RW;
#endif
	add->path = opt_str;
	p = strchr(opt_str, '=');
	if (p) {
		*p++ = 0;
		add->perm = br_perm_val(p);
		//add->perm = 0;
		if (unlikely(!add->perm)) {
			err = -EINVAL;
			Err("bad branch permission %s\n", p);
			goto out;
		}
	}

	// LSM may detect it
	// do not superio.
	err = path_lookup(add->path, lkup_dirflags, &add->nd);
	//err = -1;
	if (!err) {
		opt->type = Opt_add;
		goto out;
	}
	Err("lookup failed %s (%d)\n", add->path, err);
	err = -EINVAL;

 out:
	TraceErr(err);
	return err;
}

/* called without aufs lock */
int au_parse_opts(struct super_block *sb, char *str, struct opts *opts,
		  int remount)
{
	int err, n;
	struct dentry *root;
	struct opt *opt, *opt_tail;
	char *opt_str;
	substring_t args[MAX_OPT_ARGS];
	aufs_bindex_t bindex;
	struct nameidata nd;
	struct opt_del *del;
	struct opt_mod *mod;
	struct opt_xino *xino;
	struct file *file;

	LKTRTrace("%s, remount %d, nopts %d\n", str, remount, opts->max_opt);
	xino = &opts->xino;
	DEBUG_ON(xino->path || xino->file);

	root = sb->s_root;
	err = 0;
	bindex = 0;
	opt = opts->opt;
	opt_tail = opt + opts->max_opt - 1;
	opt->type = Opt_tail;
	while (!err && (opt_str = strsep(&str, ",")) && *opt_str) {
		int token, skipped;
		char *p;
		err = -EINVAL;
		token = match_token(opt_str, options, args);
		LKTRTrace("%s, token %d, args[0]{%p, %p}\n",
			  opt_str, token, args[0].from, args[0].to);

		skipped = 0;
		switch (token) {
		case Opt_br:
			err = 0;
			while (!err && (opt_str = strsep(&args[0].from, ":"))
			       && *opt_str) {
				err = opt_add(opt, opt_str, sb, bindex++);
				//if (LktrCond) err = -1;
				if (unlikely(!err && ++opt > opt_tail)) {
					err = -E2BIG;
					break;
				}
				opt->type = Opt_tail;
				skipped = 1;
			}
			break;
		case Opt_add:
			if (unlikely(match_int(&args[0], &n))) {
				Err("bad integer in %s\n", opt_str);
				break;
			}
			bindex = n;
			err = opt_add(opt, args[1].from, sb, bindex);
			break;
		case Opt_append:
		case Opt_prepend:
			err = opt_add(opt, args[0].from, sb, /*dummy bindex*/1);
			if (!err)
				opt->type = token;
			break;
		case Opt_del:
			del = &opt->del;
			del->path = args[0].from;
			LKTRTrace("del path %s\n", del->path);
			// LSM may detect it
			// do not superio.
			err = path_lookup(del->path, lkup_dirflags, &nd);
			if (unlikely(err)) {
				Err("lookup failed %s (%d)\n", del->path, err);
				break;
			}
			del->h_root = dget(nd.dentry);
			path_release(&nd);
			opt->type = token;
			break;
#if 0
		case Opt_idel:
			del = &opt->del;
			del->path = "(indexed)";
			if (unlikely(match_int(&args[0], &n))) {
				Err("bad integer in %s\n", opt_str);
				break;
			}
			bindex = n;
			aufs_read_lock(root, !AUFS_I_RLOCK);
			if (bindex < 0 || sbend(sb) < bindex) {
				Err("out of bounds, %d\n", bindex);
				aufs_read_unlock(root, !AUFS_I_RLOCK);
				break;
			}
			err = 0;
			del->h_root = dget(au_h_dptr_i(root, bindex));
			opt->type = token;
			aufs_read_unlock(root, !AUFS_I_RLOCK);
			break;
#endif

		case Opt_mod:
			mod = &opt->mod;
			mod->path = args[0].from;
			p = strchr(mod->path, '=');
			if (unlikely(!p)) {
				Err("no permssion %s\n", opt_str);
				break;
			}
			*p++ = 0;
			mod->perm = br_perm_val(p);
			LKTRTrace("mod path %s, perm 0x%x, %s\n",
				  mod->path, mod->perm, p);
			if (!mod->perm) {
				Err("bad permssion %s\n", p);
				break;
			}
			// LSM may detect it
			// do not superio.
			err = path_lookup(mod->path, lkup_dirflags, &nd);
			if (unlikely(err)) {
				Err("lookup failed %s (%d)\n", mod->path, err);
				break;
			}
			mod->h_root = dget(nd.dentry);
			path_release(&nd);
			opt->type = token;
			break;
#if 0
		case Opt_imod:
			mod = &opt->mod;
			mod->path = "(indexed)";
			if (unlikely(match_int(&args[0], &n))) {
				Err("bad integer in %s\n", opt_str);
				break;
			}
			bindex = n;
			aufs_read_lock(root, !AUFS_I_RLOCK);
			if (bindex < 0 || sbend(sb) < bindex) {
				Err("out of bounds, %d\n", bindex);
				aufs_read_unlock(root, !AUFS_I_RLOCK);
				break;
			}
			mod->perm = br_perm_val(args[1].from);
			LKTRTrace("mod path %s, perm 0x%x, %s\n",
				  mod->path, mod->perm, args[1].from);
			if (!mod->perm) {
				Err("bad permssion %s\n", args[1].from);
				aufs_read_unlock(root, !AUFS_I_RLOCK);
				break;
			}
			err = 0;
			mod->h_root = dget(au_h_dptr_i(root, bindex));
			opt->type = token;
			aufs_read_unlock(root, !AUFS_I_RLOCK);
			break;
#endif

		case Opt_xino:
			skipped = 1;
			if (xino->file)
				fput(xino->file);
			xino->file = NULL;
			xino->path = NULL;
			file = xino_create(sb, args[0].from, /*silent*/0,
					   /*parent*/NULL);
			err = PTR_ERR(file);
			if (IS_ERR(file))
				break;
			err = -EINVAL;
			if (unlikely(file->f_dentry->d_sb == sb)) {
				fput(file);
				Err("%s must be outside\n", args[0].from);
				break;
			}
			err = 0;
			xino->file = file;
			xino->path = args[0].from;
			break;

		case Opt_noxino:
			if (xino->file)
				fput(xino->file);
			xino->file = NULL;
			xino->path = NULL;
			err = 0;
			opt->type = token;
			break;

		case Opt_dirwh:
			if (unlikely(match_int(&args[0], &opt->dirwh)))
				break;
			err = 0;
			opt->type = token;
			break;

		case Opt_rdcache:
			if (unlikely(match_int(&args[0], &opt->rdcache)))
				break;
			err = 0;
			opt->type = token;
			break;

		case Opt_plink:
		case Opt_noplink:
		case Opt_list_plink:
		case Opt_clean_plink:
		case Opt_diropq_a:
		case Opt_diropq_w:
		case Opt_warn_perm:
		case Opt_nowarn_perm:
		case Opt_dlgt:
		case Opt_nodlgt:
			err = 0;
			opt->type = token;
			break;

		case Opt_udba:
			opt->udba = udba_val(args[0].from);
			if (opt->udba >= 0) {
				err = 0;
				opt->type = token;
			}
			break;

#if 0
		case Opt_coo:
			opt->coo = coo_val(args[0].from);
			if (opt->coo >= 0) {
				err = 0;
				opt->type = token;
			}
			break;
#endif

		case Opt_ignore:
#ifndef CONFIG_AUFS_COMPAT
			Warn("ignored %s\n", opt_str);
#endif
			skipped = 1;
			err = 0;
			break;
		case Opt_err:
			Err("unknown option %s\n", opt_str);
			break;
		}

		if (!err && !skipped) {
			if (unlikely(++opt > opt_tail)) {
				err = -E2BIG;
				opt--;
				opt->type = Opt_tail;
				break;
			}
			opt->type = Opt_tail;
		}
	}

	dump_opts(opts);
	if (unlikely(err))
		au_free_opts(opts);
	TraceErr(err);
	return err;
}

int au_do_opts(struct super_block *sb, struct opts *opts, int remount)
{
	int err, do_sigen;
	struct opt *opt;
	struct aufs_sbinfo *sbinfo;
	struct inode *dir;
	unsigned int udba;

	TraceEnter();
	SiMustWriteLock(sb);
	DiMustWriteLock(sb->s_root);
	dir = sb->s_root->d_inode;
	IiMustWriteLock(dir);

	/* disable inotify temporary */
	udba = au_flag_test(sb, AuMask_UDBA);
	if (unlikely(udba == AuFlag_UDBA_INOTIFY && ibend(dir) >= 0)) {
		udba_set(sb, AuFlag_UDBA_NONE);
		au_reset_hinotify(dir, au_hi_flags(dir, 1) & ~AUFS_HI_XINO);
	}

	err = 0;
	do_sigen = 0;
	if (opts->xino.path) {
		err = xino_set(sb, &opts->xino, remount);
		if (unlikely(err))
			goto out;
		do_sigen++;
	}

	sbinfo = stosi(sb);
	opt = opts->opt;
	while (!err && opt->type != Opt_tail) {
		switch (opt->type) {
		case Opt_add:
			err = br_add(sb, &opt->add, remount);
			if (!err)
				do_sigen++;
			break;
		case Opt_del:
		case Opt_idel:
			err = br_del(sb, &opt->del, remount);
			if (!err)
				do_sigen++;
			break;
		case Opt_mod:
		case Opt_imod:
			err = br_mod(sb, &opt->mod, remount);
			if (err > 0) {
				do_sigen++;
				err = 0;
			}
			break;
		case Opt_append:
			opt->add.bindex = sbend(sb) + 1;
			err = br_add(sb, &opt->add, remount);
			if (!err)
				do_sigen++;
			break;
		case Opt_prepend:
			opt->add.bindex = 0;
			err = br_add(sb, &opt->add, remount);
			if (!err)
				do_sigen++;
			break;

		case Opt_dirwh:
			sbinfo->si_dirwh = opt->dirwh;
			break;

		case Opt_rdcache:
			sbinfo->si_rdcache = opt->rdcache * HZ;
			break;

		case Opt_noxino:
			err = xino_clr(sb);
			break;

		case Opt_plink:
			au_flag_set(sb, AuFlag_PLINK);
			break;
		case Opt_noplink:
			if (au_flag_test(sb, AuFlag_PLINK))
				au_put_plink(sb);
			au_flag_clr(sb, AuFlag_PLINK);
			break;
		case Opt_list_plink:
			if (au_flag_test(sb, AuFlag_PLINK))
				au_list_plink(sb);
			break;
		case Opt_clean_plink:
			if (au_flag_test(sb, AuFlag_PLINK))
				au_put_plink(sb);
			break;

		case Opt_udba:
			/* set it later */
			udba = opt->udba;
			do_sigen++;
			break;

		case Opt_diropq_a:
			au_flag_set(sb, AuFlag_ALWAYS_DIROPQ);
			break;
		case Opt_diropq_w:
			au_flag_clr(sb, AuFlag_ALWAYS_DIROPQ);
			break;

		case Opt_warn_perm:
			au_flag_set(sb, AuFlag_WARN_PERM);
			break;
		case Opt_nowarn_perm:
			au_flag_clr(sb, AuFlag_WARN_PERM);
			break;

		case Opt_dlgt:
			au_flag_set(sb, AuFlag_DLGT);
			break;
		case Opt_nodlgt:
			au_flag_clr(sb, AuFlag_DLGT);
			break;

		case Opt_coo:
			coo_set(sb, opt->coo);
			break;

		default:
			BUG();
			err = 0;
		}
		opt++;
	}

	if (unlikely(sbend(sb) >= 0
		     && !(sb->s_flags & MS_RDONLY)
		     && sbr_perm(sb, 0) != AuBrPerm_RW))
		Warn("first branch should be rw\n");

	if (do_sigen)
		au_sigen_inc(sb);

	udba_set(sb, udba);
	/* AUFS_HI_XINO will be handled later */
	if (unlikely(udba == AuFlag_UDBA_INOTIFY && ibend(dir) >= 0))
		au_reset_hinotify(dir, au_hi_flags(dir, 1) & ~AUFS_HI_XINO);

 out:
	TraceErr(err);
	return err;
}
