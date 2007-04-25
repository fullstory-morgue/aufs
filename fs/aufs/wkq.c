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

/* $Id: wkq.c,v 1.12 2007/04/23 00:58:37 sfjro Exp $ */

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

	unsigned int wait:1;
	unsigned int dlgt:1;
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
	DEBUG_ON(!is_aufsd(current));
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

static void update_busy(struct au_wkq *wkq, struct au_wkinfo *wkinfo)
{
#ifdef CONFIG_AUFS_SYSAUFS
	unsigned int new, old;

	do {
		new = atomic_read(wkinfo->busyp);
		old = wkq->max_busy;
		if (new <= old)
			break;
	} while (cmpxchg(&wkq->max_busy, old, new) == old);
#endif
}

static int enqueue(struct au_wkq *wkq, struct au_wkinfo *wkinfo)
{
	wkinfo->busyp = &wkq->busy;
	update_busy(wkq, wkinfo);
	if (wkinfo->wait)
		return !queue_work(wkq->q, &wkinfo->wk);
	else
		return !schedule_work(&wkinfo->wk);
}

static void do_wkq(struct au_wkinfo *wkinfo)
{
	unsigned int idle, n;
	int i, idle_idx;

	TraceEnter();

	while (1) {
		if (wkinfo->wait) {
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
		} else
			idle_idx = aufs_nwkq;

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

	LKTRTrace("wkinfo{%u, %u, %p, %p, %p}\n",
		  wkinfo->wait, wkinfo->dlgt, wkinfo->func, wkinfo->busyp,
		  wkinfo->comp);
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
	if (wkinfo->wait)
		complete(wkinfo->comp);
	else
		kfree(wkinfo);
}

void au_wkq_run(au_wkq_func_t func, void *args, int dlgt, int do_wait)
{
	DECLARE_COMPLETION_ONSTACK(comp);
	struct au_wkinfo _wkinfo = {
		.wait	= 1,
		.dlgt	= !!dlgt,
		.func	= func,
		.args	= args,
		.comp	= &comp
	}, *wkinfo = &_wkinfo;

	LKTRTrace("dlgt %d, do_wait %d\n", dlgt, do_wait);
	DEBUG_ON(is_aufsd(current));

	if (unlikely(!do_wait)) {
		static DECLARE_WAIT_QUEUE_HEAD(wq);
		/*
		 * wkq_func() must free this wkinfo.
		 * it highly depends upon the implementation of workqueue.
		 */
		wait_event(wq, (wkinfo = kmalloc(sizeof(*wkinfo), GFP_KERNEL)));
		wkinfo->wait = 0;
		wkinfo->dlgt = !!dlgt;
		wkinfo->func = func;
		wkinfo->args = args;
		wkinfo->comp = NULL;
	}

	AuInitWkq(&wkinfo->wk, wkq_func);
#ifdef CONFIG_AUFS_DLGT
	if (dlgt)
		cred_store(&wkinfo->cred);
#endif
	do_wkq(wkinfo);
	if (do_wait)
		wait_for_completion(wkinfo->comp);
}

void au_wkq_fin(void)
{
	int i;

	TraceEnter();

	for (i = 0; i < aufs_nwkq; i++)
		if (au_wkq[i].q && !IS_ERR(au_wkq[i].q))
			destroy_workqueue(au_wkq[i].q);
	kfree(au_wkq);
}

int __init au_wkq_init(void)
{
	int err, i;
	struct au_wkq *nowaitq;

	LKTRTrace("%d\n", aufs_nwkq);

	/* '+1' is for accounting  of nowait queue */
	err = -ENOMEM;
	au_wkq = kcalloc(aufs_nwkq + 1, sizeof(*au_wkq), GFP_KERNEL);
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
		au_wkq_fin();
		break;
	}

	/* nowait accounting */
	nowaitq = au_wkq + aufs_nwkq;
	atomic_set(&nowaitq->busy, 0);
	nowaitq->max_busy = 0;
	nowaitq->q = NULL;

#if 0 // test accouting
	if (!err) {
		static void f(void *args) {
			DbgSleep(1);
		}
		int i;
		//au_debug_on();
		LKTRTrace("f %p\n", f);
		for (i = 0; i < 10; i++)
			au_wkq_nowait(f, NULL, 0);
		for (i = 0; i < aufs_nwkq; i++)
			au_wkq_wait(f, NULL, 0);
		DbgSleep(11);
		//au_debug_off();
	}
#endif

 out:
	TraceErr(err);
	return err;
}
