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

/* $Id: probe.h,v 1.1 2007/04/09 02:49:01 sfjro Exp $ */

#include <linux/kprobes.h>
#include <asm/ptrace.h>
#ifndef regs_return_value
#define regs_return_value(regs) ((regs)->eax)
#endif

#define HookJp(hook)	{.entry = JPROBE_ENTRY(hook)}
#define Hook(func)	{.funcname = #func, .jp = HookJp(func##_hook)}
struct hook {
	char *funcname;
	struct jprobe jp;
};

#define HookRp(hook)	{.handler = hook}
#define RHook(func)	{.funcname = #func, .rp = HookRp(func##_rhook)}
struct rhook {
	char *funcname;
	struct kretprobe rp;
};

#define RpFuncInt(func) \
static int func##_rhook(struct kretprobe_instance *ri, struct pt_regs *regs) \
{ \
	long ret = regs_return_value(regs); \
	if (ret) Dbg("ret %ld\n", ret); \
	return 0; \
}

int __init probe_init(struct hook *hooks, struct rhook *rhooks);
void __exit probe_exit(struct hook *hooks, struct rhook *rhooks);

#define LNPair(qstr)		(qstr)->len,(qstr)->name
#define DLNPair(d)		LNPair(&(d)->d_name)
#ifndef Dbg
#define Dbg(fmt, arg...) \
	printk(KERN_DEBUG "%s:%d:%s[%d]: " fmt, \
	       __func__, __LINE__, current->comm, current->pid, ##arg)
#endif
