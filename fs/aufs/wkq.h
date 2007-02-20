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

/* $Id: wkq.h,v 1.4 2007/02/05 01:44:26 sfjro Exp $ */

#ifndef __AUFS_WKQ_H__
#define __AUFS_WKQ_H__

#ifdef __KERNEL__

#include <linux/sched.h>
#include <linux/workqueue.h>

typedef void (*aufs_wkq_func_t)(void *args);
struct aufs_wkinfo {
	struct completion comp;
	struct work_struct wk;
	aufs_wkq_func_t func;
	void *args;
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
#define AufsInitWkq(wk, func)	INIT_WORK(wk, func)
#define AufsWkqFunc(name, arg)	void name(struct work_struct *arg)
#else
typedef void (*work_func_t)(void *arg);
#define AufsInitWkq(wk, func)	INIT_WORK(wk, func, wk)
#define AufsWkqFunc(name, arg)	void name(void *arg)
#endif

#define is_kthread(tsk) ({!(tsk)->mm;})

void wkq_wait(aufs_wkq_func_t func, void *args);
int __init init_wkq(void);
void fin_wkq(void);

#endif /* __KERNEL__ */
#endif /* __AUFS_WKQ_H__ */
