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

/* $Id: probe.c,v 1.1 2007/04/09 02:49:01 sfjro Exp $ */

#include <linux/kallsyms.h>
#include <linux/version.h>
#include "probe.h"

int __init probe_init(struct hook *hooks, struct rhook *rhooks)
{
	int err;
	struct hook *p;
	struct rhook *q;

	p = hooks;
	while (p->funcname) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
		p->jp.kp.symbol_name = p->funcname;
#else
		p->jp.kp.addr = (void*)kallsyms_lookup_name(p->funcname);
		BUG_ON(!p->jp.kp.addr);
#endif
		p++;
	}
	q = rhooks;
	while (q->funcname) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
		q->rp.kp.symbol_name = q->funcname;
#else
		q->rp.kp.addr = (void*)kallsyms_lookup_name(q->funcname);
		BUG_ON(!q->rp.kp.addr);
#endif
		q++;
	}

	err = 0;
	p = hooks;
	while (p->funcname && !err) {
		err = register_jprobe(&p->jp);
		p++;
	}
	if (!err) {
		q = rhooks;
		while (q->funcname && !err) {
			err = register_kretprobe(&q->rp);
			q++;
		}
		if (!err)
			return 0;
		while (--q != rhooks)
			unregister_kretprobe(&q->rp);
	}

	while (--p != hooks)
		unregister_jprobe(&p->jp);
	return err;
}

void __exit probe_exit(struct hook *hooks, struct rhook *rhooks)
{
	struct hook *p;
	struct rhook *q;

	p = hooks;
	while (p->funcname) {
		unregister_jprobe(&p->jp);
		p++;
	}
	q = rhooks;
	while (q->funcname) {
		unregister_kretprobe(&q->rp);
		q++;
	}
}
