
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

# $Id: retr.mk,v 1.2 2007/01/08 01:59:11 sfjro Exp $

# call me from main Makefile.
# retrieves all neccessary debian packages to ${DebPkgDir}.
# parsing debian installer, get $required and $base package names.

all:
	false

DebCache = var/cache/apt/archives

aptitude.sh:
	{ \
	echo '#!/bin/sh'; \
	echo 'HOME=/'; \
	echo 'export HOME'; \
	echo 'cd ${DebCache} && aptitude $$@'; \
	} > $@

pre: ExtCmd = apt-get strace dpkg gzip aptitude sh
pre: aptitude.sh
	for i in ${ExtCmd}; \
	do f=`which $$i`; echo $$f; ldd $$f | cut -f2 -d'>' | cut -f2 -d' '; \
	done | sort | uniq | xargs -ri cp -pu --parents {} .
	mkdir -p var/lib/apt/lists/partial var/lib/dpkg ${DebCache}/partial
	cp -pur --parents /etc/apt /usr/lib/apt \
		/etc/resolv.conf /etc/nsswitch.conf \
		/lib/libnss_dns* /lib/libresolv* \
		/usr/share/aptitude/aptitude-defaults \
		.
	{ \
	echo deb ${DebServer}/debian ${DebVer} ${DebComp}; \
	echo deb ${DebServer}/debian ${DebVer}-proposed-updates ${DebComp}; \
	echo deb ftp://security.debian.org/debian-security ${DebVer}/updates ${DebComp}; \
	echo deb ${DebServer}/debian ${DebVer} main/debian-installer; \
	} > etc/apt/sources.list
	{ \
	egrep -vw 'grep-(status|available)' /etc/grep-dctrl.rc; \
	echo grep-status ${DebPkgDir}/var/lib/dpkg/status; \
	echo grep-available ${DebPkgDir}/var/lib/dpkg/available; \
	} > etc/grep-dctrl.rc
	i=var/lib/dpkg/status; test -e $$i || touch $$i

update: pre
	mkdir -p proc
	sudo mount -t proc none ./proc
	sudo chroot . apt-get -q update
	sudo umount ./proc
	sudo chroot . dpkg --clear-avail
	for i in var/lib/apt/lists/*_Packages; \
	do sudo chroot . dpkg --merge-avail $$i; done

tmp/devices.tar.gz tmp/${DebVer}: pkg=debootstrap
tmp/devices.tar.gz tmp/${DebVer}: dir=usr/lib/debootstrap
tmp/devices.tar.gz tmp/${DebVer}:
	sudo chroot . apt-get -qqyd install ${pkg}
	ls -1t ${DebCache}/${pkg}_*.deb | head -1 | xargs -i dpkg -x {} tmp/
	mv tmp/${dir}/devices.tar.gz tmp/
	cp $(addprefix tmp/${dir}/, scripts/${DebVer} functions) tmp/
	${RM} -r tmp/usr

ifeq (${DebVer}, sarge)
tmp/${DebVer}.sh: tmp/${DebVer}
	sed -ne '/^work_out_debs/,/^}/p' $< > $@

tmp/pkg.0req: tmp/${DebVer}.sh
	ARCH=i386 . $< && work_out_debs && \
	echo $$required | tr ' ' '\n' | grep -v '^$$' | sort | uniq > $@

tmp/pkg.0base: tmp/${DebVer}.sh tmp/pkg.0req
	ARCH=i386 . tmp/${DebVer}.sh && work_out_debs && \
	echo $$base | tr ' ' '\n' | grep -v '^$$' | sort | uniq | \
	fgrep -v -f tmp/pkg.0req > $@
endif

ifeq (${DebVer}, etch)
tmp/pkg.0req: tmp/devices.tar.gz
	sudo chroot . sh aptitude.sh \
		search -F %p '~prequired!~sdebian-installer!~v' > $@

tmp/pkg.0base: tmp/pkg.0req
	sudo chroot . sh aptitude.sh \
		search -F %p '~pimportant!~sdebian-installer!~v' | \
	fgrep -v -f tmp/pkg.0req > $@
	echo update-inetd >> $@
endif

tmp/pkg.1dep:
	echo mdetect read-edid ${InstPkg} ${RetrPkg} | \
	tr ' ' '\n' | grep -v '^$$' | sort | uniq > $@

tmp/pkg.2retr: $(addprefix tmp/pkg., 0req 0base 1dep)
	{ \
	cat $^; \
	sudo chroot . sh aptitude.sh search -F %p '~pstandard!~sdebian-installer!~v'; \
	echo prebaseconfig; \
	} | tr -d ' ' | sort | uniq > $@

tmp/pkg.3all: tmp/pkg.2retr
	{ \
	cat $<; \
	cat $< | \
	xargs -r -n25 | \
	sed -e 's/^/^(/;s/ /|/g;s/$$/)$$/;s/+/\\\\+/g' | \
	while read i; \
	do grep-available --config-file=etc/grep-dctrl.rc \
		-P -e $$i -s Depends,Pre-Depends,Recommends,Suggests -n; done | \
	grep -v '^$$' | \
	sed -e 's/([^)]*)//g' | \
	tr -s ' ,|' '\n' | sort | uniq; \
	} | \
	xargs -r -n25 | \
	sed -e 's/^/^(/;s/ /|/g;s/$$/)$$/;s/+/\\\\+/g' | \
	while read i; \
	do grep-available --config-file=etc/grep-dctrl.rc \
		-P -e $$i -s Package -n; done | \
	grep -v '^$$' | sort | uniq > $@

retr_all: tmp/pkg.3all
	cat $< | xargs sudo chroot . sh aptitude.sh download

remove_old: retr_all
#	ls -vr ${i}_*.deb | tail +2
	cd ${DebCache} && ls -1 | sed -e 's/_.*.deb$$//' | sort | uniq -d | \
	while read i; \
	do ls -t $${i}_*.deb | tail +2; \
	done | xargs -r rm -v

retr: remove_old
