
# aufs sample -- diskless system

# Copyright (C) 2006 Junjiro Okajima
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

# $Id: Makefile,v 1.1 2006/12/22 06:08:56 sfjro Exp $

include def.mk
-include priv_def.mk

MAKE += --no-print-directory
export

TftpTgt = aufs.ko comount.sh initrd pxelinux.0 pxelinux.cfg/default vmlinuz
TftpTgt := $(addprefix ${TftpDir}/, ${TftpTgt})

all:
	@echo man -l diskless.6aufs or read diskless.html, edit def.mk,
	@echo and then run \"make update retr local dhcpdDL.conf tftp nfsdir\"

clean:
	${RM} pxelinux.0 busybox
	${RM} typescript dhcpdDL.conf initrd diskless.html
	${RM} -r ${DebPkgInfoDir} initrd.dir
	${RM} *~ .*~

real_clean: clean
	-${RM} ${BusyBoxPkg}_*.deb netboot.tar.gz

########################################
# retrieve packages

retr update: m = ${DebPkgDir}/Makefile
retr update: t = ${DebPkgDir}/tmp
retr update:
	mkdir -p ${DebPkgDir} ${DebPkgInfoDir}
	test -e ${m} || ln -s ${CURDIR}/retr.mk ${m}
	test -e ${t} || ln -s ${DebPkgInfoDir} ${t}
	${MAKE} -C ${DebPkgDir} $@
	${RM} ${m} ${t}

########################################
# local install
local: ${DhcpClients}
	script -c "${MAKE} -f local.mk $@"

########################################

dhcpdDL.conf: f = /etc/dhcp3/dhcpd.conf
dhcpdDL.conf: ${DhcpdConfIn} ${DhcpClients}
	cp ${DhcpdConfIn} $@
	awk -v f="${DhcpClients}" ' \
	BEGIN {print "\n####", f, "####"} \
	/#/ || /^[[:space:]]*$$/ {next} \
	{ \
		print "host", $$1, "{"; \
		print "\toption host-name \"" $$1 "\";"; \
		print "\tfixed-address", $$2 ";"; \
		print "\thardware ethernet", $$3 ";"; \
		print "}"; \
	}' ${DhcpClients} >> $@
	@if [ -f ${f} ]; \
	then \
		grep -q "${CURDIR}/$@" ${f} || \
		echo edit your ${f}, and include ${CURDIR}/$@, \
			then restart your dhcp server.; \
	fi

########################################

initrd busybox initrd.dir:
	${MAKE} -f initrd.mk $@

########################################

netboot.tar.gz:
	wget -q ${DebInstaller}/current/images/netboot/$@
pxelinux.0: d=debian-installer/i386
pxelinux.0: netboot.tar.gz
	tar -C /tmp -xpzf $< ./${d}/$@
	mv /tmp/${d}/$@ .

${TftpDir}/%: %
	cp -pu $< $@
${TftpDir}/vmlinuz: ${DLKernel}
	cp -pu $< $@
${TftpDir}/pxelinux.cfg/default: pxe.conf
	cp -pu $< $@
${TftpDir}/comount.sh: ${Comount}
	cp -pu $< $@
tftp: ${TftpTgt}

########################################

common = $(addprefix $(dir ${DLRoot}), common)
nfsdir:
	sudo mkdir -p $(addprefix ${DLRootRW}/, hostA hostB hostC)
	sudo mkdir -p $(addprefix $(dir ${DLRoot}), http) ${common}/usr/bin
wh = $(addprefix ${common}/usr/bin/.wh., dpkg apt-get aptitude)
wh: nfsdir ${common}/.wh..wh.aufs ${wh}
${common}/.wh..wh.aufs:
	> $@
${wh}: ${common}/.wh..wh.aufs
	ln $< $@

########################################

diskless.html: diskless.6aufs
#	man2html $< > $@
	man -l -Thtml $< > $@

-include priv.mk
