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

/* $Id: wkq.h,v 1.6 2007/03/27 12:48:27 sfjro Exp $ */

#ifndef __AUFS_WKQ_H__
#define __AUFS_WKQ_H__

#ifdef __KERNEL__

#include <linux/sched.h>
#include <linux/workqueue.h>

struct au_wkq {
	struct workqueue_struct *q;

	/* accounting */
	atomic_t busy;
	unsigned int max_busy;
};

typedef void (*au_wkq_func_t)(void *args);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
#define AuInitWkq(wk, func)	INIT_WORK(wk, func)
#define AuWkqFunc(name, arg)	void name(struct work_struct *arg)
#else
typedef void (*work_func_t)(void *arg);
#define AuInitWkq(wk, func)	INIT_WORK(wk, func, wk)
#define AuWkqFunc(name, arg)	void name(void *arg)
#endif

void wkq_wait(au_wkq_func_t func, void *args, int dlgt);
void wkq_nowait(au_wkq_func_t func, void *args, int dlgt);
int __init au_init_wkq(void);
void au_fin_wkq(void);

#endif /* __KERNEL__ */
#endif /* __AUFS_WKQ_H__ */
