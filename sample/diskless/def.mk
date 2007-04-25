
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

# $Id: def.mk,v 1.4 2007/04/23 00:59:51 sfjro Exp $

# kernel for diskless machines
KVer = 2.6.20.3jrousDL
#KVer = 2.6.20jrousDL
#KVer = 2.6.20-rc3jrousDL
#KVer = 2.6.19jrousDL
#KVer = 2.6.18jrousDL
DLKernel = ${CURDIR}/vmlinuz-${KVer}
DLModTar = ${CURDIR}/kmod-${KVer}.tar.bz2

# aufs
AufsKo = ${CURDIR}/../../aufs.ko
MountAufs = ${CURDIR}/../../util/mount.aufs
UmountAufs = ${CURDIR}/../../util/umount.aufs
Auplink = ${CURDIR}/../../util/auplink
Aulchown = ${CURDIR}/../../util/aulchown

########################################

# retrieve debian packages
DebVer = sarge
#DebVer = etch
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
# slax

NfsCdKoFull = $(addprefix lib/modules/${KVer}/kernel/, \
	drivers/block/loop.ko fs/isofs/isofs.ko fs/nls/nls_base.ko fs/nls/nls_iso8859-1.ko)

NfsSlax = /ext2/diskless/slax
SlaxUrl1 = http://merlin.fit.vutbr.cz/mirrors/slax/SLAX-6.x
SlaxUrl = ${SlaxUrl1}/testing/slax-6.0.0pre10.iso
SlaxKoFull = ${NfsCdKoFull}
SlaxKo = $(notdir ${SlaxKoFull}) unlzma.ko sqlzma.ko squashfs.ko

########################################
# knoppix

NfsKnoppix = /ext2/diskless/knoppix
KnoppixUrl = ftp://ftp.planetmirror.com/pub/knoppix/KNOPPIX_V5.1.1CD-2007-01-04-EN.iso
KnoppixKoFull = ${NfsCdKoFull}
KnoppixKo = $(notdir ${KnoppixKoFull}) cloop.ko

########################################
# gentoo

NfsGentoo = /ext2/diskless/gentoo
GentooUrl1 = ftp://gentoo.mirrors.tds.net/pub/gentoo/releases/x86/2006.1/livecd
GentooUrl = ${GentooUrl1}/livecd-i686-installer-2006.1.iso
GentooKoFull = ${NfsCdKoFull}
GentooKo = $(notdir ${GentooKoFull}) unlzma.ko sqlzma.ko squashfs.ko

########################################
# ubuntu edgy

NfsEdgy = /ext2/diskless/edgy
EdgyUrl = ftp://ftp.ecc.u-tokyo.ac.jp/UBUNTU-CDS/edgy/ubuntu-6.10-desktop-i386.iso
EdgyKoFull = ${NfsCdKoFull}
EdgyKo = $(notdir ${EdgyKoFull}) unlzma.ko sqlzma.ko squashfs.ko

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
BranchDirs += cdrom slax knoppix gentoo edgy
