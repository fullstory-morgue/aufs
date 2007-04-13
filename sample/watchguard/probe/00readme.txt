
Sample modules for linux kprobe
Junjiro Okajima
$Id: 00readme.txt,v 1.1 2007/04/09 02:49:01 sfjro Exp $

Introduction
----------------------------------------
It's a simple kernel module which uses kprobe.
This module sets a kprobe hook to a kernel internal function, and
prints a debug log.
You need to enable CONFIG_KPROBES.
This sample supports linux-2.6.16 and later, but I didn't test them
all.

Samples
----------------------------------------
All kernel modules are prefixed by 'p', for example pcpup.ko.

o cpup
  sets a hook to aufs copy-up function. it prints all copying-up
  filename.
  you can edit $EXTRA_CFLAGS in cpup/Makefile.
o open
  this is not aufs specific. it prints all opening filename on every
  filesystem.

Build
----------------------------------------
Just 'make'.
You can edit $kdir in ./Makefile.

Enjoy!
