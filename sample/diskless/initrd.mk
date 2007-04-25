
# aufs sample -- diskless system

# Copyright (C) 2006, 2007 Junjiro Okajima
#
# This program, aufs is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# $Id: initrd.mk,v 1.4 2007/04/23 00:59:51 sfjro Exp $

all:
	false

busybox:
	aptitude download ${BusyBoxPkg} > /dev/null
	ls -1t ${BusyBoxPkg}_*.deb \
	| head -1 \
	| xargs -i ar -p {} data.tar.gz \
	| tar -C /tmp -xpzf - ./bin/$@
	mv /tmp/bin/$@ .

initrd.dir: busybox
	mkdir $@
	cd $@ && mkdir aufs branch bin dev initrd proc tmp
	cd $@/branch && mkdir -p ${BranchDirs} loop
	sudo mknod $@/dev/console c 5 1
	sudo mknod $@/dev/null c 1 3
	sudo mknod $@/dev/ttyS0 c 4 64
	for i in `seq 0 31`; \
	do sudo mknod $@/dev/loop$$i b 7 $$i; mkdir $@/branch/loop/$$i; done
	sudo mknod $@/dev/cloop0 b 240 0
	sudo mknod $@/dev/lktr c 10 63
	for i in lktr lktr_exec; \
	do test -e $$i && cp -p $$i $@; \
	done
	cp -p busybox $@/bin
	ln $@/bin/busybox $@/bin/sh
	for i in ${InitrdExtCmd}; \
	do f=`which $$i`; echo $$f; ldd $$f | cut -f2 -d'>' | cut -f2 -d' '; \
	done | sort | uniq | xargs -ri cp -p --parents {} $@

initrd.dir/%: %
	cp -pu $< $@

initrd: initrd.dir $(addprefix initrd.dir/, linuxrc)
	sudo chown -R root:root $@.dir/*
	mkcramfs $@.dir $@
	sudo chown -R ${USER} $@.dir/*
initramfs: initrd
	cd initrd.dir && \
	find . | \
	cpio --quiet -o -H newc > ../$@
#	cpio --quiet -c -o > ../$@
initramfs.gz: initramfs
	gzip -9 $<
