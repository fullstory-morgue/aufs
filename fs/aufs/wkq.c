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

/* $Id: wkq.c,v 1.8 2007/03/27 12:50:54 sfjro Exp $ */

#include <linux/kthread.h>
#include "aufs.h"

struct au_wkq *au_wkq;

struct au_cred {
#ifdef CONFIG_AUFS_DLGT
	uid_t fsuid;
	gid_t fsgid;
	kernel_cap_t cap_effective, cap_inheritable, cap_permitted;
	//unsigned keep_capabilities:1;
	//struct user_struct *user;
	//struct fs_struct *fs;
	//struct nsproxy *nsproxy;
#endif
};

struct au_wkinfo {
	struct work_struct wk;

	int dlgt; // can be a bit
	struct au_cred cred;

	au_wkq_func_t func;
	void *args;

	atomic_t *busyp;
	struct completion *comp;
};

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_DLGT
static void cred_store(struct au_cred *cred)
{
	cred->fsuid = current->fsuid;
	cred->fsgid = current->fsgid;
	cred->cap_effective = current->cap_effective;
	cred->cap_inheritable = current->cap_inheritable;
	cred->cap_permitted = current->cap_permitted;
}

static void cred_revert(struct au_cred *cred)
{
	DEBUG_ON(!au_is_kthread(current));
	current->fsuid = cred->fsuid;
	current->fsgid = cred->fsgid;
	current->cap_effective = cred->cap_effective;
	current->cap_inheritable = cred->cap_inheritable;
	current->cap_permitted = cred->cap_permitted;
}

static void cred_switch(struct au_cred *old, struct au_cred *new)
{
	cred_store(old);
	cred_revert(new);
}
#endif

/* ---------------------------------------------------------------------- */

static int enqueue(struct au_wkq *wkq, struct au_wkinfo *wkinfo)
{
	unsigned int v, cur;

	wkinfo->busyp = &wkq->busy;
	v = atomic_read(wkinfo->busyp);
	cur = wkq->max_busy;
	if (unlikely(cur < v))
		(void)cmpxchg(&wkq->max_busy, cur, v);
	return !queue_work(wkq->q, &wkinfo->wk);
}

static void do_wkq(struct au_wkinfo *wkinfo)
{
	unsigned int idle, n;
	int i, idle_idx;

	TraceEnter();

	while (1) {
		idle_idx = 0;
		idle = UINT_MAX;
		for (i = 0; i < aufs_nwkq; i++) {
			n = atomic_inc_return(&au_wkq[i].busy);
			if (n == 1 && !enqueue(au_wkq + i, wkinfo))
				return; /* success */

			if (n < idle) {
				idle_idx = i;
				idle = n;
			}
			atomic_dec(&au_wkq[i].busy);
		}

		atomic_inc(&au_wkq[idle_idx].busy);
		if (!enqueue(au_wkq + idle_idx, wkinfo))
			return; /* success */

		/* impossible? */
		Warn1("failed to queue_work()\n");
		yield();
	}
}

static AuWkqFunc(wkq_func, wk)
{
	struct au_wkinfo *wkinfo = container_of(wk, struct au_wkinfo, wk);

#ifdef CONFIG_AUFS_DLGT
	if (!wkinfo->dlgt)
		wkinfo->func(wkinfo->args);
	else {
		struct au_cred cred;
		cred_switch(&cred, &wkinfo->cred);
		wkinfo->func(wkinfo->args);
		cred_revert(&cred);
	}
#else
	wkinfo->func(wkinfo->args);
#endif
	atomic_dec(wkinfo->busyp);
	complete(wkinfo->comp);
}

void wkq_wait(au_wkq_func_t func, void *args, int dlgt)
{
	DECLARE_COMPLETION_ONSTACK(comp);
	struct au_wkinfo wkinfo = {
		.dlgt	= dlgt,
		.func	= func,
		.args	= args,
		.comp	= &comp
	};

	LKTRTrace("dlgt %d\n", dlgt);
	DEBUG_ON(au_is_kthread(current));

	AuInitWkq(&wkinfo.wk, wkq_func);
#ifdef CONFIG_AUFS_DLGT
	if (dlgt)
		cred_store(&wkinfo.cred);
#endif
	do_wkq(&wkinfo);
	wait_for_completion(wkinfo.comp);
}

void au_fin_wkq(void)
{
	int i;

	TraceEnter();

	for (i = 0; i < aufs_nwkq; i++)
		if (au_wkq[i].q && !IS_ERR(au_wkq[i].q))
			destroy_workqueue(au_wkq[i].q);
	kfree(au_wkq);
}

int __init au_init_wkq(void)
{
	int err, i;

	LKTRTrace("%d\n", aufs_nwkq);

	err = -ENOMEM;
	au_wkq = kcalloc(aufs_nwkq, sizeof(*au_wkq), GFP_KERNEL);
	if (unlikely(!au_wkq))
		goto out;

	err = 0;
	for (i = 0; i < aufs_nwkq; i++) {
		au_wkq[i].q = create_singlethread_workqueue(AUFS_WKQ_NAME);
		if (au_wkq[i].q && !IS_ERR(au_wkq[i].q)) {
			atomic_set(&au_wkq[i].busy, 0);
			au_wkq[i].max_busy = 0;
			continue;
		}

		err = PTR_ERR(au_wkq[i].q);
		au_fin_wkq();
		break;
	}

 out:
	TraceErr(err);
	return err;
}
