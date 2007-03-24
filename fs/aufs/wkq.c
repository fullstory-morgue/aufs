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

/* $Id: wkq.c,v 1.7 2007/03/19 04:32:35 sfjro Exp $ */

/*
 * Occasionally aufs needs to get superuser privilege to access branches,
 * which is called 'superio' locally.
 * Anciently, aufs changed 'current->fsuid' to zero temporary with disabling
 * some capabilities (CAP_xxx). It runs fast, but had minor limitations around
 * disabled capabilities.
 * Now I decided to separate superio as a kernel thread.
 * It may be slower than ancient simple approach in some cases (rare),
 * and it may meet some system limitation in other cases (very rare),
 * but it gives no limitation to user processes.
 *
 * "create a thread every time" .vs. "thread pool", to implement superio.
 *
 * - create a short life thread every time when superio is needed
 * - wait for the completion of the superio thread
 * - the request to create the superio thread is handled by the thread 'kthread'
 * or
 * - prepare one or a few superio threads, as residential thread pool
 * - enqueue the superio request to the threads
 * - wait for the thread returns our result
 *
 * currently thread pool is chosen.
 */

#include <linux/kthread.h>
#include "aufs.h"

static struct workqueue_struct **wkq;
static atomic_t wkq_last = ATOMIC_INIT(0);

struct aufs_cred {
	uid_t fsuid;
	//gid_t fsgid;
	kernel_cap_t cap_effective, cap_inheritable, cap_permitted;
	//unsigned keep_capabilities:1;
	//struct user_struct *user;
	//struct fs_struct *fs;
	//struct nsproxy *nsproxy;

};

struct aufs_wkinfo {
	struct completion comp;
	struct work_struct wk;
	aufs_wkq_func_t func;
	void *args;
	int dlgt; // can be a bit
	struct aufs_cred cred;
};

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_DLGT
static void cred_store(struct aufs_cred *cred)
{
	cred->fsuid = current->fsuid;
	cred->cap_effective = current->cap_effective;
	cred->cap_inheritable = current->cap_inheritable;
	cred->cap_permitted = current->cap_permitted;
}

static void cred_revert(struct aufs_cred *cred)
{
	DEBUG_ON(!is_kthread(current));
	current->fsuid = cred->fsuid;
	current->cap_effective = cred->cap_effective;
	current->cap_inheritable = cred->cap_inheritable;
	current->cap_permitted = cred->cap_permitted;
}

static void cred_switch(struct aufs_cred *old, struct aufs_cred *new)
{
	cred_store(old);
	cred_revert(new);
}
#endif

/* ---------------------------------------------------------------------- */

static void do_wkq(struct work_struct *wk)
{
	unsigned int idx;

	TraceEnter();

	// todo: busy thread bitmap and find_first_bit()
	// lazy round-robin
	//smp_mb();
	while (1) {
		idx = atomic_inc_return(&wkq_last);
		idx %= aufs_nwkq;
		if (queue_work(wkq[idx], wk))
			break;
		Warn("failed to queue_work()\n");
	}
}

static AufsWkqFunc(wkq_func, wk)
{
	struct aufs_wkinfo *wkinfo = container_of(wk, struct aufs_wkinfo, wk);

#ifdef CONFIG_AUFS_DLGT
	if (!wkinfo->dlgt)
		wkinfo->func(wkinfo->args);
	else {
		struct aufs_cred cred;
		cred_switch(&cred, &wkinfo->cred);
		wkinfo->func(wkinfo->args);
		cred_revert(&cred);
	}
#else
	wkinfo->func(wkinfo->args);
#endif
	complete(&wkinfo->comp);
}

void wkq_wait(aufs_wkq_func_t func, void *args, int dlgt)
{
	struct aufs_wkinfo wkinfo = {
		.func	= func,
		.args	= args,
		.dlgt	= dlgt
	};

	LKTRTrace("dlgt %d\n", dlgt);
	DEBUG_ON(is_kthread(current));

	AufsInitWkq(&wkinfo.wk, wkq_func);
	init_completion(&wkinfo.comp);
#ifdef CONFIG_AUFS_DLGT
	if (dlgt)
		cred_store(&wkinfo.cred);
#endif
	do_wkq(&wkinfo.wk);
	wait_for_completion(&wkinfo.comp);
}

void au_fin_wkq(void)
{
	int i;

	TraceEnter();

	for (i = 0; i < aufs_nwkq; i++)
		if (wkq[i] && !IS_ERR(wkq[i]))
			destroy_workqueue(wkq[i]);
	kfree(wkq);
}

int __init au_init_wkq(void)
{
	int err, i;

	LKTRTrace("%d\n", aufs_nwkq);

	err = -ENOMEM;
	wkq = kcalloc(aufs_nwkq, sizeof(*wkq), GFP_KERNEL);
	if (unlikely(!wkq))
		goto out;

	err = 0;
	for (i = 0; i < aufs_nwkq; i++) {
		wkq[i] = create_singlethread_workqueue(AUFS_WKQ_NAME);
		if (wkq[i] && !IS_ERR(wkq[i]))
			continue;

		err = PTR_ERR(wkq[i]);
		au_fin_wkq();
		break;
	}

 out:
	TraceErr(err);
	return err;
}
