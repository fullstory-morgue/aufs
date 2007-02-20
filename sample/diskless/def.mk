
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

# $Id: def.mk,v 1.2 2007/01/08 01:56:56 sfjro Exp $

# kernel for diskless machines
KVer = 2619jrousDL
#KVer = 2618jrousDL
DLKernel = ${CURDIR}/vmlinuz-${KVer}
DLModTar = ${CURDIR}/kmod-${KVer}.tar.bz2

# aufs
AufsKo = ${CURDIR}/../../aufs.ko
MountAufs = ${CURDIR}/../../util/mount.aufs
Auplink = ${CURDIR}/../../util/auplink
Aulchown = ${CURDIR}/../../util/aulchown

########################################

# retrieve debian packages
DebVer = sarge
DebServer = ftp://ftp.debian.org
DebComp = main

# extract pxelinux.0 from debian installer netboot.tar.gz
DebInstaller = ${DebServer}/debian/dists/${DebVer}/main/installer-i386

# where the packages are spooled
DebPkgDir = /ext2/diskless/${DebVer}/pkg
DebPkgInfoDir = ${CURDIR}/retr_info

# install debian system under local directory
DLRoot = /ext2/diskless/${DebVer}/root
DLRootRW = ${HOME}/tmp/rw

# uninstall them after local install
UninstPkg = ipchains nano ppp pppconfig pppoe pppoeconf
# slang1 slang1a-utf8

# install them additionally
DebugPkg = strace debconf-utils deborphan
InstPkg = ${DebugPkg} bzip2 cvs gawk lynx module-init-tools nfs-common portmap sudo

# retrieve them additionally
RetrPkg = emacs21 ssh

########################################

# seed of dhcpdDL.conf
DhcpdDLConfIn = dhcpdDL.conf.in
DhcpClients = clients

# tftp root dir
TftpDir = /var/lib/tftpboot/diskless

# actual mount script, called by linuxrc
Comount = comount.sh

########################################
# initrd

# extract busybox binary from debian pkg
BusyBoxPkg = busybox-cvs-static

# other commands to be included in initrd
InitrdExtCmd = bootpc insmod mount portmap

# dirs in initrd, just prepare them
BranchDirs = hostA hostB hostC http smtp ftp common root
