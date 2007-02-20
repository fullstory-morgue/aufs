
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

# $Id: local.mk,v 1.2 2007/01/08 01:59:05 sfjro Exp $

# call me from main Makefile.
# installs debian system to a local dir ${DLRoot}.
# simulating debian installer, extract and customize packages.

dpkg_dir = ${DLRoot}/var/lib/dpkg

all:
	false

clean:
	-sudo umount -n ${DLRoot}/proc
	sudo ${RM} -r ${DLRoot}
	test ! -d ${DLRoot}

ifiles: hosts=${DLRoot}/etc/hosts
ifiles: clean
	mkdir -p ${DLRoot}/sbin ${dpkg_dir}
	if test -d ifiles; \
	then tar -C ifiles -cf - --exclude=CVS . | { cd ${DLRoot} && tar -xf -; }; fi
	awk -v f="${DhcpClients}" ' \
	BEGIN {print "\n####", f, "####"} \
	/#/ || /^[[:space:]]*$$/ {next} \
	{print $$2, $$1}' ${DhcpClients} >> ${hosts}
	if test -f ${DLModTar};then tar -C ${DLRoot} -xpjf ${DLModTar}; fi
	cp -p ${MountAufs} ${Auplink} ${Aucpup} ${DLRoot}/sbin
	chmod 500 ${DLRoot}/sbin/$(notdir ${MountAufs})

status: s = ${dpkg_dir}/status
status: a = ${dpkg_dir}/available
status: dpkg_ver = $(shell \
	basename ${DebPkgDir}/var/cache/apt/archives/dpkg_*.deb | cut -f2 -d'_')
status: ifiles
	> ${a}
	touch ${s}
	echo >> ${s}
	echo Package: dpkg >> ${s}
	echo Version: ${dpkg_ver} >> ${s}
	echo Status: install ok installed >> ${s}

local: status
	sudo chown -R root.root ${DLRoot}
	test -d ${DebPkgInfoDir}
	sh ./local.sh ${DLRoot} ${DebPkgDir} ${DebPkgInfoDir}
