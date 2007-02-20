#!/bin/sh

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

# $Id: comount.sh,v 1.2 2007/01/08 01:58:27 sfjro Exp $

set -ex
NfsRW="192.168.1.1:/home/jro/tmp/rw"
NfsRO="192.168.1.1:/ext2/diskless"

DebVer=sarge
#DebVer=etch
case $HOSTNAME in
www.domain|hostA|jrous)
	service=http;;
mail.domain|hostB)
	service=smtp;;
ftp.domain|hostC)
	service=ftp;;
*)
	echo unknown host $HOSTNAME
	false
	;;
esac

portmap
sleep 2

for i in root common $service
do mount -n -t nfs -o ro,tcp,intr ${NfsRO}/$DebVer/$i /branch/$i
done
/bin/mount -n -t nfs -o rw,tcp,noatime,dirsync,intr ${NfsRW}/$HOSTNAME /branch/$HOSTNAME

killall portmap
sleep 2

br="/branch/${HOSTNAME}=rw:/branch/${service}=ro+wh:/branch/common=ro+wh:/branch/root=ro+wh"
mount -n -t aufs -o br:${br} aufsroot aufs
/bin/mount -n --bind /tmp /branch

d=/aufs/etc
mkdir -p $d
grep ' /aufs aufs ' /proc/mounts | sed -e 's:/aufs:/:' > $d/mtab
